#ifndef LILY_FILES_TYPES_H
#define LILY_FILES_TYPES_H

#include "ids.h"
#include "meowrena/meowrena.h"
#include "tokens/types.h"
#include "utils/types.h"

#include <pthread.h>

typedef enum {
    FILE_ERROR = 0,
    FILE_ALLOCATED,
    FILE_LEXED,
    FILE_PARSED,
    FILE_TYPECHECKED,
    FILE_CODEGENED,
    FILE_DONE,
} FileStage;

typedef struct {
    FileId id;
    u32 hash;
} FileBucket;

typedef struct {
    WorkspaceId workspace_id;
    FileId id;

    FileStage stage;

    u32  path_hash;
    str8 path;
    str8 buffer;

    TokenArray tokens;
    SourceLocationArray source_locations;

    u32 lines_of_code;
} File;

typedef struct {
    Arena buffer_arena;

    Arena interner_arena;

    FileBucket* buckets;
    File** entries; // we keep this an array of pointers for lifetimes!

    u32 bucket_capacity;
    u32 entry_capacity;

    u32 count;

    u32 resize_threshold_as_u32;

    pthread_mutex_t mutex;
} FileInterner;

#endif // !LILY_FILES_TYPES_H
