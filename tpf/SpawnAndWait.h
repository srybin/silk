#pragma once
#include "Scheduler.h"

namespace Parallel {
	inline void Spawn(Task* task) {
		Scheduler::Instance()->Spawn(task);
	}

	inline void Wait(Task* task) {
		Scheduler::Instance()->Wait(task);
	}
}