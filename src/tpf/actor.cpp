#include <tpf/actor.h>
#include <tpf/spawn_and_wait.h>

namespace tpf {
	namespace parallel {
		actor::actor(int size) : queue(new ConcurrentQueue<void *>(size)) {
		}

		task *actor::compute() {
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

		void actor::Start() {
			if (!IsCanceled() && CanSpawn()) {
				Parallel::Spawn(this);
			}
		}

		ConcurrentQueue<void *> &actor::Queue() {
			return *_queue;
		}

		ActorStatus actor::Status(std::memory_order memoryOrder) {
			return _status.load(memoryOrder);
		}

		void actor::Send(std::string endPoint, void *message, bool isSpawnReceiver) {
			Actor *actor = Actors[endPoint];
			if (!actor->_queue->TryEnqueue(message)) {
				throw std::exception("Queue is full.");
			}

			if (isSpawnReceiver) {
				actor->Start();
			}
		}

		void *actor::Receive() {
			void *value;
			return _queue->TryDequeue(value) ? value : nullptr;
		}

		bool actor::CanSpawn() {
			return _status.exchange(ActorStatus::Start, std::memory_order_release) == ActorStatus::Wait;
		}

		bool actor::SwitchStatus(ActorStatus from, ActorStatus to) {
			ActorStatus status = _status.load(std::memory_order_acquire);
			return status == from &&
				   _status.compare_exchange_strong(status, to, std::memory_order_release, std::memory_order_acquire);
		}
	}
}