#ifndef LILY_DRIVER_TYPES_H
#define LILY_DRIVER_TYPES_H

#include "diagnostics/types.h"
#include "threadpool/types.h"
#include "workspace/types.h"

typedef struct {
    Arena scratch_arena;

    ThreadPool thread_pool;

    DiagnosticEngine diagnostic_engine;

    Arena workspace_arena;

    Workspace** workspaces;
    u32 workspace_count;

    const char* home_directory;
} Driver;

#endif // !LILY_DRIVER_TYPES_H
