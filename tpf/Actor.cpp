#include "Actor.h"

using namespace Parallel;

Task* Actor::Compute() {
	while (true) {
		while (true) {
			if (SwitchStatus(ActorStatus::Start, ActorStatus::Run) && !IsCanceled()) {
				Run();
			} else {
				break;
			}
		}

		if (IsCanceled()) {
			_status.store(ActorStatus::Wait, std::memory_order_release);
			break;
		}

		if (SwitchStatus(ActorStatus::Run, ActorStatus::Wait)) {
			break;
		}
	}

	Recycle();
	return nullptr;
}

void Actor::Start() {
	if (!IsCanceled() && CanSpawn()) {
		Scheduler::Instance()->Spawn(this);
	}
}

ConcurrentQueue<void*>& Actor::Queue() {
	return _queue;
}

ActorStatus Actor::Status(std::memory_order memoryOrder) {
	return _status.load(memoryOrder);
}

void Actor::Send(std::string endPoint, void* message, bool isSpawnReceiver) {
	Actor* actor = Actors[endPoint];
	actor->_queue.Enqueue(message);

	if (isSpawnReceiver) {
		actor->Start();
	}
}

void* Actor::Receive() {
	void* value;
	return _queue.TryDequeue(value) ? value : nullptr;
}

bool Actor::CanSpawn() {
	return _status.exchange(ActorStatus::Start, std::memory_order_release) == ActorStatus::Wait;
}

bool Actor::SwitchStatus(ActorStatus from, ActorStatus to) {
	ActorStatus status = _status.load(std::memory_order_acquire);
	return status == from && _status.compare_exchange_strong(status, to, std::memory_order_release, std::memory_order_acquire);
}