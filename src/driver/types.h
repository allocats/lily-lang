#ifndef LILY_DRIVER_TYPES_H
#define LILY_DRIVER_TYPES_H

#include "diagnostics/types.h"
#include "threadpool/types.h"
#include "utils/types.h"
#include "workspace/types.h"

typedef struct {
    Arena arena;

    str8* paths;
    u32 count;
    u32 capacity;
} StandardLibraryList;

typedef struct {
    Arena scratch_arena;

    ThreadPool thread_pool;

    DiagnosticEngine diagnostic_engine;

    Arena workspace_arena;

    Workspace** workspaces;
    u32 workspace_count;

    const char* home_directory;
    const char* standard_library_directory;

    StandardLibraryList standard_library_path_list;
} Driver;

#endif // !LILY_DRIVER_TYPES_H
