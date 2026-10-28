#ifndef LILY_THREADPOOL_TYPES_H
#define LILY_THREADPOOL_TYPES_H

#include <pthread.h>

#include "codegen/types.h"
#include "diagnostics/types.h"
#include "meowrena/meowrena.h"
#include "string_interner/local/types.h"
#include "symbols/table/local/types.h"
#include "utils/types.h"

typedef struct ThreadTask ThreadTask;

struct ThreadTask {
    void (*fn)(void *);
    void *arg;

    ThreadTask *next;
};

typedef struct {
    ThreadTask *front;
    ThreadTask *rear;

    u32 count;
} ThreadTaskQueue;

typedef struct {
    Arena task_arena;

    pthread_t *threads;
    u32 thread_count;

    u32 active_tasks;
    bool shutdown;

    ThreadTaskQueue queue;

    pthread_mutex_t mutex;
    pthread_cond_t tasks_available;
    pthread_cond_t all_done;
} ThreadPool;

typedef struct {
    ThreadPool *pool;
    u32 index;

    LocalStringInterner *string_interner;

    LocalSymbolTable *symbol_table;

    DiagnosticEngine *diagnostic_engine;

    CodegenCtx *codegen_ctx;
} ThreadContext;

#endif // !LILY_THREADPOOL_TYPES_H
