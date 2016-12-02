#pragma once
#include <atomic>

namespace tpf {
   class cancellation_token {
   public:
       cancellation_token() : is_cancelled_(false) { }

       bool is_cancelled(std::memory_order memoryOrder = std::memory_order_acquire) {
           return is_cancelled_.load(memoryOrder);
       }

       void cancel(std::memory_order memoryOrder = std::memory_order_release) {
           is_cancelled_.store(true, memoryOrder);
       }

   private:
       std::atomic<bool> is_cancelled_;
   };
}
