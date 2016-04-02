#pragma once
#include <atomic>
#include "SpinWait.h"

namespace Parallel {
	template<typename T>
	class ConcurrentQueue {
	public:
		ConcurrentQueue(int size) : _size(size), _tail(0), _head(0), _buffer(new std::atomic<T>[size]), _commits(new std::atomic<bool>[size]) {
			for (int i = 0; i < size; ++i) {
				_buffer[i].store(nullptr, std::memory_order_relaxed);
				_commits[i].store(false, std::memory_order_relaxed);
			}
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
					while (_commits[tail].load(std::memory_order_acquire))
						spinWait2.SpinOnce();

					_buffer[tail].store(value, std::memory_order_relaxed);
					_commits[tail].store(true, std::memory_order_release);
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
				while (!_commits[head].load(std::memory_order_acquire))
					spinWait2.SpinOnce();

				value = _buffer[head].load(std::memory_order_relaxed);
				_commits[head].store(false, std::memory_order_release);
				return true;
			}
		}

	private:
		int _size;
		std::atomic<int> _tail;
		std::atomic<int> _head;
		std::atomic<T>* _buffer;
		std::atomic<bool>* _commits;
	};
}