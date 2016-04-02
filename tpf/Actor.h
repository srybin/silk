#pragma once
#include "Task.h"
#include "Scheduler.h"
#include "ConcurrentQueue.h"

namespace Parallel {
	enum class ActorStatus { Wait, Start, Run };

	class Actor : public Task {
	public:
		Actor(int size = 1024);

		Task* Compute() override;

		virtual void Start();

		ConcurrentQueue<void*>& Queue();

		ActorStatus Status(std::memory_order memoryOrder = std::memory_order_acquire);

	protected:
		virtual void Run() = 0;

		void Send(std::string endPoint, void* message, bool isSpawnReceiver = false);

		void* Receive();

		bool CanSpawn();

	private:
		bool SwitchStatus(ActorStatus from, ActorStatus to);

		ConcurrentQueue<void*>* _queue;
		std::atomic<ActorStatus> _status{ ActorStatus::Wait };
	};

	static std::unordered_map<std::string, Actor*> Actors;
}