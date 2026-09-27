#ifndef LILY_DIAGNOSTICS_TYPES_H
#define LILY_DIAGNOSTICS_TYPES_H

#include "files/types.h"
#include "meowrena/meowrena.h"
#include "utils/types.h"
#include "workspace/types.h"

#include <pthread.h>

static constexpr u8 DIAG_LOC_START_OF_TOK = (1 << 0);
static constexpr u8 DIAG_LOC_END_OF_TOK   = (1 << 1);
static constexpr u8 DIAG_LOC_WHOLE_TOK    = (1 << 2);

typedef enum {
    DIAG_ERROR,
    DIAG_WARNING,
    DIAG_NOTE
} DiagKind;

typedef struct {
    DiagKind kind;

    Workspace* workspace;
    FileId file_id;

    u32 line;
    u32 col;
    u32 len;

    str8 msg;
    str8 help;

    bool is_generic;
} Diagnostic;

typedef struct {
    // mostly for strings and array
    Arena arena;

    // TODO: Add support to read from flags
    u32 threshold_value;

    u32 error_count;

    u32 count;
    u32 capacity;
    Diagnostic* diags;

    pthread_mutex_t mutex;
} DiagnosticEngine;

#endif // !LILY_DIAGNOSTICS_TYPES_H
