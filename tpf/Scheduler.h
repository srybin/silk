#pragma once
#include <mutex>
#include <unordered_map>
#include "Sync.h"
#include "Task.h"
#include "IO.h"
#include "Win32IO.h"
#include "CpuWorker.h"
#include "IoWorker.h"
#include "QueuesContainer.h"

namespace Parallel {
	class Scheduler {
	public:
		Scheduler();

		static Scheduler* Instance() {
			if (_scheduler == nullptr) {
				std::lock_guard<std::mutex> locker(_lock);

				if (_scheduler == nullptr) {
					_scheduler = new Scheduler();
				}
			}

			return _scheduler;
		}

		void Spawn(Task* task);

		void Spawn(Task* task, std::thread::id threadId);

		void Wait(Task* task);

		Task* FetchTaskFromLocalQueues(std::thread::id currentThreadId);

		Task* StealTaskFromInternalQueueOtherWorkers(std::thread::id currentThreadId);

		Task* StealTaskFromExternalQueueOtherWorkers(std::thread::id currentThreadId);

		Task* StealTaskFromOtherWorkers(std::function<ConcurrentQueue<Task*>*(std::thread::id)> queue, std::thread::id currentThreadId);

		void Compute(Task* task);

		void EnqueueInIoQueue(void* io, Task* continuation);

		Task* FetchCurrentTask(std::thread::id currentThreadId);

	private:
		int _cores;
		IoQueue* _ioQueue;
		Sync* _syncForWorkers;
		static std::mutex _lock;
		static Scheduler* _scheduler;
		std::vector<IoWorker*> _ioWorkers;
		std::vector<CpuWorker*> _cpuWorkers;
		std::atomic<int> _currentWorker = -1;
		std::unordered_map<std::thread::id, Task*> _currentTasks;
		std::unordered_map<std::thread::id, QueuesContainer*> _queues;
	};
}