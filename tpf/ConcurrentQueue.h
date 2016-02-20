#pragma once
#include <atomic>
#include <algorithm>
#include "SpinWait.h"
#include "HazardPointersUnit.h"

namespace Parallel {
	template<typename T>
	class ConcurrentQueue {
	public:
		ConcurrentQueue() : _tail(new Segment()), _head(_tail.load(std::memory_order_relaxed)), _hazardPointersUnit(new HazardPointersUnit(2)) {
		}

		void Enqueue(T value) {
			while (true) {
				SpinWait spinWait;

				Segment* tail = _tail.load(std::memory_order_acquire);
				_hazardPointersUnit->Hazard(0, tail);
				if (tail != _tail.load(std::memory_order_acquire)) {
					_hazardPointersUnit->Unhazerd(0);
					continue;
				}

				if (tail->High >= 31) {
					_hazardPointersUnit->Unhazerd(0);
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

					if (i <= 31) {
						_hazardPointersUnit->Unhazerd(0);
						break;
					}
				}
			}
		}

		bool TryDequeue(T& value) {
			while (true) {
				Segment* head = _head.load(std::memory_order_acquire);
				_hazardPointersUnit->Hazard(0, head);
				if (head != _head.load(std::memory_order_acquire)) {
					_hazardPointersUnit->Unhazerd(0);
					continue;
				}

				int low = (std::min)(head->Low.load(std::memory_order_relaxed), 32);
				int high = (std::min)(head->High.load(std::memory_order_acquire), 31);

				Segment* next = head->Next.load(std::memory_order_acquire);
				_hazardPointersUnit->Hazard(1, next);
				if (next != head->Next.load(std::memory_order_acquire)) {
					_hazardPointersUnit->Unhazerd(0);
					_hazardPointersUnit->Unhazerd(1);
					continue;
				}

				if (low > high && next == nullptr) {
					_hazardPointersUnit->Unhazerd(0);
					_hazardPointersUnit->Unhazerd(1);
					return false;
				}

				_hazardPointersUnit->Unhazerd(1);

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
							do {
								newSegment = head->Next.load(std::memory_order_acquire);;
								_hazardPointersUnit->Hazard(1, newSegment);
								if (newSegment != head->Next.load(std::memory_order_acquire)) {
									_hazardPointersUnit->Unhazerd(1);
								}

								if (newSegment == nullptr) {
									spinWait3.SpinOnce();
								} else {
									break;
								}
							} while (true);

							_head.store(newSegment, std::memory_order_release);
							_hazardPointersUnit->Unhazerd(0);
							_hazardPointersUnit->Unhazerd(1);
							_hazardPointersUnit->Retire(head);
						}

						_hazardPointersUnit->Unhazerd(0);

						return true;
					}
				}

				_hazardPointersUnit->Unhazerd(0);
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
		HazardPointersUnit* _hazardPointersUnit;
	};
}