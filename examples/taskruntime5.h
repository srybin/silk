#include <experimental/coroutine>
#include <sys/types.h>
#include <sys/event.h>
#include "./../src/silk.h"

struct silk__coro : public silk__task {
    struct promise_type;

    std::experimental::coroutine_handle<promise_type> h;
    
    silk__coro(std::experimental::coroutine_handle<promise_type> handle) : h(handle) {}

    void finalize() { h.destroy(); }
    
    bool resume() {
        if (not h.done())
            h.resume();
        return not h.done();
    }
};

struct silk__coro::promise_type {  
    auto get_return_object() { return std::experimental::coroutine_handle<promise_type>::from_promise(*this); }
    
    auto initial_suspend() { return std::experimental::suspend_always(); }
    
    auto final_suspend() { return std::experimental::suspend_always(); }
    
    void return_void() {}
    
    void unhandled_exception() { std::terminate(); }

    static silk__coro get_return_object_on_allocation_failure() { throw std::bad_alloc(); }
};

typedef struct silk__uwcontext_t : silk__wcontext {
    silk__coro* current_coro;  
} silk__uwcontext;

silk__wcontext* silk__makeuwcontext() {
    silk__uwcontext* c = new silk__uwcontext();
    silk__init_wcontext(c);
    return (silk__wcontext*)c;
}

silk__uwcontext* silk__fetch_current_uwcontext() {
    return (silk__uwcontext*) silk__wcontexts[silk__current_worker_id];
}

silk__coro* silk__spawn(silk__coro coro) {
    silk__coro* c = new silk__coro(coro);

    silk__spawn(silk__current_worker_id, (silk__task*) c);

    return c;
}

void silk__spawn(silk__coro* coro) {
    silk__spawn(silk__current_worker_id, (silk__task*) coro);
}

void silk__schedule(silk__task* t) {
    silk__coro* c = (silk__coro*)t;

    silk__fetch_current_uwcontext()->current_coro = c;

    if (!c->resume()) {
        c->finalize();
    }
}

int kq;

typedef struct silk__io_read_frame_t {
    silk__coro* coro; 
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
        frame->coro = silk__fetch_current_uwcontext()->current_coro;
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

auto silk__read_async(const int socket, char* buf, const int nbytes) {
    return silk__io_read_awaitable {buf, nbytes, socket};
}