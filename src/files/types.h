#ifndef LILY_FILES_TYPES_H
#define LILY_FILES_TYPES_H

#include "ast/types.h"
#include "ids.h"
#include "meowrena/meowrena.h"
#include "tokens/types.h"
#include "utils/types.h"

#include <pthread.h>

typedef enum {
    FILE_ERROR = 0,
    FILE_ALLOCATED,
    FILE_LEXING,
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

    _Atomic(FileStage) stage;

    u32 path_hash;
    str8 path;
    str8 buffer;

    Ast ast;

    TokenArray tokens;

    ScopeId scope_id;

    u32 lines_of_code;

} __attribute__((aligned(64))) File;

typedef struct {
    FileBucket *buckets;
    File **entries; // we keep this an array of pointers for lifetimes!

    u32 bucket_capacity;
    u32 entry_capacity;

    u32 count;

    u32 resize_threshold_as_u32;

    pthread_mutex_t mutex;

    Arena interner_arena;
} FileInterner;

#endif // !LILY_FILES_TYPES_H
