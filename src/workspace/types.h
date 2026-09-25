#ifndef LILY_WORKSPACE_TYPES_H
#define LILY_WORKSPACE_TYPES_H

#include "files/types.h"
#include "ids.h"
#include "string_interner/types.h"
#include "utils/timer.h"
#include "utils/types.h"

/*
 *
 * maps StringId -> path to that module
 *
 * Example:
 *   intern("std/io") -> /home/angel/.local/lily/std/io/module.lily
 *
 */
typedef struct {
    Arena arena;

    /*
     *
     *  indexing by StringId - range.start
     *
     */
    str8* paths;
    u32 count;

    /*
     *
     * range.start = lowest StringId
     * range.end = highest StringId
     *
     */
    SpanU32 range;
} StandardLibraryMap;

/*
 *
 * TODO: need to make this configurable and actually impact things!
 *
 */
typedef enum {
    OS_LINUX,
    OS_WINDOWS,
} TargetOS;

typedef struct {
    WorkspaceId id;
    str8 name;

    TargetOS target_os;

    StandardLibraryMap standard_library_map;

    FileInterner file_interner;

    StringInterner string_interner;

    // Maybe maker timers per workspace
    Timer frontend_timer;
    Timer backend_timer;
    Timer linker_timer;

    u32 total_lines_of_code;
} Workspace;

#endif // !LILY_WORKSPACE_TYPES_H
