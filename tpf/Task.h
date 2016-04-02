#pragma once
#include <atomic>
#include "Ct.h"

namespace Parallel {
	class Task {
	public:
		Task() : _isRecyclable(false), _pendingCount(0), _continuation(nullptr), _cancellationToken(nullptr) {
		}

		virtual ~Task() {
		}

		virtual Task* Compute() = 0;

		Task* Continuation(std::memory_order memoryOrder = std::memory_order_acquire) {
			return _continuation.load(memoryOrder);
		}

		void Continuation(Task* task, std::memory_order memoryOrder = std::memory_order_release) {
			_continuation.store(task, memoryOrder);
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

		bool IsRecyclable(std::memory_order memoryOrder = std::memory_order_acquire) {
			return _isRecyclable.load(memoryOrder);
		}

		void IsRecyclable(bool value, std::memory_order memoryOrder = std::memory_order_release) {
			_isRecyclable.store(value, memoryOrder);
		}

		bool IsCanceled() {
			Ct* token = _cancellationToken.load(std::memory_order_acquire);

			if (token == nullptr) {
				return false;
			}

			return token->IsCanceled();
		}

		void Cancel() {
			Ct* token = _cancellationToken.load(std::memory_order_acquire);
			if (token != nullptr) {
				token->Cancel();
			}
		}

		void CancellationToken(Ct* token, std::memory_order memoryOrder = std::memory_order_release) {
			_cancellationToken.store(token, memoryOrder);
		}

		Ct* CancellationToken(std::memory_order memoryOrder = std::memory_order_acquire) {
			return _cancellationToken.load(memoryOrder);
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
		std::atomic<bool> _isRecyclable;
		std::atomic<int> _pendingCount;
		std::atomic<Task*> _continuation;
		std::atomic<Ct*> _cancellationToken;
	};
}