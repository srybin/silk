#pragma once

#include <thread>
#include <atomic>
#include "semaphore.h"

template<int> struct int_to_type {};

class silk__fast_random {
	unsigned x, c;
	static const unsigned a = 0x9e3779b1; // a big prime number
public:
	unsigned short get() {
		return get(x);
	}

	unsigned short get(unsigned& seed) {
		unsigned short r = (unsigned short)(seed >> 16);
		seed = seed*a + c;
		return r;
	}

	silk__fast_random(void* unique_ptr) { init(uintptr_t(unique_ptr)); }
	silk__fast_random(uint32_t seed) { init(seed); }
	silk__fast_random(uint64_t seed) { init(seed); }

	template <typename T>
	void init(T seed) {
		init(seed, int_to_type<sizeof(seed)>());
	}

	void init(uint64_t seed, int_to_type<8>) {
		init(uint32_t((seed >> 32) + seed), int_to_type<4>());
	}

	void init(uint32_t seed, int_to_type<4>) {
		// threads use different seeds for unique sequences
		c = (seed | 1) * 0xba5703f5; // c must be odd, shuffle by a prime number
		x = c ^ (seed >> 1); // also shuffle x for the first get() invocation
	}
};

class silk__spin_lock {
	std::atomic_flag lock_ = ATOMIC_FLAG_INIT;
public:
	void lock() {
		while (lock_.test_and_set(std::memory_order_acquire)) {
		}
	}

	void unlock() {
		lock_.clear(std::memory_order_release);
	}
};

typedef struct silk__task_t {
	silk__task_t* next;
	silk__task_t* prev;
} silk__task;

typedef struct silk__wcontext_t {
	silk__fast_random* random;
	silk__spin_lock* affinity_sync;
	silk__task* affinity_tail;
	silk__task* affinity_head;
	silk__spin_lock* sync;
	silk__task* tail;
	silk__task* head;
} silk__wcontext;

thread_local int silk__current_worker_id;
int silk__workers_count;
std::atomic<int> silk__workers_count_incrementor;
silk__wcontext** silk__wcontexts;
auto_reset_event* silk__sem = new auto_reset_event();

inline void silk__init_wcontext(silk__wcontext* c) {
	c->head = c->tail = c->affinity_head = c->affinity_tail = nullptr;
	c->random = new silk__fast_random(c);
	c->sync = new silk__spin_lock();
	c->affinity_sync = new silk__spin_lock();
}

silk__wcontext* silk__makecontext() {
	silk__wcontext* c = new silk__wcontext();
	silk__init_wcontext(c);
	return c;
}

inline void silk__spawn(const int worker_id, silk__task* t) {
	silk__wcontext* c = silk__wcontexts[worker_id];

	t->prev = t->next = nullptr;
    
	c->sync->lock();

	if (!c->head) {
		c->tail = c->head = t;
	} else {
		t->next = c->head;
		c->head->prev = t;
		c->head = t;
	}

	c->sync->unlock();

	silk__sem->signal(silk__workers_count);
}

inline silk__task* silk__fetch(const int worker_id) {
	silk__wcontext* c = silk__wcontexts[worker_id];

	silk__task* t = nullptr;

	c->sync->lock();

	if (c->head) {
		t = c->head;
		
		c->head = t->next;

		if (!c->head)
			c->tail = nullptr;
		else
			c->head->prev = nullptr;

		t->prev = t->next = nullptr;
	}

	c->sync->unlock();

	return t;
}

inline silk__task* silk__steal(const int thief_thread_id) {
	silk__task* t = nullptr;

	silk__wcontext* c = silk__wcontexts[thief_thread_id];

	for (int i = 0; i < 100; i++) {
		int v = c->random->get() % silk__workers_count;

		if (v == thief_thread_id)
			continue;

		silk__wcontext* vc = silk__wcontexts[v];

		vc->sync->lock();

		if (vc->head) {
			t = vc->tail;
			vc->tail = t->prev;

			if (!vc->tail)
				vc->head = nullptr;
			else
				vc->tail->next = nullptr;
			
			t->prev = t->next = nullptr;
		}

		vc->sync->unlock();

		if (t)
			return t;
	}

	return t;
}

