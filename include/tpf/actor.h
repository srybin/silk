#pragma once
#include <tpf/task.h>
#include <tpf/concurrent_queue.h>

namespace tpf {
	namespace parallel {
		enum class actor_status {
			wait, start, run
		};

		class actor : public task {
		public:
			actor(int size = 1024);

			task *compute() override;

			virtual void start();

			concurrent_queue<void *> &queue();

			actor_status status(std::memory_order memory_order = std::memory_order_acquire);

		protected:
			virtual void run() = 0;

			void send(std::string endPoint, void *message, bool isSpawnReceiver = false);

			void *receive();

			bool can_spawn();

		private:
			bool SwitchStatus(ActorStatus from, ActorStatus to);

			ConcurrentQueue<void *> *_queue;
			std::atomic <ActorStatus> _status{ActorStatus::Wait};
		};

		static std::unordered_map<std::string, Actor *> actors;
	}
}