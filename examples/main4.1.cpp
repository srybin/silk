#include <stdio.h>
#include <unistd.h>
#include <iostream>
#include "./taskruntime4.1.h"

silk__coro<int> c2(const int i) {
	co_await silk__yield();

	co_return i;
}

silk__coro<int> c3(const int i) {
	auto r = co_await c2(i);

	co_return r + 1;
}

silk__coro<int> c33(const int i) {
	return c3(i);
}

silk__coro<> c31() {
	silk__coro<int> c0 = c3(2);

	auto r = co_await c0;

	printf("%d c31() -> %d\n", silk__current_worker_id, r);
}

std::atomic<int> count;

silk__independed_coro c0() {
	co_await silk__yield();

	silk__coro<int> c0 = c3( 2 );

	auto r0 = co_await c3( 1 );

	auto r1 = co_await c0;

	auto r2 = co_await c0;

	co_await c31();

	auto r3 = co_await c3( 3 );

	auto r4 = co_await c33( 4 );

	auto r5 = co_await c33( 5 );

	auto completes_synchronously = []() -> silk__coro<int> {
		co_return 1;
	};

	int r6 = co_await completes_synchronously();

	printf("%d c0() -> %d %d %d %d %d %d %d\n", silk__current_worker_id, r0, r1, r2, r3, r4, r5, r6);

	count.fetch_add(1, std::memory_order_acquire);
}

int main() {
	silk__init_pool(silk__schedule, silk__makecontext);

	for (int i = 0; i < 1000000; i++) {
		silk__spawn( c0() );
	}

	silk__join_main_thread_2_pool(silk__schedule);
	
	sleep(3);

	printf("coros: %d\n", count.load());

	return 0;
}