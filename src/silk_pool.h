#pragma once

#include <thread>
#include <atomic>
#include "./silk.h"

thread_local int silk__current_worker_id;
std::atomic<int> silk__workers_count_incrementor;

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