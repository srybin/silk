#pragma once
#include "common.h"
#include "cancellation_token.h"

#include <cassert>
#include <initializer_list>

namespace tpf {
	namespace internal {
		/// All there filels must be filled using local scheduler task allocator
		struct task_base {
			scheduler* scheduler_;
			task* continuation_;
			cancellation_token* cancellation_token_;
			std::atomic<int> ref_count_;
		};

		class allocate_root_proxy : no_assign {
		public:
			static task& allocate(size_t size);
			static void free(task& t);
		};

		class allocate_child_proxy : no_assign {
		public:
			task& allocate(size_t size);
			void free(task &t);
		};

		class allocate_as_child_of_proxy : no_assign {
		public:
			allocate_as_child_of_proxy(task& parent) : parent_(parent), proxy_(nullptr) {}
			task& allocate(size_t size);
			void free(task& t);

		private:
			task* proxy_;
			task& parent_;
		};

		class allocate_continuation_proxy : no_assign {
		public:
			task& allocate(size_t size);
			void free(task &t);
		};
	}

	class task : private cache_padding<details::task_base> {
	public:
		task();

		virtual ~task() {}

		virtual Task *execute() = 0;

		/// Allocation hepler routines

		static task &internal::allocate_root() {
			return details::allocate_root_proxy();
		}

		internal::allocate_child_proxy &allocate_child() {
			return *reinterpret_cast<details::allocate_child_proxy *>(this);
		}

		internal::allocate_continuation_proxy &allocate_continuation() {
			return *reinterpret_cast<details::allocate_continuation_proxy*>(this);
		}

		internal::allocate_child_proxy &allocate_as_child_of(task& parent) {
			return details::allocate_as_child_of_proxy(parent);
		}

		inline void *operator new(size_t bytes) {
			return ::operator new(bytes);
		}

		inline void *operator new(size_t bytes, const internal::allocate_continuation_proxy& p) {
			return &p.allocate(bytes);
		}

		inline void *operator new(size_t bytes, const internal::allocate_child_proxy& p) {
			return &p.allocate(bytes);
		}

		/// Task state access

		inline void set_continuation(task& t) {
			continuation_ = &t;
		}

		inline void set_null_continuation() {
			continuation_ = nullptr;
		}

		inline void set_ref_count(int count, std::memory_order memory_order = std::memory_order_release) {
			ref_count_.store(count, memory_order);
		}

		inline int ref_count(std::memory_order memory_order = std::memory_order_acquire) {
			return ref_count_.load(memory_order);
		}

		inline int decrement_ref_count(std::memory_order memory_order = std::memory_order_acquire) {
			return ref_count_.fetch_sub(1, memory_order) - 1;
		}

		inline int increment_ref_count(std::memory_order memory_order = std::memory_order_acquire) {
			return ref_count_.fetch_add(1, memory_order) + 1;
		}

		inline bool is_canceled(std::memory_order memory_order = std::memory_order_acquire) {
			if (cancellation_token_ == nullptr) {
				return false;
			}

			return cancellation_token_->is_cancelled(memory_order);
		}

		inline void cancel(std::memory_order memory_order = std::memory_order_release) {
			if (cancellation_token_ != nullptr) {
				cancellation_token_->cancel(memory_order);
			}
		}

		inline void set_cancellation_token(cancellation_token* token) {
			cancellation_token_ = token;
		}

		inline cancellation_token* cancellation_token() {
			return cancellation_token_;
		}

		static task* self();

	protected:
		void spawn(task& t);

		void recycle();

		inline void recycle_as_child_of(task& t) {
			continuation_ = &t;
			recycle();
		}

	private:
		friend class details::allocate_root_proxy;
		friend class details::allocate_child_proxy;
		friend class details::allocate_as_child_of_proxy;
		friend class details::allocate_continuation_proxy;
	};
}