#pragma once
#include "Sync.h"
#include "Task.h"

namespace tpf {
	namespace tasks {
		class sync_task : public task {
		public:
			sync_task(sync *sync) : sync_(sync) {
			}

			task *compute() override {
				sync_->notify_all();
				return nullptr;
			}

		private:
			sync *sync_;
		};
	}
}