#pragma once

#include <initializer_list>

namespace tpf {
    /// @brief An entry interface to make customized version of task queue for scheduler
    class task_queue_iface {
    public:
        virtual ~task_queue_iface() {}

        /// @brief Called on tasks spawning
        /// @param[in] tasks list of tasks to be spawned on queue
        /// @param[in] external indicates place of task being spawned
        virtual bool try_enqueue(std::initializer_list<task&> tasks, bool external) = 0;

        /// @brief Called when origin thread tries to access this queue(e.g. loads task)
        /// @param[in,out] output reference to a temporary pointer to a task.
        /// @return True if successfully got task from queue, otherwise false.
        virtual bool try_dequeue_optimistic(task*& output) = 0;

        /// @brief Called when foreign thread tries to access this queue(e.g. steals task)
        /// @param[in,out] output reference to a temporary pointer to a task.
        /// @return True if successfully stealed task from queue, otherwise false.
        /// @remark Also returned "false" means that queue could be updated while stealing.
        virtual bool try_dequeue_pessimistic(task*& output) = 0;
    };
}
