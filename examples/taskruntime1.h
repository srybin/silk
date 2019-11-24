#pragma once

#include "./../src/silk.h"
#include <functional>

typedef struct silk__func_t : silk__task {
	std::function<void()>* func;
} silk__func;

inline void silk__spawn(std::function<void()> t) {
	silk__func* f = new silk__func();

	f->func = new std::function<void()>(t);

	f->prev = f->next = nullptr;

	silk__spawn(silk__current_worker_id, f);
}

void silk__schedule(silk__task* t) {
	silk__func* func_container = (silk__func*)t;
	
	std::function<void()>* f = func_container->func;
	
	(*f)();
	
	free(f);

	free(t);
}