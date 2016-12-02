#pragma once

#include <functional>

namespace tpf {
    class task_queue_iface;

    struct initializer_params {
        int task_workers_count;
        int io_workers_count;
    };

    /// @brief RAII-based initializer for the library: automatically constructs and destructs
    /// TPF on program startup and shutdown.
    class initializer {
    public:
        initializer(int queue_size = 1024, initializer_params* params = nullptr) {
            initialize(queue_size);
        }

        initializer(task_queue_iface* queue, initializer_params* params = nullptr) {
            initialize(queue);
        }

        template<typename TFinalizer>
        initializer(task_queue_iface* queue, TFinalizer finalizer, initializer_params* params = nullptr) {
            initialize(queue, std::move(finalizer));
        }

        ~initializer() {
            if(is_initialized()) {
                shutdown();
            }
        }

        /// @brief Initializes scheduler with default task queue provider
        /// \param queue_size[in]
        /// \param params[in]
        /// \return
        bool initialize(int queue_size, initializer_params* params);

        /// @brief Initializes scheduler with custom task queue provider. Programmer must
        /// provide code that finalizes task queue after scheduler being disposed (e.g. after shutdown call).
        bool initialize(task_queue_iface* queue, initializer_params* params);

        /// @brief Initialized scheduler with custom task queue provider and finalization function
        /// that finalizes task queue after scheduler being disposed (e.g. after shutdown call).
        bool initialize(task_queue_iface* queue, std::function<void(task_queue_iface*)> finalizer,
                        initializer_params* params);

        /// @brief Shutdowns scheduler instance and cleans up memory.
        void shutdown();

        /// @brief Checks if scheduler being initialized
        bool is_initialized();
    };
}
