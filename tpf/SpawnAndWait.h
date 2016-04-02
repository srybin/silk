#pragma once
#include "Scheduler.h"
#include "FunctionTask.h"

namespace Parallel {
	inline void Spawn(Task* task) {
		Scheduler::Instance()->Spawn(task);
	}

	inline void Spawn(std::function<void()> func, Ct* token = nullptr) {
		Spawn(new FunctionTask(func, token));
	}

	inline void InitializeScheduler(int queuesSize) {
		Scheduler::Instance(queuesSize);
	}

	inline void Wait(Task* task) {
		Scheduler::Instance()->Wait(task);
	}
}