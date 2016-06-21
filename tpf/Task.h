#pragma once
#include <atomic>
#include "Ct.h"

namespace Parallel {
	class Task {
	public:
		Task() : _continuation(nullptr), _cancellationToken(nullptr), _pendingCount(0) {
		}

		virtual ~Task() {
		}

		virtual Task* Compute() = 0;

		Task* Continuation() {
			return _continuation;
		}

		void Continuation(Task* task) {
			_continuation = task;
		}

		void PendingCount(int count, std::memory_order memoryOrder = std::memory_order_release) {
			_pendingCount.store(count, memoryOrder);
		}

		int PendingCount(std::memory_order memoryOrder = std::memory_order_acquire) {
			return _pendingCount.load(memoryOrder);
		}

		int DecrementPendingCount(std::memory_order memoryOrder = std::memory_order_acquire) {
			return _pendingCount.fetch_sub(1, memoryOrder) - 1;
		}

		int IncrementPendingCount(std::memory_order memoryOrder = std::memory_order_acquire) {
			return _pendingCount.fetch_add(1, memoryOrder) + 1;
		}

		bool IsCanceled(std::memory_order memoryOrder = std::memory_order_acquire) {
			Ct* token = _cancellationToken;

			if (token == nullptr) {
				return false;
			}

			return token->IsCanceled(memoryOrder);
		}

		void Cancel(std::memory_order memoryOrder = std::memory_order_release) {
			Ct* token = _cancellationToken;
			if (token != nullptr) {
				token->Cancel(memoryOrder);
			}
		}

		void CancellationToken(Ct* token) {
			_cancellationToken = token;
		}

		Ct* CancellationToken() {
			return _cancellationToken;
		}

		static Task* Self();

	protected:
		void Spawn(Task* task);

		void Recycle();

		void AsContinuation(Task* task) {
			task->Continuation(this->Continuation());
			this->Continuation(nullptr);
		}

		void RecycleAsChildOf(Task* task) {
			this->Continuation(task);
			Recycle();
		}

	private:
		Task* _continuation;
		Ct* _cancellationToken;
		std::atomic<int> _pendingCount;
	};
}