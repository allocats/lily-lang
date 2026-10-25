#include "codegen/codegen.h"
#include "ast/types.h"
#include "driver/types.h"
#include "threadpool/threadpool.h"
#include "workspace/types.h"

extern Driver driver;

static void codegen_file(Workspace *ws, File *file);
static void dispatch_codegen_file(void *argument);

typedef struct {
    Workspace *ws;
    File *file;
} Arg;

void codegen(Workspace *ws) {
    for (u32 i = 0; i < ws->file_interner.count; i += 1) {
        File *file = ws->file_interner.entries[i];

        Arg *arg = arena_alloc(&driver.scratch_arena, sizeof(Arg));

        arg->ws   = ws;
        arg->file = file;

        threadpool_add_task(&driver.thread_pool, dispatch_codegen_file, arg);
    }

    threadpool_wait(&driver.thread_pool);

    arena_reset(&driver.scratch_arena);
}

static void codegen_file(Workspace *ws, File *file) {
}

/*
 *
 * Thread dispatcher
 *
 */
static void dispatch_codegen_file(void *argument) {
    Arg *arg = (Arg *)argument;

    Workspace *ws = arg->ws;
    File *file    = arg->file;

    codegen_file(ws, file);
}
