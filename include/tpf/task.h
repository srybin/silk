#pragma once
#include "common.h"
#include "cancellation_token.h"

#include <cassert>
#include <initializer_list>

namespace tpf {
	namespace details {
		/// All there filels must be filled using local scheduler task allocator
		struct task_base {
			scheduler* scheduler_;
			task* continuation_;
			cancellation_token* cancellation_token_;
			std::atomic<int> pending_count_;
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
		virtual ~task() {}

		virtual Task *compute() = 0;

		/// Allocation hepler routines

		static task &details::allocate_root() {
			return details::allocate_root_proxy();
		}

		details::allocate_child_proxy &allocate_child() {
			return *reinterpret_cast<details::allocate_child_proxy *>(this);
		}

		details::allocate_continuation_proxy &allocate_continuation() {
			return *reinterpret_cast<details::allocate_continuation_proxy*>(this);
		}

		details::allocate_child_proxy &allocate_as_child_of(task& parent) {
			return details::allocate_as_child_of_proxy(parent);
		}

		/// Task state access

		task* continuation() {
			return _continuation;
		}

		void continuation(task* t) {
			_continuation = task;
		}

		void pending_count(int count, std::memory_order memory_order = std::memory_order_release) {
			_pendingCount.store(count, memory_order);
		}

		int pending_count(std::memory_order memory_order = std::memory_order_acquire) {
			return _pendingCount.load(memory_order);
		}

		int decrement_pending_count(std::memory_order memory_order = std::memory_order_acquire) {
			return _pendingCount.fetch_sub(1, memory_order) - 1;
		}

		int increment_pending_count(std::memory_order memory_order = std::memory_order_acquire) {
			return _pendingCount.fetch_add(1, memory_order) + 1;
		}

		void cancellation_token(cancellation_token* token) {
			cancellation_token_ = token;
		}

		cancellation_token* cancellation_token() {
			return cancellation_token_;
		}

		bool is_cancelled(std::memory_order memory_order = std::memory_order_acquire) {
			cancellation_token* token = cancellation_token_;

			if (token == nullptr) {
				return false;
			}

			return token->is_cancelled(memory_order);
		}

		void cancel(std::memory_order memory_order = std::memory_order_release) {
			cancellation_token* token = cancellation_token_;
			if (token != nullptr) {
				token->cancel(memory_order);
			}
		}

		static task* self();

	protected:
		void spawn(task* t);

		void recycle();

		void as_continuation(Task* t) {
			assert(t != nullptr);
			task->continuation(this->continuation());
			this->continuation(nullptr);
		}

		void recycle_as_child_of(task* t) {
			assert(t != nullptr);
			this->continuation(t);
			Recycle();
		}

	private:
		friend class details::allocate_root_proxy;
		friend class details::allocate_child_proxy;
		friend class details::allocate_as_child_of_proxy;
		friend class details::allocate_continuation_proxy;
	};
}