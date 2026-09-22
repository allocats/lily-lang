#ifndef LILY_THREADPOOL_TYPES_H
#define LILY_THREADPOOL_TYPES_H

#include <pthread.h>

#include "meowrena/meowrena.h"
#include "utils/types.h"

typedef struct ThreadTask ThreadTask;

struct ThreadTask{
    void (*fn)(void*);
    void* arg;

    ThreadTask* next;
};

typedef struct {
    ThreadTask* front;
    ThreadTask* rear;

    u32 count;
} ThreadTaskQueue;

typedef struct {
    Arena task_arena;

    pthread_t* threads;
    u32 thread_count;

    u32 active_tasks;
    bool shutdown;

    ThreadTaskQueue queue;

    pthread_mutex_t mutex;
    pthread_cond_t tasks_available;
    pthread_cond_t all_done;
} ThreadPool;

#endif // !LILY_THREADPOOL_TYPES_H
