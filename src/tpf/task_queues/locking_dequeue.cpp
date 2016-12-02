#include <tpf/task_queues/locking_dequeue.h>
#include <tpf/task.h>
#include <tpf/spin_wait.h>
#include <tpf/type_traits.h>

#include <initializer_list>

namespace tpf {
    namespace task_queues {
        const task **locking_queue::the_empty_pool = static_cast<const task **>(nullptr);
        const task **locking_queue::the_locked_pool = reinterpret_cast<const task **>(~static_cast<intptr_t>(0));

        locking_queue::locking_queue(int queue_size) {
            this->task_pool.store(nullptr, std::memory_order_relaxed);
            this->head.store(0, std::memory_order_relaxed);
            this->tail.store(0, std::memory_order_relaxed);


            this->task_pool_ptr.store(static_task_pool, std::memory_order_relaxed);
        }

        locking_queue::~locking_queue() {
            assert_msg(is_quiescent_empty_local<std::memory_order_acquire>(),
                             "head ant tail must be the same, meaning task pool is empty");

            this->task_pool_ptr = nullptr;
        }

        ssize_t locking_queue::prepare_pool_for_tasks(size_t tasks_count) {
            auto tail = this->tail.load(std::memory_order_acquire);
            const size_t tail0 = tail;
            const size_t new_tail = tail0 + tasks_count;

            if (new_tail < task_pool_size) {
                /// There is enough space in task pool, everything is good
            } else if (new_tail <= task_pool_size - min_task_pool_size * 0.25) {
                /// Do compaction process on a task pool
                auto victim = lock();
                auto head = this->head.load(std::memory_order_acquire);

                tail -= head;

                assert_msg(is_quiescent<std::memory_order_relaxed>(),
                                 "task pool must be locked at this point!");

                /// If the free space at the beginning of the task pool is too short, we are likely facing a
                /// pathological single-producer-multiple-consumers scenario, and thus it's better to expand
                /// the job pool. Relocate the busy part to the beginning of the deque.
                memory::copy_align16(this->task_pool_ptr, this->task_pool_ptr + head, tail * size_of_ptr_trait::value);

                this->head.store(0, std::memory_order_relaxed);
                this->tail.store(tail, std::memory_order_relaxed);

                unlock(victim);
            } else {
                return no_space;
            }

            return tail;
        }

        bool locking_queue::enqueue(std::initializer_list<task*> tasks, bool external) {
            constexpr size_t one_task = 1;

            auto tasks_count = list.size();
            auto current_tail = prepare_pool_for_tasks(tasks_count);
            auto pool_ptr = this->task_pool_ptr.load(std::memory_order_consume);

            for(auto &t : tasks) {
                pool_ptr[current_tail] = t;
                current_tail += one_task;
            }

            assert(current_tail < task_pool_size);
            publish_task_pool(current_tail);

            return true;
        }

        bool pool::dequeue_task_pessimistic(task*& output) {
            auto victim = lock();
            if (!victim) {
                return nullptr;
            }

            auto result = true;
            auto head = this->head.load(std::memory_order_relaxed);
            const auto head0 = head;
            auto skip_and_bump = 0;

            this->head.store(++head, std::memory_order_relaxed);
            std::atomic_thread_fence(std::memory_order_seq_cst);

            if (head > this->tail.load(std::memory_order_relaxed)) {
                /// Stealing attempt failed, deque contents has not been changed by us
                this->head.store(head0, std::memory_order_relaxed);
                ++skip_and_bump;
                result = false;
            } else {
                std::atomic_thread_fence(std::memory_order_acq_rel);
                output = victim[head - 1];

                assert(output);
                const auto head1 = head0;

                if (head1 < head) {
                    /// Some proxies in the task pool have been bypassed. Need to close the hole left by the stolen job. The
                    /// following variant: victim[head - 1] = victim[head0]; is of constant time, but creates a potential
                    /// for degrading stealing mechanism efficiency and growing owner's stack size too much becauseof moving
                    /// earlier split off (and thus larger) chunks closer to owner's end of the deque (tail). So we use
                    /// linear time variant that is likely to be amortized to be near-constant time, though, and preserves
                    /// stealing efficiency premises. These changes in the deque must be released to the owner.
                    memory::copy_align16(victim + head1, victim + head, (head - head1) * sizeof(void *));
                    this->head.store(head1, std::memory_order_relaxed);

                    if (head > this->tail.load(std::memory_order_relaxed)) {
                        ++skip_and_bump;
                    }
                }

                victim[head0] = nullptr;
            }

            unlock(victim);

            if (bumped_head_tail != nullptr) {
                if (--skip_and_bump > 0) {
                    *bumped_head_tail = true;
                }
            }

            return result;
        }

