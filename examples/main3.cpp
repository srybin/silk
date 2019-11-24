#define _XOPEN_SOURCE 600
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "./taskruntime3.h"
//#include "./taskruntime4.h"

void silk__read(int i) {
    printf("%d silk__read(%d): starting\n", silk__current_worker_id, i);

    int r = i + 1;

    silk__yield
    
    printf("%d silk__read(%d): returning [%d]\n", silk__current_worker_id, i, r);
}

int main() {
    silk__init_pool(silk__schedule, silk__makeuwcontext);

    int frames_count = 10000;

    silk__coro_frame** frames = (silk__coro_frame**) malloc(frames_count * sizeof(silk__coro_frame*));

    for (int i = 0; i < frames_count; i++) {
        frames[i] = silk__spawn(silk__coro silk__read, 32768, 1, i);
    }

    silk__join_main_thread_2_pool(silk__schedule);

    sleep(5);

    for (int i = 0; i < frames_count; i++) {
        silk__resume(frames[i]);
    }

    silk__join_main_thread_2_pool(silk__schedule);
    
    printf("main: returning\n");

    free(frames);

    return 0;
}