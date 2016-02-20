#pragma once
#include <atomic>
#include <thread>
#include <vector>

namespace Parallel {
	class HazardPointersUnit {
	public:
		HazardPointersUnit(int hazardPointersPerThread) : _head(nullptr), _hazardPointersPerThread(hazardPointersPerThread) {
			_threshhold = (std::thread::hardware_concurrency() * 2) * hazardPointersPerThread;
		}

		void Hazard(int index, void* pointer) {
			Get()->HazerdPointers[index].store(pointer, std::memory_order_relaxed);
		}

		void Unhazerd(int index) {
			Get()->HazerdPointers[index].store(nullptr, std::memory_order_relaxed);
		}

		void Retire(void * pointer) {
			Bucket* bucket = Get();
			bucket->RetiredPointers[bucket->RetiredPointersCount++] = pointer;

			if (bucket->RetiredPointersCount == _threshhold) {
				Scan(bucket);
			}
		}

	private:
		struct Bucket {
			void** RetiredPointers;
			std::thread::id ThreadId;
			int RetiredPointersCount;
			std::atomic<Bucket*> Next;
			std::atomic<void*>* HazerdPointers;
		};

		Bucket* Get() {
			std::thread::id id = std::this_thread::get_id();
			Bucket* bucket = _head.load(std::memory_order_acquire);

			while (bucket != nullptr) {
				if (bucket->ThreadId == id) {
					return bucket;
				}

				bucket = bucket->Next.load(std::memory_order_acquire);
			}

			bucket = new Bucket();
			bucket->ThreadId = id;
			bucket->RetiredPointers = new void*[_threshhold];
			bucket->HazerdPointers = new std::atomic<void*>[_hazardPointersPerThread];

			Bucket* head = _head.load(std::memory_order_acquire);

			do {
				bucket->Next.store(head, std::memory_order_relaxed);
			} while (!_head.compare_exchange_weak(head, bucket, std::memory_order_release, std::memory_order_acquire));

			return bucket;
		}

		void Scan(Bucket* bucket) {
			std::vector<void*> hazardPointers;
			Bucket* b = _head.load(std::memory_order_acquire);
			while (b != nullptr) {
				if (b->ThreadId != bucket->ThreadId) {
					for (int i = 0; i < _hazardPointersPerThread; i++) {
						void* p = bucket->HazerdPointers[i].load(std::memory_order_acquire);
						if (p != nullptr) {
							hazardPointers.push_back(p);
						}
					}
				}

				b = b->Next.load(std::memory_order_acquire);
			}

			int newRetiredPointersCount = 0;
			void** newRetiredPointers = new void*[_threshhold];

			int max = _threshhold / 2;
			for (int i = 0; i < max; ++i) {
				if (std::find(hazardPointers.begin(), hazardPointers.end(), bucket->RetiredPointers[i]) != hazardPointers.end()) {
					newRetiredPointers[newRetiredPointersCount++] = bucket->RetiredPointers[i];
				} else {
					delete bucket->RetiredPointers[i];
				}
			}

			for (int i = _threshhold / 2; i < _threshhold; ++i) {
				newRetiredPointers[newRetiredPointersCount++] = bucket->RetiredPointers[i];
			}

			delete bucket->RetiredPointers;
			bucket->RetiredPointers = newRetiredPointers;
			bucket->RetiredPointersCount = newRetiredPointersCount;
		}

		int _threshhold;
		std::atomic<Bucket*> _head;
		int _hazardPointersPerThread;
	};
}