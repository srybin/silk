#pragma once

#include <thread>
#include <atomic>
#include <unordered_map>
#include "semaphore.h"

template<int> struct int_to_type {};

class fast_random {
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

	fast_random(void* unique_ptr) { init(uintptr_t(unique_ptr)); }
	fast_random(uint32_t seed) { init(seed); }
	fast_random(uint64_t seed) { init(seed); }

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

class spin_lock {
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

typedef struct pool_bucket {
	void* t;
	pool_bucket* next;
	pool_bucket* prev;
} pool_bucket;

typedef struct worker_context {
	spin_lock sync;
	void* extension;
	pool_bucket* tail;
	pool_bucket* head;
	fast_random* random;
} worker_context;

typedef void(*schedule)(void*);

static std::unordered_map<std::thread::id, worker_context*> worker_contexts;
static std::unordered_map<int, std::thread::id> worker_context_indexes;

static auto_reset_event* sem = new auto_reset_event();

typedef worker_context*(*make_context_ptr)();

inline worker_context* make_context() {
	worker_context* c = new worker_context();
	c->head = c->tail = nullptr;
	c->random = new fast_random(c);
	return c;
}

inline void spawn(const std::thread::id worker_id, void* t) {
	worker_context* c = worker_contexts[worker_id];

	pool_bucket* bucket = new pool_bucket();
	bucket->t = t;
	bucket->prev = bucket->next = nullptr;

	c->sync.lock();

	if (!c->head) {
		c->tail = c->head = bucket;
	} else {
		bucket->next = c->head;
		c->head->prev = bucket;
		c->head = bucket;
	}

	c->sync.unlock();

	sem->signal();
}

inline void* fetch(const std::thread::id worker_id) {
	worker_context* c = worker_contexts[worker_id];

	void* t = nullptr;

	c->sync.lock();

	if (c->head) {
		pool_bucket* b = c->head;

		c->head = b->next;

		t = b->t;

		if (!c->head)
			c->tail = nullptr;
		else
			c->head->prev = nullptr;

		b->next = nullptr;

		delete b;
	}

	c->sync.unlock();

	return t;
}


inline void* steal(const std::thread::id thief_thread_id) {
	void* t = nullptr;

	worker_context* c = worker_contexts[thief_thread_id];

	const int bn = worker_contexts.size();

	for (int i = 0; i < 100; i++) {
		int v = c->random->get() % bn;

		const std::thread::id victim_thread_id = worker_context_indexes[v];

		if (victim_thread_id == thief_thread_id)
			continue;

		worker_context* vc = worker_contexts[victim_thread_id];

		vc->sync.lock();

		if (vc->head) {
			pool_bucket* b = vc->tail;
			vc->tail = b->prev;

			t = b->t;

			if (!vc->tail)
				vc->head = nullptr;
			else
				vc->tail->next = nullptr;

			delete b;
		}

		vc->sync.unlock();

		if (t)
			return t;
	}

	return t;
}


inline void schedule_loop(const schedule s) {
	int wait_count = 0;

	sem->wait();

	std::atomic_thread_fence(std::memory_order_acquire);

	while (true) {
		void* t = fetch(std::this_thread::get_id());

		if (!t) {
			t = steal(std::this_thread::get_id());
		}

		if (t) {
			wait_count = 0;
			s(t);
		} else {
			if (wait_count < 100) {
				wait_count++;

				continue;
			}

			sem->wait();
		}
	}
}

inline void join_main_thread_2_pool(const schedule s) {
	int wait_count = 0;

	while (true) {
		void* t = fetch(std::this_thread::get_id());

		if (!t) {
			t = steal(std::this_thread::get_id());
		}

		if (t) {
			wait_count = 0;
			s(t);
		} else {
			if (wait_count < 100) {
				wait_count++;

				continue;
			}

			return;
		}
	}
}

inline void join_main_thread_2_pool_in_infinity_loop(const schedule s) {
	schedule_loop(s);
}

inline void init_pool(schedule s, const make_context_ptr make_context_func) {
	const int cores = std::thread::hardware_concurrency();
	worker_contexts[std::this_thread::get_id()] = make_context_func();
	worker_context_indexes[0] = std::this_thread::get_id();

	for (int i = 1; i < cores; i++) {
		std::thread w(schedule_loop, s);
		
		worker_contexts[w.get_id()] = make_context_func();
		worker_context_indexes[i] = w.get_id();

		w.detach();
	}

	std::atomic_thread_fence(std::memory_order_release);
}