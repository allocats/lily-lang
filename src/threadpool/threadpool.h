#ifndef LILY_THREADPOOL_H
#define LILY_THREADPOOL_H

#include "threadpool/types.h"

void threadpool_init(ThreadPool* pool, u32 count);
void threadpool_destroy(ThreadPool* pool);

void threadpool_add_task(ThreadPool* pool, void (*fn)(void*), void* arg);
void threadpool_wait(ThreadPool* pool);

#endif // !LILY_THREADPOOL_H
