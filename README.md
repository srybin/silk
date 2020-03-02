# Silk
Silk is lightweight engine for creating task-based runtime. Silk define building blocks for tasks, thread's contexts, and task's containers. You can take

```C
silk__task
silk__wcontext
silk__wcontext** silk__wcontexts
```

and

```C++
silk__spawn()
silk__fetch()
silk__steal()
silk__enqueue()
silk__spawn_affinity()
silk__fetch_affinity()
silk__enqueue_affinity()
```

primitivs to implement your own task-based runtime. Silk does not know about tasks details and how execute them or other specific scheduling details.
Also Silk implement simple and lightweight thread pool. Each OS thread has its own task dequeue and use work-stealing balansing strategy. Each OS thread in the thread pool can only spawn tasks in local dequeue like in a stack and fetch them from local dequeue like from stack. If local dequeue does not have any task, thread try to steal task from other random thread. Also eache OS thread has afinity dequeue for tasks witch others OS threads can not to steal.
Task-based runtime have to define its own schedule func and task type that inherits from the type silk__task:

```C
typedef struct task_t : public silk__task {
    ...
} task;

void schedule(silk__task* st) {
    task* t = (task*) st;
   
    //execute t
}
```

To initialize thread pool and run all OS threads you can call:

```C
silk__init_pool(schedule, make_wcontext); 
/*
or silk__init_pool(schedule, make_wcontext, 4) where 4 - threads count in the thread pool.
std::thread::hardware_concurrency() - default threads count.
*/
```

Function make_wcontext() is default function to make context for OS thread. If you want to expand the context you can define your own context type that inherits from the type silk__wcontext and define function to make this context.
OS thread which call silk__init_pool() also are included to the thread pool. He can spawn tasks and join to thread pool by calling silk__join_main_thread_2_pool() or silk__join_main_thread_2_pool_in_infinity_loop() to execute tasks.

## Examples:
Directory "examples" has 3 examples of task-based runtime:

1. [taskruntime1.h](examples/taskruntime1.h)/[main1.cpp](examples/main1.cpp), where tasks are functions.
2. [taskruntime2.h](examples/taskruntime2.h)/[main2.1.cpp](examples/main2.1.cpp), where tasks are based on continuation passing like in Intel Threading Building Blocks.
3. [taskruntime2.h](examples/taskruntime2.h)/[main2.2.cpp](examples/main2.2.cpp) implement simple TCP server (FreeBSD/kqueue/taskruntime2.h - continuation passing).
4. [taskruntime3.1.h](examples/taskruntime3.1.h)/[taskruntime3.2.h](examples/taskruntime3.2.h)/[main3.1.cpp](examples/main3.1.cpp) implement simple TCP server with only async read, where tasks are stackfull coroutines (taskruntime3.2.h - thread-bound coroutines were after first calling, taskruntime3.1.h - not thread-bound coroutines).
5. [taskruntime3.1.h](examples/taskruntime3.1.h)/[taskruntime3.2.h](examples/taskruntime3.2.h)/[main3.2cpp](examples/main3.2.cpp) implement simple TCP server with async accept and async read, where tasks are stackfull coroutines (taskruntime3.2.h - thread-bound coroutines were after first calling, taskruntime3.1.h - not thread-bound coroutines).
6. [taskruntime4.1.h](examples/taskruntime4.1.h)/[main4.1.cpp](examples/main4.1.cpp), where tasks are coroutines TS. Each coroutine does not start until the coroutine is awaited like in cppcoro.
7. [taskruntime4.2.h](examples/taskruntime4.2.h)/[main4.2.cpp](examples/main4.2.cpp), where tasks are coroutines TS. Each coroutine can be spawned via co_await or using spawn function for other courutine and later wait to end of spawned coroutine.
8. [taskruntime4.3.h](examples/taskruntime4.3.h)/[main4.3.cpp](examples/main4.3.cpp), where tasks are coroutines TS. Each coroutine start immediately and when child coroutine suspend, parent coroutine gets control back. 
9. [taskruntime4.2.h](examples/taskruntime4.2.h)/[main4.4.cpp](examples/main4.4.cpp) implement simple TCP server (FreeBSD/kqueue) with async accept and async read, where tasks are coroutines TS with spawn function.
10. [taskruntime4.3.h](examples/taskruntime4.3.h)/[main4.5.cpp](examples/main4.5.cpp) implement simple TCP server (FreeBSD/kqueue) with async accept and async read, where task is coroutines TS without spawn function.
11. [taskruntime4.3.h](examples/taskruntime4.3.h)/[main4.6.cpp](examples/main4.6.cpp) implement simple TCP server and client with event loop (FreeBSD/kqueue), where task is coroutines TS.

## Roadmap:
- [x] Separete silk.h on 2 files: silk.h and silk_pool.h because it is usefull take only task container primitifs for implementing own thread pool.
- [x] Refactore slim semaphore implementation.
- [ ] Start 2 or more independed thread pools.
- [ ] Shotdown default thread pool.
