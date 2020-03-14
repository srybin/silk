#pragma once

#include "./../src/silk_pool.h"
#include <functional>

namespace silk {
    namespace demo_runtime_1 {
        struct func : silk::task {
        	std::function<void()>* func;
        };
        
        inline void spawn(std::function<void()> t) {
        	func* f = new func();
        
        	f->func = new std::function<void()>(t);
        
        	silk::spawn(silk::current_worker_id, (silk::task*) f);
		}
        
        #define spawn2( ex ) spawn([=]() { ex; })
        
        void schedule(silk::task* t) {
        	func* func_container = (func*)t;
        	
        	std::function<void()>* f = func_container->func;
        	
        	(*f)();
        	
        	delete f;
        
        	delete t;
		}
    }
}