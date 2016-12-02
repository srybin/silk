#pragma once

#include <tpf/common.h>
#include <tpf/task_queue_iface.h>
#include <atomic>

namespace tpf {
    namespace task_queues {
        namespace details {
            struct locking_dequeue_cache_line_1 {
                /// Synchronization of access to task pool
                /// Also is used to specify if the slot is empty or locked
                std::atomic<task **> task_pool;

                /// Index of the first ready task in the deque.
                /// Modified by thieves, and by the owner during compaction/reallocation
                std::atomic<size_t> head;
            };

            struct locking_dequeue_cache_line_2 {
                /// Index of the element following the last ready task in the deque.
                /// Modified by the owner thread.
                std::atomic<size_t> tail;

                /// task pool of the scheduler that owns this slot
                std::atomic<task **> task_pool_ptr;
            };
        }

        class locking_queue : private cache_padding<details::locking_dequeue_cache_line_1>,
                              private cache_padding<details::locking_dequeue_cache_line_2>,
                              public task_queue_iface {
        public:
            locking_queue(int queue_size);
            virtual ~locking_queue() {};

            /// @brief Called on every task spawning
            virtual void enqueue(std::initializer_list<task&> tasks, bool external) final;

            /// @brief Called when origin thread tries to access this queue(e.g. loads task)
            virtual bool try_dequeue_optimistic(task*& t) final;

            /// @brief Called when foreign thread tries to access this queue(e.g. steals task)
            virtual bool try_dequeue_pessimistic(task*& t) final;

        private:
            /// @brief Gets vacant index inside pool to start adding tasks
            ///
            /// @param[in] tasks_count  task count to add into the pool
            /// @return starting index of pool to start addition of tasks
            ssize_t prepare_pool_for_tasks(size_t tasks_count);

            /// @brief Fixates the pool with new tail that is modified in enqueue_tasks
            ///        function
            ///
            /// @param[in] new_tail new tail of dequeue
            void publish_task_pool(size_t new_tail) {
                BOOST_ASSERT_MSG(new_tail <= task_pool_size,
                                 "task queue end was overwritten! possible programmer's fault?");

                /// Release fence is necessary to make sure that previously stored pointers are visible to thieves
                this->tail.store(new_tail, std::memory_order_release);
            }

            task **lock();

            void unlock(task **victim);

            void acquire();

            void release();

            template<std::memory_order order>
            bool is_empty_or_starved() const {
                auto head = this->head.load(order);
                auto tail = this->tail.load(order);
                auto ptr = this->task_pool.load(order);
                return head >= tail || ptr == const_cast<task **>(the_empty_pool);
            }

            template<std::memory_order order>
            bool is_quiescent() const {
                auto ptr = this->task_pool.load(order);
                return ptr == const_cast<task **>(the_empty_pool) || ptr == const_cast<task **>(the_locked_pool);
            }

            template<std::memory_order order>
            bool is_quiescent_empty() const {
                return is_quiescent<order>() && is_quiescent_empty_local<order>();
            }

            template<std::memory_order order>
            bool is_quiescent_reset() const {
                auto head = this->head.load(order);
                auto tail = this->tail.load(order);
                return is_quiescent<order>() && (head == 0 && tail == 0);
            }

            template<std::memory_order order>
            bool is_quiescent_empty_local() const {
                auto head = this->head.load(order);
                auto tail = this->tail.load(order);
                return head == tail;
            }

            template<std::memory_order order>
            bool is_non_empty_and_ready() const {
                auto head = this->head.load(order);
                auto tail = this->tail.load(order);
                auto ptr = this->task_pool.load(order);
                return ptr != const_cast<task **>(the_empty_pool) && head < tail;
            }

            constexpr size_t min_task_pool_size = 64;
            constexpr ssize_t no_space = -1;
            static const task **the_empty_pool;
            static const task **the_locked_pool;

            task **internal_pool;
        };
    }
}
