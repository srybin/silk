#pragma once
#include <atomic>
#include "SpinWait.h"
#include <assert.h>

namespace Parallel {
	template<typename T>
	class ConcurrentQueue {
	public:
		ConcurrentQueue(int size) : _size(size), _tail(0), _head(0) {
			assert( size > 0 );
			_buffer = new AtomicBufferWithIndicator[size];
		}

		~ConcurrentQueue() {
			delete[] _buffer;
		}

		bool TryEnqueue(T value) {
			SpinWait spinWait;
			while (true) {
				int tail = _tail.load(std::memory_order_acquire);
				int nextTail = (tail + 1) % _size;

				if (nextTail != _head.load(std::memory_order_acquire)) {
					if (!_tail.compare_exchange_strong(tail, nextTail, std::memory_order_release)) {
						spinWait.SpinOnce();
						continue;
					}

					SpinWait spinWait2;
					AtomicBufferWithIndicator& buffer = _buffer[tail];
					while (buffer.isCommited.load(std::memory_order_acquire))
						spinWait2.SpinOnce();

					buffer.value = value;
					buffer.isCommited.store(true, std::memory_order_release);
					return true;
				}

				return false;
			}
		}

		bool TryDequeue(T& value) {
			SpinWait spinWait;
			while (true) {
				int head = _head.load(std::memory_order_acquire);
				if (head == _tail.load(std::memory_order_acquire)) {
					return false;
				}

				if (!_head.compare_exchange_strong(head, (head + 1) % _size, std::memory_order_release)) {
					spinWait.SpinOnce();
					continue;
				}

				SpinWait spinWait2;
				AtomicBufferWithIndicator& buffer = _buffer[head];
				while (!buffer.isCommited.load(std::memory_order_acquire))
					spinWait2.SpinOnce();

				value = buffer.value;
				buffer.isCommited.store(false, std::memory_order_release);

				return true;
			}
		}

	private:
		struct AtomicBufferWithIndicator
		{
			T value;
			std::atomic<bool> isCommited;
		};

		int _size;
		std::atomic<int> _tail;
		std::atomic<int> _head;
		AtomicBufferWithIndicator* _buffer;
	};
}