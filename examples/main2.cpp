#include <iostream>
#include "taskruntime2.h"

void c() {
	std::cout << "C task is executing in thread pool..." << std::endl;
}

void c1(const int i) {
	std::cout << "C" << i << " task is executing in thread pool..." << std::endl;
}

int main() {
	init_pool();

	spawn(c);

	for (int i = 0; i < 1000000; i++) {
		spawn([=]() { c1(i); });
	}

	join_main_thread_2_pool();

	return 0;
}