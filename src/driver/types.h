#ifndef LILY_DRIVER_TYPES_H
#define LILY_DRIVER_TYPES_H

#include "diagnostics/types.h"
#include "string_interner/local/types.h"
#include "symbols/table/local/types.h"
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
    Workspace** workspaces;
    u32 workspace_count;

    LocalStringInterner* local_string_interners;
    u8 local_string_interner_count;

    LocalSymbolTable* local_symbol_tables;
    u8 local_symbol_table_count;

    DiagnosticEngine* local_diagnostic_engines;
    u8 local_diagnostic_engine_count;

    ThreadPool thread_pool;

    const char* home_directory;
    const char* standard_library_directory;

    StandardLibraryList standard_library_path_list;

    Arena workspace_arena;
    Arena scratch_arena;
} Driver;

#endif // !LILY_DRIVER_TYPES_H
