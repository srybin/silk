#pragma once

#include <tpf/task_queue_iface.h>
#include <tpf/concurrent_queue.h>

namespace tpf {
    namespace task_queues {
        class lockfree_ring_buffer_queue : public task_queue_iface {
        public:
            /// @brief Called on every task spawning
            virtual bool try_enqueue(std::initializer_list<task&> tasks, bool external) final {
                auto queue = external ? &this->outter_queue : &this->inner_queue;
                for(auto &value : tasks) {
                    if(!queue->try_enqueue(value)){
                        return false;
                    }
                }

                return true;
            }

            /// @brief Called when origin thread tries to access this queue(e.g. loads task)
            virtual bool try_dequeue_optimistic(task*& t) final {
                return inner_queue.try_dequeue(t);
            }

            /// @brief Called when foreign thread tries to access this queue(e.g. steals task)
            virtual bool try_dequeue_pessimistic(task*& t) final {
                return outter_queue.try_dequeue(t);
            }

        private:
            concurrent_queue<task*> inner_queue;
            concurrent_queue<task*> outter_queue;
        };
    }
}
