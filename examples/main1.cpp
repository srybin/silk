#include <iostream>
#include "taskruntime1.h"

void c() {
	std::cout << "C task is executing in thread pool..." << std::endl;
}

void c1(const int i) {
	std::cout << "C" << i << " task is executing in thread pool..." << std::endl;
}

int main() {
	silk__init_pool(silk__schedule, silk__makecontext);

	for (int i = 0; i < 30000; i++) {
		silk__spawn( c );
	}

	for (int i = 0; i < 30000; i++) {
		silk__spawn2( c1( i ) );
	}

	silk__join_main_thread_2_pool(silk__schedule);

	return 0;
}