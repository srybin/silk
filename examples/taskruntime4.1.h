#include <experimental/coroutine>
#include "./../src/silk_pool.h"
#include <sys/types.h>
#include <sys/event.h>

namespace silk {
    namespace demo_runtime_4_1 {
        template<typename T> struct task_promise;
        template<typename T> struct task;
        struct independed_task;
        
        struct task_promise_base {
        	std::experimental::coroutine_handle<> continuation;
        };
        
        struct final_awaitable {
        	bool await_ready() const noexcept { return false; }
        
        	template<typename T> void await_suspend(std::experimental::coroutine_handle<T> coro) {
        		coro.promise().continuation.resume();
        	}
        
        	void await_resume() noexcept {}
        };
        
        struct frame : public silk::task {
        	std::experimental::coroutine_handle<> coro;
        
        	frame(std::experimental::coroutine_handle<> c) : coro(c) {}
        };
        
        void spawn(std::experimental::coroutine_handle<> coro) {
        	silk::spawn(silk::current_worker_id, (silk::task*) new frame(coro));
        }
        
        template<typename T = void> struct task_awaitable {
        	task<T>& awaitable;
        
        	bool await_ready() noexcept { return awaitable.coro.done(); }
        
        	void await_suspend(std::experimental::coroutine_handle<> coro) noexcept {
        		awaitable.coro.promise().continuation = coro;
        		awaitable.coro.resume();
        	}
        
        	auto await_resume() noexcept { return awaitable.result(); }
        };
        
        template<typename T> struct task_promise : public task_promise_base {
        	std::exception_ptr e_;
        	T v_;
        
        	task<T> get_return_object() noexcept;
        
        	auto initial_suspend() { return std::experimental::suspend_always(); }
        
        	auto final_suspend() { return final_awaitable{}; }
        
        	void unhandled_exception() { e_ = std::current_exception(); }
        
        	void return_value(const T v) { v_ = v; }
        
        	T result() {
        		if (e_) {
        			std::rethrow_exception(e_);
        		}
        
        		return v_;
        	}
        };
        
        template<> struct task_promise<void> : public task_promise_base {
        	task_promise() noexcept = default;
        	task<void> get_return_object() noexcept;
        	auto initial_suspend() { return std::experimental::suspend_always{}; }
        
        	auto final_suspend() { return final_awaitable{}; }
        
        	void return_void() noexcept {}
        
        	std::exception_ptr e_;
        	void unhandled_exception() { e_ = std::current_exception(); }
        	void result() {
        		if (e_) {
        			std::rethrow_exception(e_);
        		}
        	}
        };
        
        template<typename T = void> struct task {
        	using promise_type = task_promise<T>;
        
        	std::experimental::coroutine_handle<task_promise<T>> coro;
        
        	task(std::experimental::coroutine_handle<task_promise<T>> c) : coro(c) {
        	}
        
        	~task() {
        		if (coro && coro.done())
        			coro.destroy();
        	}
        
        	const T result() { return coro.promise().result(); }
        
        	task_awaitable<T> operator co_await() { return task_awaitable<T> {*this}; }
        };
        
        template<typename T> task<T> task_promise<T>::get_return_object() noexcept {
        	return task<T> { std::experimental::coroutine_handle<task_promise>::from_promise(*this) };
        }
        
        inline task<void> task_promise<void>::get_return_object() noexcept {
        	return task<void> { std::experimental::coroutine_handle<task_promise>::from_promise(*this) };
        }
        
        struct independed_task_promise {
        	independed_task get_return_object() noexcept;
        	auto initial_suspend() { return std::experimental::suspend_always{}; }
        
        	auto final_suspend() { return std::experimental::suspend_never{}; }
        
        	void return_void() noexcept {}
        
        	std::exception_ptr e_;
        	void unhandled_exception() { e_ = std::current_exception(); }
        	void result() {
        		if (e_) {
        			std::rethrow_exception(e_);
        		}
        	}
        };
        
        struct independed_task {
        	using promise_type = independed_task_promise;
        
        	std::experimental::coroutine_handle<> coro;
        
        	independed_task(std::experimental::coroutine_handle<> c) : coro(c) { }
        };
        
        inline independed_task independed_task_promise::get_return_object() noexcept {
        	return independed_task{ std::experimental::coroutine_handle<independed_task_promise>::from_promise(*this) };
        }
        
        void spawn(independed_task c) {
        	spawn(c.coro);
        }
        
        void schedule(silk::task* t) {
        	frame* c = (frame*)t;
        
        	c->coro.resume();
        
        	delete c;
        }
        
        struct yield_awaitable {
        	bool await_ready() const noexcept { return false; }
        
        	template<typename T> void await_suspend(std::experimental::coroutine_handle<T> c) {
        		spawn(c);
        	}
        
        	void await_resume() noexcept {}
        };
        
        auto yield() {
        	return yield_awaitable{};
        }   
    }
}