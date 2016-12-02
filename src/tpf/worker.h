#pragma once
#include <thread>
#include "Sync.h"

namespace tpf {
	class worker {
	public:
		explicit worker(sync* sync) : sync_(sync) {
			_thread = std::thread([&]() {
				_sync->Wait();
				Execute();
			});

			thread_id_ = thread_.get_id();
		}

		virtual ~worker() {
		}

		std::thread::id thread_id() {
			return thread_id_;
		}

		virtual void execute() = 0;

	protected:
		sync* sync_;
		std::thread thread_;
		std::thread::id thread_id_;
	};
}