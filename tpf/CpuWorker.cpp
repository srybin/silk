#include "Scheduler.h"
#include "CpuWorker.h"
#include "Sync.h"

using namespace Parallel;

CpuWorker::CpuWorker(Scheduler* scheduler, Sync* sync) : Worker(sync), _scheduler(scheduler) {
}

void CpuWorker::Execute() {
	while (true) {
		Task* task = _scheduler->FetchTaskFromLocalQueues(std::this_thread::get_id());

		if (task == nullptr) {
			task = _scheduler->StealTaskFromInternalQueueOtherWorkers(std::this_thread::get_id());
		}

		if (task == nullptr) {
			task = _scheduler->StealTaskFromExternalQueueOtherWorkers(std::this_thread::get_id());
		}

		if (task != nullptr) {
			_scheduler->Compute(task);
		} else {
			task = _scheduler->FetchTaskFromLocalQueues(std::this_thread::get_id());

			if (task != nullptr) {
				_scheduler->Compute(task);
			} else {
				_sync->Wait();
			}
		}
	}
}