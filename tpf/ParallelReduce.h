#pragma once
#include "Task.h"
#include "Scheduler.h"
#include "RangeHelpers.h"
#include "Ct.h"

namespace Parallel {
	template<typename Body>
	class ParallelReduceContinuationTask : public Task {
	public:
		ParallelReduceContinuationTask(Body& body, Body& splitBody) : _body(body), _splitBody(splitBody) {
		}

		virtual ~ParallelReduceContinuationTask() {
			delete _splitBody;
		}

		Task* Compute() override {
			_body.Join(_splitBody);
			return nullptr;
		}

	private:
		Body& _body;
		Body& _splitBody;
	};

	template<typename Index, typename Body>
	class ParallelReduceTask : public Task {
	public:
		ParallelReduceTask(Index first, Index last, Index step, Body& body)
			: _first(first), _last(last), _step(step), _body(body) {
		}

		Task* Compute() override {
			if ((_last - _first) + 1 <= _step && !IsLastSplitRangeForStepOne(_first, _last, _step)) {
				for (Index i = _first; i <= _last && !IsCanceled(); ++i) {
					_body(i);
				}

				return nullptr;
			} else {
				Body& splitBody = *new Body(_body);
				Task* c = new ParallelReduceContinuationTask<Body>(_body, splitBody);
				AsContinuation(c);
				Ct* token = this->CancellationToken();
				c->CancellationToken(token);
				Index midRange = FetchMidRangeFor(_first, _last, _step);
				Task* t = new ParallelReduceTask(midRange + 1, _last, _step, splitBody);
				t->CancellationToken(token);
				t->Continuation(c);
				_last = midRange;
				RecycleAsChildOf(c);
				c->PendingCount(2);
				Spawn(t);
				return this;
			}
		}

	private:
		Index _first;
		Index _last;
		Index _step;
		Body& _body;
	};

	template<typename Index, typename Body>
	void Reduce(Index first, Index last, Body& body, Ct* token = nullptr) {
		Reduce(first, last, 0, body, token);
	}

	template<typename Index, typename Body>
	void Reduce(Index first, Index last, Index step, Body& body, Ct* token = nullptr) {
		Index s = step == 0 ? ceil((float)FetchRangeLength(first, last) / (float)std::thread::hardware_concurrency()) : step;
		Task* task = new ParallelReduceTask<Index, Body>(first, last, s, body);
		task->CancellationToken(token);
		Scheduler::Instance()->Wait(task);
	}
}