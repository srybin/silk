#include <experimental/coroutine>
#include "./../src/silk_pool.h"
#include <sys/types.h>
#include <sys/event.h>

template<typename T> struct silk__coro_promise;
template<typename T> struct silk__coro;
struct silk__independed_coro;

struct silk__coro_promise_base {
	std::experimental::coroutine_handle<> continuation;
};

struct silk__final_awaitable {
	bool await_ready() const noexcept { return false; }

	template<typename T> void await_suspend(std::experimental::coroutine_handle<T> coro) {
		silk__coro_promise_base& p = coro.promise();

		if (p.continuation) {
			p.continuation.resume();
		}
	}

	void await_resume() noexcept {}
};

struct silk__frame : public silk__task {
	std::experimental::coroutine_handle<> coro;

	silk__frame(std::experimental::coroutine_handle<> c) : coro(c) {}
};

void silk__spawn(std::experimental::coroutine_handle<> coro) {
	silk__spawn(silk__current_worker_id, (silk__task*) new silk__frame(coro));
}

template<typename T = void> struct silk__coro_awaitable {
	silk__coro<T>& awaitable;

	bool await_ready() noexcept { return awaitable.coro.done(); }

	void await_suspend(std::experimental::coroutine_handle<> coro) noexcept {
		silk__coro_promise_base& p = awaitable.coro.promise();
		p.continuation = coro;
		awaitable.coro.resume();
	}

	auto await_resume() noexcept { return awaitable.result(); }
};

template<typename T> struct silk__coro_promise : public silk__coro_promise_base {
	std::exception_ptr e_;
	T v_;

	silk__coro<T> get_return_object() noexcept;

	auto initial_suspend() { return std::experimental::suspend_always(); }

	auto final_suspend() { return silk__final_awaitable{}; }

	void unhandled_exception() { e_ = std::current_exception(); }

	void return_value(const T v) { v_ = v; }

	T result() {
		if (e_) {
			std::rethrow_exception(e_);
		}

		return v_;
	}
};

template<> struct silk__coro_promise<void> : public silk__coro_promise_base {
	silk__coro_promise() noexcept = default;
	silk__coro<void> get_return_object() noexcept;
	auto initial_suspend() { return std::experimental::suspend_always{}; }

	auto final_suspend() { return silk__final_awaitable{}; }

	void return_void() noexcept {}

	std::exception_ptr e_;
	void unhandled_exception() { e_ = std::current_exception(); }
	void result() {
		if (e_) {
			std::rethrow_exception(e_);
		}
	}
};

template<typename T = void> struct silk__coro {
	using promise_type = silk__coro_promise<T>;

	std::experimental::coroutine_handle<silk__coro_promise<T>> coro;

	silk__coro(std::experimental::coroutine_handle<silk__coro_promise<T>> c) : coro(c) {
	}

	~silk__coro() {
		if (coro && coro.done())
			coro.destroy();
	}

	const T result() { return coro.promise().result(); }

	silk__coro_awaitable<T> operator co_await() { return silk__coro_awaitable<T> {*this}; }
};

template<typename T> silk__coro<T> silk__coro_promise<T>::get_return_object() noexcept {
	return silk__coro<T> { std::experimental::coroutine_handle<silk__coro_promise>::from_promise(*this) };
}

inline silk__coro<void> silk__coro_promise<void>::get_return_object() noexcept {
	return silk__coro<void> { std::experimental::coroutine_handle<silk__coro_promise>::from_promise(*this) };
}

struct silk__independed_coro_promise {
	silk__independed_coro get_return_object() noexcept;
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

struct silk__independed_coro {
	using promise_type = silk__independed_coro_promise;

	std::experimental::coroutine_handle<> coro;

	silk__independed_coro(std::experimental::coroutine_handle<> c) : coro(c) { }
};

inline silk__independed_coro silk__independed_coro_promise::get_return_object() noexcept {
	return silk__independed_coro{ std::experimental::coroutine_handle<silk__independed_coro_promise>::from_promise(*this) };
}

void silk__spawn(silk__independed_coro c) {
	silk__spawn(c.coro);
}

void silk__schedule(silk__task* t) {
	silk__frame* c = (silk__frame*)t;

	c->coro.resume();

	delete c;
}

struct silk__yield_awaitable {
	bool await_ready() const noexcept { return false; }

	template<typename T> void await_suspend(std::experimental::coroutine_handle<T> c) {
		silk__spawn(c);
	}

	void await_resume() noexcept {}
};

auto silk__yield() {
	return silk__yield_awaitable{};
}