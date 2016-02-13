#pragma once
#include "Task.h"
#include "Scheduler.h"
#include "RangeHelpers.h"
#include "Ct.h"

namespace Parallel {
	template<typename Index, typename Func>
	class ParallelForTask : public Task {
	public:
		ParallelForTask(Index first, Index last, Index step, const Func& func)
			: _first(first), _last(last), _step(step), _func(func) {
		}

		Task* Compute() override {
			if ((_last - _first) + 1 <= _step && !IsLastSplitRangeForStepOne(_first, _last, _step)) {
				for (Index i = _first; i <= _last && !IsCanceled(); ++i) {
					_func(i);
				}

				return nullptr;
			} else {
				Index midRange = FetchMidRangeFor(_first, _last, _step);
				Task* t = new ParallelForTask(midRange + 1, _last, _step, _func);
				t->CancellationToken(this->CancellationToken());
				Task* c = this->Continuation();
				c->IncrementPendingCount();
				t->Continuation(c);
				_last = midRange;
				Recycle();
				Spawn(t);
				return this;
			}
		}

	private:
		Index _first;
		Index _last;
		Index _step;
		const Func& _func;
	};

	template<typename Index, typename Func>
	void For(Index first, Index last, const Func& func, Ct* token = nullptr) {
		For(first, last, 0, func, token);
	}

	template<typename Index, typename Func>
	void For(Index first, Index last, Index step, const Func& func, Ct* token = nullptr) {
		Index s = step == 0 ? ceil((float)FetchRangeLength(first, last) / (float)std::thread::hardware_concurrency()) : step;
		Task* task = new ParallelForTask<Index, Func>(first, last, s, func);
		task->CancellationToken(token);
		Scheduler::Instance()->Wait(task);
	}
}