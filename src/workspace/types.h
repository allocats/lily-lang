#ifndef LILY_WORKSPACE_TYPES_H
#define LILY_WORKSPACE_TYPES_H

#include "files/types.h"
#include "ids.h"
#include "string_interner/types.h"
#include "utils/timer.h"
#include "utils/types.h"

typedef struct {
    WorkspaceId id;
    str8 name;

    FileInterner file_interner;

    StringInterner string_interner;

    // Maybe maker timers per workspace
    Timer frontend_timer;
    Timer backend_timer;
    Timer linker_timer;

    u32 total_lines_of_code;
} Workspace;

#endif // !LILY_WORKSPACE_TYPES_H
