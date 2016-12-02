#pragma once

#include <tpf/task.h>

namespace tpf {
    struct io_completion_container {
        Task* continuation_task;
        std::thread::id source_thread_id;

        CompletionContainer() { }

        CompletionContainer(task* continuation, std::thread::id thread_id)
            : continuation_task(continuation), source_thread_id(thread_id) {
        }
    };

    class io_queue {
    public:
        virtual ~io_queue() {
        }

        void virtual enqueue(void* io, io_completion_container* completion) = 0;

        bool virtual try_dequeue(io_completion_container& completion) = 0;
    };
}
