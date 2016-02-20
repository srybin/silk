#include "SyncTask.h"
#include "Scheduler.h"
#include "SyncBasedOnWindowsEvent.h"

using namespace Parallel;

std::mutex Scheduler::_lock;
Scheduler* Scheduler::_scheduler;

Scheduler::Scheduler()
	: _cores(std::thread::hardware_concurrency())
	, _ioQueue(new IoQueueBasedOnWindowsCompletionPorts())
	, _syncForWorkers(new SyncBasedOnWindowsEvent())
	, _ioWorkers(std::vector<IoWorker*>())
	, _cpuWorkers(std::vector<CpuWorker*>()) 
{
	for (int i = 0; i < _cores; i++) {
		QueuesContainer* queues = new QueuesContainer();

		CpuWorker* worker = new CpuWorker(this, _syncForWorkers);
		_queues.insert(make_pair(worker->ThreadId(), queues));
		_cpuWorkers.push_back(worker);
		_currentTasks.insert(make_pair(worker->ThreadId(), nullptr));
	}

	SyncBasedOnWindowsEvent* syncForIoWorkers = new SyncBasedOnWindowsEvent();
	_ioWorkers.push_back(new IoWorker(*_ioQueue, syncForIoWorkers));
	syncForIoWorkers->NotifyAll();
}

void Scheduler::Spawn(Task* task) {
	if (_currentWorker.load(std::memory_order_acquire) == _cores - 1) _currentWorker.store(0, std::memory_order_release);
	else ++_currentWorker;

	_queues[_cpuWorkers[_currentWorker]->ThreadId()]->ExternalQueue.Enqueue(task);
	_syncForWorkers->NotifyAll();
}

void Scheduler::Spawn(Task* task, std::thread::id threadId) {
	std::unordered_map<std::thread::id, QueuesContainer*>::const_iterator i = _queues.find(threadId);

	if (i == _queues.end()) {
		Spawn(task);
	} else {
		i->second->InternalQueue.Enqueue(task);
		_syncForWorkers->NotifyAll();
	}
}

void Scheduler::Wait(Task* task) {
	SyncBasedOnWindowsEvent sync;
	SyncTask* syncTask = new SyncTask(&sync);
	task->Continuation(syncTask);
	syncTask->PendingCount(1);
	Spawn(task);
	sync.Wait();
}

Task* Scheduler::FetchTaskFromLocalQueues(std::thread::id currentThreadId) {
	Task* task;
	if (_queues[currentThreadId]->InternalQueue.TryDequeue(task) || _queues[currentThreadId]->ExternalQueue.TryDequeue(task)) {
		return task;
	}
	return nullptr;
}

Task* Scheduler::StealTaskFromInternalQueueOtherWorkers(std::thread::id currentThreadId) {
	return StealTaskFromOtherWorkers([&](std::thread::id id) { return &_queues[id]->InternalQueue; }, currentThreadId);
}

Task* Scheduler::StealTaskFromExternalQueueOtherWorkers(std::thread::id currentThreadId) {
	return StealTaskFromOtherWorkers([&](std::thread::id id) { return &_queues[id]->ExternalQueue; }, currentThreadId);
}

Task* Scheduler::StealTaskFromOtherWorkers(std::function<ConcurrentQueue<Task*>*(std::thread::id)> queue, std::thread::id currentThreadId) {
	Task* task;
	for (std::unordered_map<std::thread::id, QueuesContainer*>::iterator it = _queues.begin(); it != _queues.end(); ++it) {
		if (it->first != currentThreadId) {
			if (queue(it->first)->TryDequeue(task)) {
				return task;
			}
		}
	}

	return nullptr;
}

void Scheduler::Compute(Task* task) {
	do {
		Task* continuation = nullptr;

		if (!task->IsCanceled()) {
			task->IsRecyclable(false);
			_currentTasks[std::this_thread::get_id()] = task;
			Task* c = task->Compute();

			Task* newC = nullptr;
			while (c != nullptr && !c->IsCanceled()) {
				c->IsRecyclable(false);
				_currentTasks[std::this_thread::get_id()] = task;
				newC = c->Compute();
				continuation = c->Continuation();

				if (newC == nullptr && continuation != nullptr) {
					task = c;
				} else if (c != newC) {
					delete c;
					c = nullptr;
				}

				c = newC;
			}

			if (!task->IsCanceled() && (task->PendingCount() != 0 || task->IsRecyclable())) {
				break;
			}
		}

		continuation = task->Continuation();
		delete task;
		task = continuation != nullptr && continuation->DecrementPendingCount() <= 0 ? continuation : nullptr;
	} while (task != nullptr);
}

Task* Scheduler::FetchCurrentTask(std::thread::id currentThreadId) {
	return _currentTasks[currentThreadId];
}

void Scheduler::EnqueueInIoQueue(void* io, Task* continuation) {
	_ioQueue->Enqueue(io, new CompletionContainer(continuation, std::this_thread::get_id()));
}

void Task::Spawn(Task* task) {
	Scheduler::Instance()->Spawn(task, std::this_thread::get_id());
}

Task* Task::Self() {
	return Scheduler::Instance()->FetchCurrentTask(std::this_thread::get_id());
}