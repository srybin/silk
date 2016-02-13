#pragma once
#include "Sync.h"
#include "Task.h"

namespace Parallel {
	class SyncTask : public Task {
	public:
		SyncTask(Sync* sync) : _sync(sync) {
		}

		Task* Compute() override {
			_sync->NotifyAll();
			return nullptr;
		}

	private:
		Sync* _sync;
	};
}