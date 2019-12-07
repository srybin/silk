#pragma once

#include <thread>
#include <atomic>

#if defined(_WIN32)
//---------------------------------------------------------
// Semaphore (Windows)
//---------------------------------------------------------

#include <windows.h>
#undef min
#undef max

class Semaphore
{
private:
	HANDLE m_hSema;

	Semaphore(const Semaphore& other) = delete;
	Semaphore& operator=(const Semaphore& other) = delete;

public:
	Semaphore(int initialCount = 0)
	{
		m_hSema = CreateSemaphore(NULL, initialCount, MAXLONG, NULL);
	}

	~Semaphore()
	{
		CloseHandle(m_hSema);
	}

	void wait()
	{
		WaitForSingleObject(m_hSema, INFINITE);
	}

	void signal(int count = 1)
	{
		ReleaseSemaphore(m_hSema, count, NULL);
	}
};


#elif defined(__MACH__)
//---------------------------------------------------------
// Semaphore (Apple iOS and OSX)
// Can't use POSIX semaphores due to http://lists.apple.com/archives/darwin-kernel/2009/Apr/msg00010.html
//---------------------------------------------------------

#include <mach/mach.h>

class Semaphore
{
private:
	semaphore_t m_sema;

	Semaphore(const Semaphore& other) = delete;
	Semaphore& operator=(const Semaphore& other) = delete;

public:
	Semaphore(int initialCount = 0)
	{
		assert(initialCount >= 0);
		semaphore_create(mach_task_self(), &m_sema, SYNC_POLICY_FIFO, initialCount);
	}

	~Semaphore()
	{
		semaphore_destroy(mach_task_self(), m_sema);
	}

	void wait()
	{
		semaphore_wait(m_sema);
	}

	void signal()
	{
		semaphore_signal(m_sema);
	}

	void signal(int count)
	{
		while (count-- > 0)
		{
			semaphore_signal(m_sema);
		}
	}
};


#elif defined(__unix__)
//---------------------------------------------------------
// Semaphore (POSIX, Linux)
//---------------------------------------------------------

#include <semaphore.h>

class Semaphore
{
private:
	sem_t m_sema;

	Semaphore(const Semaphore& other) = delete;
	Semaphore& operator=(const Semaphore& other) = delete;

public:
	Semaphore(int initialCount = 0)
	{
		sem_init(&m_sema, 0, initialCount);
	}

	~Semaphore()
	{
		sem_destroy(&m_sema);
	}

	void wait()
	{
		// http://stackoverflow.com/questions/2013181/gdb-causes-sem-wait-to-fail-with-eintr-error
		int rc;
		do
		{
			rc = sem_wait(&m_sema);
		} while (rc == -1 && errno == EINTR);
	}

	void signal()
	{
		sem_post(&m_sema);
	}

	void signal(int count)
	{
		while (count-- > 0)
		{
			sem_post(&m_sema);
		}
	}
};


#else

#error Unsupported platform!

#endif

class silk__slim_semaphore {
	std::atomic<int> m_count;
	Semaphore m_sema;
public:
	silk__slim_semaphore(int initialCount = 0) : m_count(initialCount) {
	}

	void wait() {
		int oldCount = m_count.load(std::memory_order_relaxed);

		if ((oldCount > 0 && m_count.compare_exchange_strong(oldCount, oldCount - 1, std::memory_order_acquire)))
			return;
		
		int spin = 10000;

		while (spin--) {
			oldCount = m_count.load(std::memory_order_relaxed);

			if ((oldCount > 0) && m_count.compare_exchange_strong(oldCount, oldCount - 1, std::memory_order_acquire))
				return;
			
			std::atomic_signal_fence(std::memory_order_acquire);
		}

		oldCount = m_count.fetch_sub(1, std::memory_order_acquire);
		
		if (oldCount <= 0) {
			m_sema.wait();
		}
	}

	void signal(const int count = 1) {
		const int old_count = m_count.fetch_add(count, std::memory_order_release);
		const int to_release = -old_count < count ? -old_count : count;

		if (to_release > 0) {
			m_sema.signal(to_release);
		}
	}
};

class silk__auto_reset_event {
	// m_status == 1: Event object is signaled.
	// m_status == 0: Event object is reset and no threads are waiting.
	// m_status == -N: Event object is reset and N threads are waiting.
	std::atomic<int> m_status_;
	silk__slim_semaphore m_sema_;

public:
	silk__auto_reset_event() : m_status_(0) {
	}

	void signal(const int count = 1) {
		int old_status = m_status_.load(std::memory_order_relaxed);
		for (;;) {    // Increment m_status atomically via CAS loop.
		
			const int new_status = old_status < 1 ? old_status + 1 : 1;
			
			if (m_status_.compare_exchange_weak(old_status, new_status, std::memory_order_release, std::memory_order_relaxed))
				break;
			// The compare-exchange failed, likely because another thread changed m_status.
			// oldStatus has been updated. Retry the CAS loop.
		}

		if (old_status < 0)
			m_sema_.signal(count);    // Release one waiting thread.
	}

	void wait() {
		const int old_status = m_status_.fetch_sub(1, std::memory_order_acquire);

		if (old_status < 1) {
			m_sema_.wait();
		}
	}
};

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
silk__auto_reset_event* silk__sem = new silk__auto_reset_event();

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