#include "./../src/silk_pool.h"
#include <ucontext.h>
#include <functional>
#include <sys/types.h>
#include <sys/event.h>
#include <unistd.h>
#include <sys/socket.h>
#include <fcntl.h>

namespace silk {
    namespace demo_runtime_3_1 {
        struct coro_frame : silk::task {
            std::function<void()> after_yield;
            int read_sequence_count;
            bool is_suspended;
            ucontext_t* coro;
            int stack_size;
            char* stack;
        };
        
        struct uwcontext : silk::wcontext {
            coro_frame* current_coro_frame;
            ucontext_t* scheduler_coro;  
        };
        
        silk::wcontext* makeuwcontext() {
            uwcontext* c = new uwcontext();
            silk::init_wcontext(c);
            c->scheduler_coro = new ucontext_t();
            return (silk::wcontext*)c;
        }
        
        uwcontext* fetch_current_uwcontext() {
            return (uwcontext*) silk::wcontexts[silk::current_worker_id];
        }
        
        void yield() {
            uwcontext* c = fetch_current_uwcontext();
            
            c->current_coro_frame->is_suspended = true;
            
            swapcontext(c->current_coro_frame->coro, c->scheduler_coro);
        }
        
        #define define_coro (void(*)())
        #define yield yield();
        
        template<typename... Args> coro_frame* spawn( void(*func)(), int stack_size, int args_size, Args... args ) {
            char* stack = new char[stack_size];
            ucontext_t* coro = new ucontext_t();
            getcontext(coro);
            coro->uc_stack.ss_sp = stack;
            coro->uc_stack.ss_size = stack_size;
            makecontext(coro, func, args_size, args...);
        
            coro_frame* f = new coro_frame();
            f->stack_size = stack_size;
            f->stack = stack;
            f->coro = coro;
        
            silk::spawn( silk::current_worker_id, (silk::task*) f );
            
            return f;
        }
        
        void resume(coro_frame* frame) {
            spawn(current_worker_id, (silk::task*) frame);
        }
        
        void schedule( silk::task* t ) {
            uwcontext* c = fetch_current_uwcontext();
        
            c->current_coro_frame = (coro_frame*) t;
            c->current_coro_frame->is_suspended = false;
            c->current_coro_frame->coro->uc_link = c->scheduler_coro;
        
            std::atomic_thread_fence(std::memory_order_acquire);
        
            swapcontext( c->scheduler_coro, c->current_coro_frame->coro );
        
            if ( c->current_coro_frame->is_suspended ) {
                std::function<void()> ay = c->current_coro_frame->after_yield;
               
                c->current_coro_frame->after_yield = nullptr;
               
                if ( ay ) {
                    ay();
                }
               
                return;
            }
            
            delete c->current_coro_frame->coro;
            delete c->current_coro_frame->stack;
            delete c->current_coro_frame;
        }
        
        int kq;
        
        typedef struct io_read_frame_t {
            coro_frame* coro_frame;
            int nbytes;
            char* buf;
            int n;
        } io_read_frame;
        
        int read_async(const int socket, char* buf, const int nbytes ) {
            uwcontext* c = fetch_current_uwcontext();
        
            if (c->current_coro_frame->read_sequence_count < 32) {
                memset(buf, 0, nbytes);
               
                int n = read(socket, buf, nbytes); //NON-BLOCKING MODE...
               
                if (n >= 0 || (n == -1 && errno != EAGAIN)) {
                    c->current_coro_frame->read_sequence_count++;
        
                    return n;
                }
            }
        
            c->current_coro_frame->read_sequence_count = 0;
        
            io_read_frame* frame = new io_read_frame();
            frame->coro_frame = c->current_coro_frame;
            frame->nbytes = nbytes;
            frame->buf = buf;
        
            c->current_coro_frame->after_yield = [=]() {
                struct kevent evSet;
                EV_SET(&evSet, socket, EVFILT_READ, EV_ADD | EV_ONESHOT, 0, 0, frame);
                assert(-1 != kevent(kq, &evSet, 1, NULL, 0, NULL));
            };
        
            yield
        
            int n = frame->n;
        
            delete frame;
        
            return n;
        }
        
        int accept_async(const int listensocket, struct sockaddr* addr, socklen_t* socklen) {
            int s;
        
            while(1) {
                s = accept(listensocket, addr, socklen); //NON-BLOCKING MODE...
        
                if (s == -1 && errno == EAGAIN) {
                    uwcontext* c = fetch_current_uwcontext();
               
                    c->current_coro_frame->after_yield = [=]() {
                        struct kevent evSet;
                        EV_SET(&evSet, listensocket, EVFILT_READ, EV_ADD | EV_ONESHOT, 0, 0, c->current_coro_frame);
                        assert(-1 != kevent(kq, &evSet, 1, NULL, 0, NULL));
                    };
                   
                    yield
                   
                    continue;
                }
                
                fcntl(s, F_SETFL, fcntl(s, F_GETFL, 0) | O_NONBLOCK);
        
                return s;
            }
        }
    }
}