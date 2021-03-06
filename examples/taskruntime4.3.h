#include <experimental/coroutine>
#include "./../src/silk_pool.h"
#include <sys/types.h>
#include <sys/event.h>
#include <unistd.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <arpa/inet.h>

namespace silk {
    namespace demo_runtime_4_3 {    
        template<typename T> struct task_promise;
        template<typename T> struct task;
        struct independed_task;
        
        enum task_state { unspawned, awaitable, completed };
        
        struct task_promise_base {
        	std::atomic<task_state> state = task_state::unspawned;
        
        	std::experimental::coroutine_handle<> continuation;
        };
        
        struct frame : public silk::task {
        	std::experimental::coroutine_handle<> coro;
        
        	frame(std::experimental::coroutine_handle<> c) : coro(c) {}
        };
        
        void spawn(std::experimental::coroutine_handle<> coro) {
        	silk::spawn(silk::current_worker_id, (silk::task*) new frame(coro));
        }
        
        struct final_awaitable {
        	bool await_ready() const noexcept { return false; }
        
        	template<typename T> void await_suspend(std::experimental::coroutine_handle<T> coro) {
        		task_promise_base& p = coro.promise();
        
        		if (p.state.exchange(task_state::completed, std::memory_order_release) == task_state::awaitable) {
        			spawn(p.continuation);
        		}
        	}
        
        	void await_resume() noexcept {}
        };
        
        template<typename T = void> struct task_awaitable {
        	task<T>& awaitable;
        
        	bool await_ready() noexcept { return false; }
        
        	void await_suspend(std::experimental::coroutine_handle<> coro) noexcept {
        		task_promise_base& p = awaitable.coro.promise();
        
        		p.continuation = coro;
        
        		task_state s = task_state::unspawned;
        		if (!p.state.compare_exchange_strong(s, task_state::awaitable, std::memory_order_release)) {
        			spawn(coro);
        		}
        	}
        
        	auto await_resume() noexcept { return awaitable.result(); }
        };
        
        template<typename T> struct task_promise : public task_promise_base {
        	std::exception_ptr e_;
        	T v_;
        
        	task<T> get_return_object() noexcept;
        
        	auto initial_suspend() { return std::experimental::suspend_never{}; }
        
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
        	auto initial_suspend() { return std::experimental::suspend_never{}; }
        
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
        
        	task(std::experimental::coroutine_handle<task_promise<T>> c) : coro(c) { }
        
        	~task() {
        		if (coro && coro.done()) {
        			coro.destroy();
        		}
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
        	auto initial_suspend() { return std::experimental::suspend_never{}; }
        
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
        
        int kq;
        
        struct io_read_awaitable {
            char* buf;
            int nbytes;
            int socket;
        
	        int n;
	        std::experimental::coroutine_handle<> coro;
        
            constexpr bool await_ready() const noexcept { return false; }
                
            void await_suspend(std::experimental::coroutine_handle<> c) {
                coro = c;
                struct kevent evSet;
                EV_SET(&evSet, socket, EVFILT_READ, EV_ADD | EV_ONESHOT, 0, 0, this);
                assert(-1 != kevent(kq, & evSet, 1, NULL, 0, NULL));
            }
        
            auto await_resume() { return n; }
        };
        
        struct io_accept_awaitable {
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
                EV_SET(&evSet, listening_socket, EVFILT_READ, EV_ADD | EV_ONESHOT, 0, 0, new frame(coro));
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
        
        struct io_connect_awaitable {
        	char* host;
        	int port;
        
        	int result;
        	int err;
        	int s;
        
            bool await_ready() noexcept {
        		s = socket( AF_INET, SOCK_STREAM, 0 );
        		fcntl(s, F_SETFL, fcntl(s, F_GETFL, 0) | O_NONBLOCK);
        		struct sockaddr_in peer;
        		peer.sin_family = AF_INET;
                peer.sin_port = htons( port );
        		peer.sin_addr.s_addr = inet_addr( host );
        		result = connect( s, ( struct sockaddr * )&peer, sizeof( peer ) );
        		err = errno;
        		return result == 0 || (result = -1 && err != EINPROGRESS);
            }
               
            void await_suspend(std::experimental::coroutine_handle<> coro) {
        		struct kevent evSet;
                EV_SET(&evSet, s, EVFILT_WRITE, EV_ADD | EV_ONESHOT, 0, 0, new frame(coro));
                assert(-1 != kevent(kq, & evSet, 1, NULL, 0, NULL));
            }
           
            auto await_resume() {
        		return std::make_tuple(s, result, err);
            }
        };
        
        struct io_write_awaitable {
        	int s;
        	char* buf;
        	int bytes;
        
            bool await_ready() noexcept { return false; }
               
            void await_suspend(std::experimental::coroutine_handle<> coro) {
        		struct kevent evSet;
                EV_SET(&evSet, s, EVFILT_WRITE, EV_ADD | EV_ONESHOT, 0, 0, new frame(coro));
                assert(-1 != kevent(kq, & evSet, 1, NULL, 0, NULL));
            }
           
            auto await_resume() { return write(s, buf, bytes); }
        };
        
        auto read_async(const int socket, char* buf, const int nbytes) {
            return io_read_awaitable {buf, nbytes, socket};
        }
        
        auto accept_async( const int listening_socket ) {
            return io_accept_awaitable { listening_socket };
        }
        
        auto connect_async( char* host, int port) {
        	return io_connect_awaitable { host, port };
        }
        
        auto write_async(int socket, char* buf, int bytes) {
        	return io_write_awaitable{ socket, buf, bytes };
        }   
    }
}