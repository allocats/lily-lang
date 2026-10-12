#ifndef LILY_WORKSPACE_TYPES_H
#define LILY_WORKSPACE_TYPES_H

#include "files/types.h"
#include "ids.h"
#include "string_interner/global/types.h"
#include "symbols/table/global/types.h"
#include "types/builtins/types.h"
#include "types/table/types.h"
#include "utils/timer.h"
#include "utils/types.h"

typedef struct {
    str8 path;
    FileId file_id;
} StandardLibraryEntry;

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
    StandardLibraryEntry *entries;
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

    GlobalStringInterner global_string_interner;

    StandardLibraryMap standard_library_map;

    FileInterner file_interner;

    GlobalSymbolTable global_symbol_table;

    TypeTable type_table;

    BuiltinTypes builtin_types;

    /*
     *
     * looks for `main` function, going to make a rule that 
     * `main` cannot contain arguments nor a return type
     *
     */
    SymbolId entry_point_id;

    // Maybe maker timers per workspace
    Timer frontend_timer;
    Timer backend_timer;
    Timer linker_timer;

    u32 total_lines_of_code;
} Workspace;

#endif // !LILY_WORKSPACE_TYPES_H
