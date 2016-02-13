#pragma once
#include "Task.h"
#include "ConcurrentQueue.h"

namespace Parallel {
	struct QueuesContainer {
		ConcurrentQueue<Task*> InternalQueue;
		ConcurrentQueue<Task*> ExternalQueue;
	};
}