#pragma once
#include <thread>
#include "Sync.h"

namespace Parallel {
	class Worker {
	public:
		explicit Worker(Sync* sync) : _sync(sync) {
			_thread = std::thread([&]() {
				_sync->Wait();
				Execute();
			});

			_threadId = _thread.get_id();
		}

		virtual ~Worker() {
		}

		std::thread::id ThreadId() {
			return _threadId;
		}

		virtual void Execute() = 0;

	protected:
		Sync* _sync;
		std::thread _thread;
		std::thread::id _threadId;
	};
}