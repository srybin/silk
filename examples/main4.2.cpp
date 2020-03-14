#include <stdio.h>
#include <unistd.h>
#include <iostream>
#include "./taskruntime4.2.h"

silk::demo_runtime_4_2::task<int> c2(const int i) {
	co_await silk::demo_runtime_4_2::yield();

	co_return i;
}

silk::demo_runtime_4_2::task<int> c3(const int i) {
	auto r = co_await c2(i);

	co_return r + 1;
}

silk::demo_runtime_4_2::task<int> c33(const int i) {
	return c3(i);
}

silk::demo_runtime_4_2::task<> c31() {
	silk::demo_runtime_4_2::task<int> c0 = c3(2);

	silk::demo_runtime_4_2::spawn(c0);

	auto r = co_await c0;

	printf("%d c31() -> %d\n", silk::current_worker_id, r);
}

std::atomic<int> count;

silk::demo_runtime_4_2::independed_task c0() {
	co_await silk::demo_runtime_4_2::yield();

	silk::demo_runtime_4_2::task<int> c0 = c3( 2 );

	silk::demo_runtime_4_2::spawn( c0 );

	auto r0 = co_await c3( 1 );

	auto r1 = co_await c0;

	co_await c31();

	silk::demo_runtime_4_2::task<> c01 = silk::demo_runtime_4_2::spawn( c31() );

	co_await c01;

	auto r2 = co_await c3( 3 );

	auto r3 = co_await c33( 4 );

	silk::demo_runtime_4_2::task<int> c1 = silk::demo_runtime_4_2::spawn( c33( 5 ) );

	auto r4 = co_await c1;

	auto completes_synchronously = []() -> silk::demo_runtime_4_2::task<int> {
		co_return 1;
	};

	int r5 = co_await completes_synchronously();
	
	//int r6 = co_await silk__spawn( completes_synchronously() );

	silk::demo_runtime_4_2::task<int> c2 = silk::demo_runtime_4_2::spawn( completes_synchronously() );

	int r6 = co_await c2;

	printf("%d c0() -> %d %d %d %d %d %d %d\n", silk::current_worker_id, r0, r1, r2, r3, r4, r5, r6);

	count.fetch_add(1, std::memory_order_acquire);
}

int main() {
	silk::init_pool(silk::demo_runtime_4_2::schedule, silk::makecontext);

	for (int i = 0; i < 1000000; i++) {
		silk::demo_runtime_4_2::spawn( c0() );
	}

	silk::join_main_thread_2_pool(silk::demo_runtime_4_2::schedule);
	
	sleep(3);

	printf("coros: %d\n", count.load());
	
	return 0;
}