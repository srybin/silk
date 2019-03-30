#pragma once

#include "./../src/tpf.h"

inline void spawn(std::function<void()> t) {
	spawn(std::this_thread::get_id(), (void*)new std::function<void()>(t));
}

inline void custom_schedule(void* v) {
	std::function<void()>* t = (std::function<void()>*)v;
	
	(*t)();
	
	delete t;
}

inline void init_pool() {
	init_pool(custom_schedule, make_context);
}

inline void join_main_thread_2_pool() {
	join_main_thread_2_pool(custom_schedule);
}

inline void join_main_thread_2_pool_in_infinity_loop() {
	join_main_thread_2_pool_in_infinity_loop(custom_schedule);
}