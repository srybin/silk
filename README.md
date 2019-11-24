# Silk
Silk is lightweight engine for creating task-based runtime. Silk define building blocks for tasks, thread's contexts, and task's containers. You can take

```C
silk__task
silk__wcontext
silk__wcontext** silk__wcontexts

and

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
2. [taskruntime2.h](examples/taskruntime2.h)/[main2.cpp](examples/main2.cpp), where tasks are based on continuation passing like in Intel Threading Building Blocks.
3. [taskruntime3.h](examples/taskruntime3.h)/[taskruntime4.h](examples/taskruntime4.h)/[main3.cpp](examples/main3.cpp), where tasks are coroutines (taskruntime3.h - thread-bound coroutines were after first calling, taskruntime4.h - not thread-bound coroutines).
4. [main4.cpp](examples/main4.cpp) implements simple TCP server (FreeBSD/kqueue/taskruntime2.h - continuation passing).
5. [main5.cpp](examples/main5.cpp) implements simple TCP server (FreeBSD/kqueue/taskruntime3.h/taskruntime4.h - coroutines).

## Roadmap:
- [ ] Separete silk.h on 2 files: silk.h and silk_pool.h because it is usefull take only task container primitifs for implementing own thread pool.
- [ ] Refactore lightweight semaphore implementation.
- [ ] Start 2 or more independed thread pools.
- [ ] Shotdown default thread pool.