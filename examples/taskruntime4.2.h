#include <experimental/coroutine>
#include "./../src/silk_pool.h"
#include <sys/types.h>
#include <sys/event.h>
#include <unistd.h>
#include <sys/socket.h>
#include <fcntl.h>

template<typename T> struct silk__coro_promise;
template<typename T> struct silk__coro;
struct silk__independed_coro;

enum silk__coro_state { unspawned, spawned, awaitable, completed, destroyed };

struct silk__coro_promise_base {
	std::atomic<silk__coro_state> state = silk__coro_state::unspawned;

	std::experimental::coroutine_handle<> continuation;
};

struct silk__frame : public silk__task {
	std::experimental::coroutine_handle<> coro;

	silk__frame(std::experimental::coroutine_handle<> c) : coro(c) {}
};

void silk__spawn(std::experimental::coroutine_handle<> coro) {
	silk__spawn(silk__current_worker_id, (silk__task*) new silk__frame(coro));
}

struct silk__final_awaitable {
	bool await_ready() const noexcept { return false; }

	template<typename T> void await_suspend(std::experimental::coroutine_handle<T> coro) {
		silk__coro_promise_base& p = coro.promise();

		if (p.state.exchange(silk__coro_state::completed, std::memory_order_release) == silk__coro_state::awaitable) {
			silk__spawn(p.continuation);
		}
	}

	void await_resume() noexcept {}
};

template<typename T = void> struct silk__coro_awaitable {
	silk__coro<T>& awaitable;

	bool await_ready() noexcept { return false; }

	void await_suspend(std::experimental::coroutine_handle<> coro) noexcept {
		silk__coro_promise_base& p = awaitable.coro.promise();

		p.continuation = coro;

		silk__coro_state s = p.state.exchange(silk__coro_state::awaitable, std::memory_order_release);

		if (s == silk__coro_state::unspawned) {
			silk__spawn(awaitable.coro);
		} else if (s == silk__coro_state::completed) {
			silk__spawn(coro);
		}
	}

	auto await_resume() noexcept { 
		awaitable.coro.promise().state.store(silk__coro_state::destroyed, std::memory_order_release);

		return awaitable.result(); 
	}
};

template<typename T> struct silk__coro_promise : public silk__coro_promise_base {
	std::exception_ptr e_;
	T v_;

	silk__coro<T> get_return_object() noexcept;

	auto initial_suspend() { return std::experimental::suspend_always{}; }

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

	silk__coro(std::experimental::coroutine_handle<silk__coro_promise<T>> c) : coro(c) { }

	~silk__coro() {
		if (coro && coro.done() && coro.promise().state.load(std::memory_order_acquire) == silk__coro_state::destroyed) {
			coro.destroy();
		}
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

template<typename T = void> silk__coro<T> silk__spawn(silk__coro<T> c) {
	silk__coro_promise_base& p = c.coro.promise();
	p.state.store(silk__coro_state::spawned, std::memory_order_release);
	silk__spawn(c.coro);
	return c;
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

int kq;

typedef struct silk__io_read_frame_t {
    std::experimental::coroutine_handle<> coro; 
    int nbytes;
    char* buf;
    int n;
} silk__io_read_frame;

struct silk__io_read_awaitable {
    char* buf;
    int nbytes;
    int socket;

    silk__io_read_frame* frame;

    constexpr bool await_ready() const noexcept { return false; }
        
    void await_suspend(std::experimental::coroutine_handle<> coro) {
        frame = new silk__io_read_frame();
        frame->coro = coro;
        frame->nbytes = nbytes;
        frame->buf = buf;
        
        struct kevent evSet;
        EV_SET(&evSet, socket, EVFILT_READ, EV_ADD | EV_ONESHOT, 0, 0, frame);
        assert(-1 != kevent(kq, & evSet, 1, NULL, 0, NULL));
    }

    auto await_resume() {
        int n = frame->n;

        delete frame;
        
        return n; 
    }
};

struct silk__io_accept_awaitable {
    int listening_socket;
	
	struct sockaddr_storage addr;
	socklen_t socklen = sizeof(addr);
	bool success;
    int err;
    int s;
   
    bool await_ready() noexcept {
		s = accept(listening_socket, (struct sockaddr *)&addr, &socklen);
        success = !(s == -1 && errno == EAGAIN);
        err = errno;
        return success;
    }
       
    void await_suspend(std::experimental::coroutine_handle<> coro) {
		struct kevent evSet;
        EV_SET(&evSet, listening_socket, EVFILT_READ, EV_ADD | EV_ONESHOT, 0, 0, new silk__frame(coro));
        assert(-1 != kevent(kq, & evSet, 1, NULL, 0, NULL));
    }
   
    auto await_resume() {
		if ( success ) {
			fcntl(s, F_SETFL, fcntl(s, F_GETFL, 0) | O_NONBLOCK);
            return std::make_tuple(s, addr, err);
        }

		s = accept(listening_socket, (struct sockaddr *)&addr, &socklen);

		fcntl(s, F_SETFL, fcntl(s, F_GETFL, 0) | O_NONBLOCK);

		return std::make_tuple(s, addr, errno);
    }
};

auto silk__read_async(const int socket, char* buf, const int nbytes) {
    return silk__io_read_awaitable {buf, nbytes, socket};
}

auto silk__accept_async( const int listening_socket ) {
    return silk__io_accept_awaitable { listening_socket };
}