        bool pool::dequeue_task_optimistic(task*& output) {
            auto out_task = nullptr;
            auto tail = this->head.load(std::memory_order_relaxed);

            this->tail.store(--tail, std::memory_order_relaxed);
            std::atomic_thread_fence(std::memory_order_acq_rel);

            if (this->head.load(std::memory_order_relaxed) > tail) {
                acquire(); /// Do smooth lock instead of hard like in lock()
                auto head = this->head.load(std::memory_order_relaxed);
                if (head <= tail) {
                    /// The thief backed-off, so grab the task instance
                    auto pool_ptr = this->task_pool_ptr.load(std::memory_order_relaxed);
                    std::swap(pool_ptr[tail], out_task);
                    assert(out_task);
                }
#if defined(DEBUG)
                else {
                        auto head_debug = this->head.load(std::memory_order_acquire);
                        auto tail_debug = this->tail.load(std::memory_order_acquire);
                        assert_msg(head == head_debug && tail == tail_debug && head == tail + 1,
                                         "task pool acquisition failed - victim arbitration algorithm failure");
                    }
#endif

                if (head < tail) {
                    release();
                } else {
                    this->tail.store(0, std::memory_order_relaxed);
                    this->head.store(0, std::memory_order_relaxed);

                    std::atomic_thread_fence(std::memory_order_release);

                    assert_msg(this->task_pool == const_cast<task **>(the_locked_pool),
                                     "task pool must be locked when leaving area out of tasks");

                    assert_msg(is_quiescent_empty<std::memory_order_acquire>(),
                                     "cannot leave area when the task pool is empty");

                    /// No release fence is necessary here as this assignment precludes external
                    /// accesses to the local task pool when becomes visible. Thus it is harmless
                    /// if it gets hoisted above preceding local bookkeeping manipulations.
                    this->task_pool.store(const_cast<task **>(the_empty_pool), std::memory_order_release);
                }
            } else {
                auto pool_ptr = this->task_pool_ptr.load(std::memory_order_relaxed);
                std::atomic_thread_fence(std::memory_order_acq_rel);
                std::swap(out_task, pool_ptr[tail]);
                assert(out_task);
            }

            output = out_task;
            return result != nullptr;
        }

        task **pool::lock() {
            task **this_victim;
            spin_wait backoff;
            auto is_sync_prepare_done = false;

            while(true) {
                    /// No need to lock bacause all thieves have this in their's cache line storage.
                    this_victim = this->task_pool;

                    /// NOTE: Do not use comparison of head and tail indices to check for the presence of work in the victim's
                    /// job pool, as they may give incorrect indication because of job pool relocations.
                    if (this_victim == const_cast<task **>(the_empty_pool)) {
                        break;
                    }

                    auto expected = const_cast<task **>(the_locked_pool);
                    if (this_victim != expected &&
                        this->task_pool.compare_exchange_weak(expected, this_victim,
                                                              std::memory_order_release, std::memory_order_relaxed)) {
                        break;
                    }

                    backoff.spin_once();
            };

            BOOST_ASSERT(this_victim == const_cast<task **>(the_empty_pool) ||
                         (this->task_pool == const_cast<task **>(the_locked_pool) &&
                          this_victim != const_cast<task **>(the_locked_pool)));

            return this_victim;
        }

        void pool::unlock(task **victim) {
            BOOST_ASSERT_MSG(this->task_pool == const_cast<task **>(the_locked_pool),
                             "this task pool is not locked, but must be!");

            this->task_pool.store(victim, std::memory_order_release);
        }

        void pool::acquire() {
            if (is_empty()) {
                return;
            }

            spin_wait backoff;
            while(true) {
                    /// Local copy of the area slot task pool pointer is necessary for the next assertion to work correctly to
                    /// exclude asynchronous state transition effect.
                    task **pool = this->task_pool.load(std::memory_order_relaxed);

                    BOOST_ASSERT_MSG(pool == const_cast<task **>(the_locked_pool) ||
                    pool == this->task_pool_ptr.load(std::memory_order_relaxed),
                    "task pool ownership corruption?!");

                    auto expected = this->task_pool_ptr.load(std::memory_order_relaxed);
                    if (this->task_pool != const_cast<task **>(the_locked_pool) &&
                        this->task_pool.compare_exchange_weak(expected, const_cast<task **>(the_locked_pool))) {
                        break;
                    }

                    /// Someone else acquired a lock, so pause and do exponential backoff.
                    backoff.spin_once();
            };

            std::atomic_thread_fence(std::memory_order_acq_rel);
            BOOST_ASSERT_MSG(this->task_pool == const_cast<task **>(the_locked_pool),
                             "not really acquired task pool");
        }

        void pool::release() {
            if (is_empty()) {
                return;
            }

            assert_msg(
                    this->task_pool.load(std::memory_order_acquire) == const_cast<task **>(the_locked_pool),
                    "task pool is not really locked");

            auto pool_ptr = this->task_pool_ptr.load(std::memory_order_relaxed);
            this->task_pool.store(pool_ptr, std::memory_order_relaxed);
        }
    }
}

