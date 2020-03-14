#pragma once

#include <thread>
#include <atomic>
#include "./silk.h"

namespace silk {
    thread_local int current_worker_id;
    std::atomic<int> workers_count_incrementor;
    
    inline void init_wcontext(wcontext* c) {
    	c->head = c->tail = c->affinity_head = c->affinity_tail = nullptr;
    	c->random = new fast_random(c);
    	c->sync = new spin_lock();
    	c->affinity_sync = new spin_lock();
    }
    
    wcontext* makecontext() {
    	wcontext* c = new wcontext();
    	init_wcontext(c);
    	return c;
    }
    
    inline void schedule_loop(void(*s)(task*)) {
    	int wait_count = 0;
    
    	int worker_id = current_worker_id;
    
    	sem->wait();
    
    	std::atomic_thread_fence(std::memory_order_acquire);
    
    	while (1) {
    		task* t = fetch(worker_id);
    
    		if (!t) {
    			t = fetch_affinity(worker_id);
    		}
    
    		if (!t) {
    			t = steal(worker_id);
    		}
    
    		if (t) {
    			wait_count = 0;
    			s(t);
    		} else {
    			if (wait_count < 200) {
    				wait_count++;
    
    				continue;
    			}
    
    			sem->wait();
    		}
    	}
    }
    
    inline void join_main_thread_2_pool(void(*s)(task*)) {
    	int wait_count = 0;
    
    	int worker_id = current_worker_id;
    
    	while (1) {
    		task* t = fetch(worker_id);
    
    		if (!t) {
    			t = fetch_affinity(worker_id);
    		}
    
    		if (!t) {
    			t = steal(worker_id);
    		}
    
    		if (t) {
    			wait_count = 0;
    			s(t);
    		} else {
    			if (wait_count < 200) {
    				wait_count++;
    
    				continue;
    			}
    
    			return;
    		}
    	}
    }
    
    inline void join_main_thread_2_pool_in_infinity_loop(void(*s)(task*)) {
    	schedule_loop(s);
    }
    
    void start_schedule_loop_4_not_main_thread(void(*s)(task*)) {
    	current_worker_id = workers_count_incrementor.fetch_add(1, std::memory_order_acquire);
    
    	schedule_loop(s);
    }
    
    inline void init_pool(void(*s)(task*), wcontext* (*mc)(), int threads) {
    	wcontexts = (wcontext**) malloc(threads * sizeof(wcontext*));
    
    	workers_count = threads;
    
    	wcontexts[0] = mc();
    
    	current_worker_id = workers_count_incrementor.fetch_add(1, std::memory_order_acquire);
    
    	for (int i = 1; i < threads; i++) {
    		std::thread w(start_schedule_loop_4_not_main_thread, s);
    
    		wcontexts[i] = mc();
    		
    		w.detach();
    	}
    
    	std::atomic_thread_fence(std::memory_order_release);
    }
    
    inline void init_pool(void(*s)(task*), wcontext* (*mc)()) {
    	init_pool(s, mc, std::thread::hardware_concurrency());
    }
}