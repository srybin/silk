#pragma once
#include <atomic>
#include <algorithm>
#include "SpinWait.h"

namespace Parallel {
	template<typename T>
	class ConcurrentQueue {
	public:
		ConcurrentQueue() : _tail(new Segment()), _head(_tail.load(std::memory_order_relaxed)) {
		}

		void Enqueue(T value) {
			while (true) {
				SpinWait spinWait;

				Segment* tail = _tail.load(std::memory_order_acquire);
				if (tail->High >= 31) {
					spinWait.SpinOnce();
				} else {
					int i = ++tail->High;

					if (i <= 31) {
						tail->Buffer[i].store(value, std::memory_order_relaxed);
						tail->Commits[i].store(true, std::memory_order_release);
					}

					if (i == 31) {
						tail->Next.store(new Segment(), std::memory_order_relaxed);
						_tail.store(tail->Next, std::memory_order_release);
					}

					if (i <= 31)
						break;
				}
			}
		}

		bool TryDequeue(T& value) {
			while (true) {
				Segment* head = _head.load(std::memory_order_acquire);

				int low = (std::min)(head->Low.load(std::memory_order_relaxed), 32);
				int high = (std::min)(head->High.load(std::memory_order_acquire), 31);

				if (low > high && head->Next.load(std::memory_order_relaxed) == nullptr)
					return false;

				SpinWait spinWait1;

				for (; low <= high; high = (std::min)(head->High.load(std::memory_order_relaxed), 31)) {
					if (!head->Low.compare_exchange_strong(low, low + 1, std::memory_order_release, std::memory_order_acquire)) {
						spinWait1.SpinOnce();

						low = (std::min)(head->Low.load(std::memory_order_relaxed), 32);
					} else {
						SpinWait spinWait2;

						while (!head->Commits[low].load(std::memory_order_acquire))
							spinWait2.SpinOnce();

						value = head->Buffer[low].load(std::memory_order_acquire);

						if (low + 1 >= 32) {
							SpinWait spinWait3;

							Segment* newSegment;
							while ((newSegment = head->Next.load(std::memory_order_relaxed)) == nullptr)
								spinWait3.SpinOnce();

							_head.store(newSegment, std::memory_order_release);
						}

						return true;
					}
				}
			}
		}

	private:
		struct Segment {
			std::atomic<int> Low;
			std::atomic<int> High = -1;
			std::atomic<T> Buffer[32];
			std::atomic<bool> Commits[32];
			std::atomic<Segment*> Next;
		};

		std::atomic<Segment*> _tail;
		std::atomic<Segment*> _head;
	};
}