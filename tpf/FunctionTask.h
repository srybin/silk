#pragma once
#include "Task.h"
#include <functional>

namespace Parallel {
	class FunctionTask : public Task {
	public:
		FunctionTask(std::function<void()> func, Ct* token) : _func(func) {
			CancellationToken(token);
		}

		Task* Compute() override {
			_func();
			return nullptr;
		}

	private:
		std::function<void()> _func;
	};
}