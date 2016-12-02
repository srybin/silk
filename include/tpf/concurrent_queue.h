#pragma once

#include <tpf/spin_wait.h>

namespace tpf {
    template<typename T>
    class concurrent_queue {
    public:
        concurrent_queue(int size) : size_(size), tail_(0), head_(0) {
            assert( size > 0 );
            buffer_ = new AtomicBufferWithIndicator[size];
        }

        ~concurrent_queue() {
            delete[] buffer_;
        }

        bool try_enqueue(T value) {
            spin_wait enqueue_wait;
            while (true) {
                int tail = tail_.load(std::memory_order_acquire);
                int next_tail = (tail + 1) % size_;

                if (next_tail != _head.load(std::memory_order_acquire)) {
                    if (!tail_.compare_exchange_strong(tail, next_tail, std::memory_order_release)) {
                        enqueue_wait.spin_once();
                        continue;
                    }

                    spin_wait enqueue_wait_inner;
                    AtomicBufferWithIndicator& buffer = buffer_[tail];
                    while (buffer.is_commited.load(std::memory_order_acquire))
                        enqueue_wait_inner.spin_once();

                    buffer.value = value;
                    buffer.is_commited.store(true, std::memory_order_release);
                    return true;
                }

                return false;
            }
        }

        bool try_dequeue(T& value) {
            spin_wait dequeue_wait;
            while (true) {
                int head = head_.load(std::memory_order_acquire);
                if (head == tail_.load(std::memory_order_acquire)) {
                    return false;
                }

                if (!head_.compare_exchange_strong(head, (head + 1) % size_, std::memory_order_release)) {
                    dequeue_wait.spin_once();
                    continue;
                }

                spin_wait dequeue_wait_inner;
                AtomicBufferWithIndicator& buffer = buffer_[head];
                while (!buffer.is_commited.load(std::memory_order_acquire))
                    dequeue_wait_inner.spin_once();

                value = buffer.value;
                buffer.is_commited.store(false, std::memory_order_release);

                return true;
            }
        }

    private:
        struct AtomicBufferWithIndicator {
            T value;
            std::atomic<bool> is_commited = false;
        };

        int size_;
        std::atomic<int> tail_;
        std::atomic<int> head_;
        AtomicBufferWithIndicator* buffer_;
    };
}
