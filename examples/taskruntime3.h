#include "./../src/silk.h"
#include <ucontext.h>
#include <sys/types.h>
#include <sys/event.h>
#include <unistd.h>

typedef struct silk__coro_frame_t : silk__task {
    int read_sequence_count;
    bool is_suspended;
    ucontext_t* coro;
    int stack_size;
    int affinity_to;
    char* stack;
} silk__coro_frame;

typedef struct silk__uwcontext_t : silk__wcontext {
    silk__coro_frame* current_coro_frame;
    ucontext_t* scheduler_coro;  
} silk__uwcontext;

silk__wcontext* silk__makeuwcontext() {
    silk__uwcontext* c = new silk__uwcontext();
    silk__init_wcontext(c);
    c->scheduler_coro = new ucontext_t();
    return (silk__wcontext*)c;
}

silk__uwcontext* silk__fetch_current_uwcontext() {
    return (silk__uwcontext*) silk__wcontexts[silk__current_worker_id];
}

void silk__yield() {
    silk__uwcontext_t* c = silk__fetch_current_uwcontext();
    
    c->current_coro_frame->is_suspended = true;
    
    swapcontext(c->current_coro_frame->coro, c->scheduler_coro);
}

#define silk__coro (void(*)())
#define silk__yield silk__yield();

template<typename... Args> silk__coro_frame* silk__spawn( void(*func)(), int stack_size, int args_size, Args... args ) {
    char* stack = new char[stack_size];
    ucontext_t* coro = new ucontext_t();
    getcontext(coro);
    coro->uc_stack.ss_sp = stack;
    coro->uc_stack.ss_size = stack_size;
    makecontext(coro, func, args_size, args...);

    silk__coro_frame* f = new silk__coro_frame();
    f->stack_size = stack_size;
    f->stack = stack;
    f->coro = coro;

    silk__spawn( silk__current_worker_id, (silk__task*) f );
    
    return f;
}

void silk__resume(silk__coro_frame* frame){
    silk__enqueue_affinity(frame->affinity_to, (silk__task*) frame);
}

void silk__schedule( silk__task* t ) {
    silk__uwcontext* c = silk__fetch_current_uwcontext();

    c->current_coro_frame = (silk__coro_frame*) t;
    c->current_coro_frame->affinity_to = silk__current_worker_id;
    c->current_coro_frame->is_suspended = false;
    c->current_coro_frame->coro->uc_link = c->scheduler_coro;

    std::atomic_thread_fence(std::memory_order_acquire);

    swapcontext( c->scheduler_coro, c->current_coro_frame->coro );

    if ( c->current_coro_frame->is_suspended )
        return;
    
    delete c->current_coro_frame->coro;
    delete c->current_coro_frame->stack;
    delete c->current_coro_frame;
}

int kq;

typedef struct silk__io_read_frame_t {
    silk__coro_frame* coro_frame;
    int nbytes;
    char* buf;
    int n;
} silk__io_read_frame;

int silk__read_async(const int socket, char* buf, const int nbytes ) {
    silk__uwcontext* c = silk__fetch_current_uwcontext();

    if (c->current_coro_frame->read_sequence_count < 32) {
        memset(buf, 0, nbytes);
       
        int n = read(socket, buf, nbytes); //NON-BLOCKING MODE...
       
        if ( n >= 0 || (n == -1 && errno != EAGAIN)) {
            c->current_coro_frame->read_sequence_count++;

            return n;
        }
    }

    c->current_coro_frame->read_sequence_count = 0;

    silk__io_read_frame* frame = new silk__io_read_frame();
    frame->coro_frame = c->current_coro_frame;
    frame->nbytes = nbytes;
    frame->buf = buf;

    struct kevent evSet;
    EV_SET(&evSet, socket, EVFILT_READ, EV_ADD | EV_ONESHOT, 0, 0, frame);
    assert(-1 != kevent(kq, &evSet, 1, NULL, 0, NULL));

    silk__yield

    int n = frame->n;

    delete frame;

    return n;
}