void silk__enqueue( const int worker_id, silk__task* t ) {
	silk__wcontext* c = silk__wcontexts[worker_id];

	t->prev = t->next = nullptr;
	
	c->sync->lock();

	if (!c->head) {
		c->tail = c->head = t;
	} else {
		t->next = c->head;
		c->head->prev = t;
		c->head = t;
	}

	c->sync->unlock();
 
    silk__sem->signal(silk__workers_count);
}

void silk__enqueue_affinity( const int worker_id, silk__task* t ) {
	silk__wcontext* c = silk__wcontexts[worker_id];

	t->prev = t->next = nullptr;
	
	c->affinity_sync->lock();

	if (!c->affinity_head) {
		c->affinity_tail = c->affinity_head = t;
	} else {
		t->next = c->affinity_head;
		c->affinity_head->prev = t;
		c->affinity_head = t;
	}

	c->affinity_sync->unlock();
 
    silk__sem->signal(silk__workers_count);
}

inline silk__task* silk__fetch_affinity( const int worker_id ) {
    silk__wcontext* c = silk__wcontexts[worker_id];
 
    silk__task* t = nullptr;
 
    c->affinity_sync->lock();

	if (c->affinity_head) {
		t = c->affinity_tail;

		c->affinity_tail = t->prev;

		if (!c->affinity_tail)
			c->affinity_head = nullptr;
		else 
			c->affinity_tail->next = nullptr;
	}
 
    c->affinity_sync->unlock();
 
    return t;
}

inline void silk__schedule_loop(void(*s)(silk__task*)) {
	int wait_count = 0;

	int worker_id = silk__current_worker_id;

	silk__sem->wait();

	std::atomic_thread_fence(std::memory_order_acquire);

	while (1) {
		silk__task* t = silk__fetch(worker_id);

		if (!t) {
			t = silk__fetch_affinity(worker_id);
		}

		if (!t) {
			t = silk__steal(worker_id);
		}

		if (t) {
			wait_count = 0;
			s(t);
		} else {
			if (wait_count < 200) {
				wait_count++;

				continue;
			}

			silk__sem->wait();
		}
	}
}

inline void silk__join_main_thread_2_pool(void(*s)(silk__task*)) {
	int wait_count = 0;

	int worker_id = silk__current_worker_id;

	while (1) {
		silk__task* t = silk__fetch(worker_id);

		if (!t) {
			t = silk__fetch_affinity(worker_id);
		}

		if (!t) {
			t = silk__steal(worker_id);
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

inline void silk__join_main_thread_2_pool_in_infinity_loop(void(*s)(silk__task*)) {
	silk__schedule_loop(s);
}

void silk__start_schedule_loop_4_not_main_thread(void(*s)(silk__task*)) {
	silk__current_worker_id = silk__workers_count_incrementor.fetch_add(1, std::memory_order_acquire);

	silk__schedule_loop(s);
}

inline void silk__init_pool(void(*s)(silk__task*), silk__wcontext* (*mc)(), int threads) {
	silk__wcontexts = (silk__wcontext**) malloc(threads * sizeof(silk__wcontext*));

	silk__workers_count = threads;

	silk__wcontexts[0] = mc();

	silk__current_worker_id = silk__workers_count_incrementor.fetch_add(1, std::memory_order_acquire);

	for (int i = 1; i < threads; i++) {
		std::thread w(silk__start_schedule_loop_4_not_main_thread, s);

		silk__wcontexts[i] = mc();
		
		w.detach();
	}

	std::atomic_thread_fence(std::memory_order_release);
}

inline void silk__init_pool(void(*s)(silk__task*), silk__wcontext* (*mc)()) {
	silk__init_pool(s, mc, std::thread::hardware_concurrency());
}