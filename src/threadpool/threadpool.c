#include "driver/types.h"
#include "meowrena/meowrena.h"
#include "threadpool/threadpool.h"
#include "threadpool/types.h"

#include <assert.h>
#include <pthread.h>
#include <threads.h>

thread_local ThreadContext thread_context;

static void* threadpool_worker(void* arg) {
    ThreadContext* ctx = (ThreadContext*) arg;

    thread_context = *ctx;

    free(ctx);

    ThreadPool* pool = thread_context.pool;

    while (true) {
        pthread_mutex_lock(&pool -> mutex);

        while (pool -> queue.count == 0 && !pool -> shutdown) {
            pthread_cond_wait(&pool -> tasks_available, &pool -> mutex);
        }

        if (pool -> shutdown && pool -> queue.count == 0) {
            pthread_mutex_unlock(&pool -> mutex);
            break;
        }

        ThreadTask* task = pool -> queue.front;
        pool -> queue.front = task -> next;
        
        if (pool -> queue.front == null) {
            pool -> queue.rear = null;
        }

        pool -> queue.count -= 1;

        pthread_mutex_unlock(&pool -> mutex);

        task -> fn(task -> arg);

        pthread_mutex_lock(&pool -> mutex);

        pool -> active_tasks -= 1;

        if (pool -> active_tasks == 0) {
            arena_reset(&pool -> task_arena);

            pthread_cond_signal(&pool -> all_done);
        }

        pthread_mutex_unlock(&pool -> mutex);
    }

    return null;
}

void threadpool_init(Driver* driver, u32 count) {
    assert(count > 0);

    ThreadPool* pool = &driver -> thread_pool;

    arena_init(&pool -> task_arena, ARENA_KB(4), ALIGN_DEFAULT);

    pool -> threads = calloc(count, sizeof(pthread_t));
    pool -> thread_count = count;

    assert(pool -> threads);

    pool -> active_tasks = 0;
    pool -> shutdown = false;

    pool -> queue.count = 0;
    pool -> queue.front = null;
    pool -> queue.rear  = null;
    
    pthread_mutex_init(&pool -> mutex, null);
    pthread_cond_init(&pool -> tasks_available, null);
    pthread_cond_init(&pool -> all_done, null);

    for (u32 i = 0; i < count; i++) {
        ThreadContext *ctx = malloc(sizeof(ThreadContext));

        assert(ctx != null);

        *ctx = (ThreadContext) {
            .pool = pool,
            .index = i,
            .string_interner = &driver -> string_interners[i]
        };

        pthread_create(&pool -> threads[i], null, threadpool_worker, ctx);
    }
}

void threadpool_destroy(ThreadPool* pool) {
    pthread_mutex_lock(&pool -> mutex);

    pool -> shutdown = 1;

    pthread_cond_broadcast(&pool -> tasks_available);
    pthread_mutex_unlock(&pool -> mutex);
 
    for (u32 i = 0; i < pool -> thread_count; i++) {
        pthread_join(pool -> threads[i], null);
    }

    pthread_mutex_destroy(&pool -> mutex);

    pthread_cond_destroy(&pool -> tasks_available);
    pthread_cond_destroy(&pool -> all_done);

    arena_destroy(&pool -> task_arena);

    free(pool -> threads);
}

void threadpool_add_task(ThreadPool* pool, void (*fn)(void*), void* arg) {
    pthread_mutex_lock(&pool -> mutex);

    ThreadTask* task = arena_alloc(&pool -> task_arena, sizeof(ThreadTask));

    task -> next = null;
    task -> arg  = arg;
    task -> fn   = fn;

    if (pool -> queue.rear != null) {
        pool -> queue.rear -> next = task;
    } else {
        pool -> queue.front = task;
    }

    pool -> queue.rear = task;
    pool -> queue.count += 1;
    pool -> active_tasks += 1;

    pthread_cond_signal(&pool -> tasks_available);
    pthread_mutex_unlock(&pool -> mutex);
}

void threadpool_wait(ThreadPool* pool) {
    pthread_mutex_lock(&pool -> mutex);

    while (pool -> active_tasks > 0) {
        pthread_cond_wait(&pool -> all_done, &pool -> mutex);
    }

    pthread_mutex_unlock(&pool -> mutex);
}
