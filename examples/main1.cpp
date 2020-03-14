#include <iostream>
#include "taskruntime1.h"

void c() {
	std::cout << "C task is executing in thread pool..." << std::endl;
}

void c1(const int i) {
	std::cout << "C" << i << " task is executing in thread pool..." << std::endl;
}

int main() {
	silk::init_pool(silk::demo_runtime_1::schedule, silk::makecontext);

	for (int i = 0; i < 30000; i++) {
		silk::demo_runtime_1::spawn( c );
	}

	for (int i = 0; i < 30000; i++) {
		silk::demo_runtime_1::spawn2( c1( i ) );
	}

	silk::join_main_thread_2_pool(silk::demo_runtime_1::schedule);

	return 0;
}