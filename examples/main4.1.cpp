#include <stdio.h>
#include <unistd.h>
#include <iostream>
#include "./taskruntime4.1.h"

silk::demo_runtime_4_1::task<int> c2(const int i) {
	co_await silk::demo_runtime_4_1::yield();

	co_return i;
}

silk::demo_runtime_4_1::task<int> c3(const int i) {
	auto r = co_await c2(i);

	co_return r + 1;
}

silk::demo_runtime_4_1::task<int> c33(const int i) {
	return c3(i);
}

silk::demo_runtime_4_1::task<> c31() {
	silk::demo_runtime_4_1::task<int> c0 = c3(2);

	auto r = co_await c0;

	printf("%d c31() -> %d\n", silk::current_worker_id, r);
}

std::atomic<int> count;

silk::demo_runtime_4_1::independed_task c0() {
	co_await silk::demo_runtime_4_1::yield();

	silk::demo_runtime_4_1::task<int> c0 = c3( 2 );

	auto r0 = co_await c3( 1 );

	auto r1 = co_await c0;

	auto r2 = co_await c0;

	co_await c31();

	auto r3 = co_await c3( 3 );

	auto r4 = co_await c33( 4 );

	auto r5 = co_await c33( 5 );

	auto completes_synchronously = []() -> silk::demo_runtime_4_1::task<int> {
		co_return 1;
	};

	int r6 = co_await completes_synchronously();

	printf("%d c0() -> %d %d %d %d %d %d %d\n", silk::current_worker_id, r0, r1, r2, r3, r4, r5, r6);

	count.fetch_add(1, std::memory_order_acquire);
}

int main() {
	silk::init_pool(silk::demo_runtime_4_1::schedule, silk::makecontext);

	for (int i = 0; i < 1000000; i++) {
		silk::demo_runtime_4_1::spawn( c0() );
	}

	silk::join_main_thread_2_pool(silk::demo_runtime_4_1::schedule);
	
	sleep(3);

	printf("coros: %d\n", count.load());

	return 0;
}