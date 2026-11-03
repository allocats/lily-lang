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

    /*
     *
     * The name of the workspace i.e.  "Lily Program <3"
     *
     * TODO: Need to expose the compiler an add functions to set this value
     * 
     */
    str8 name;

    /*
     *
     * Pretty self explanatory, just set the target output architecture
     *
     * TODO: have this affect LLVM codegen
     *
     */
    TargetOS target_os;

    /*
     *
     * The string interner for AFTER parsing, once all files are parsed the thread local 
     * interners are merged into one single `GlobalStringInterner`
     *
     */
    GlobalStringInterner global_string_interner;

    /*
     *
     * A map that maps string ids of `std/io` to actual `FileId`'s
     *
     */
    StandardLibraryMap standard_library_map;

    /*
     *
     * Interner for registered files, includes stdlib and any discovered files through `#include` and `#import`
     *
     */
    FileInterner file_interner;

    /*
     *
     * Flat array of the paths to the object files, it is either the thread count or file interner count
     *
     */
    str8 *object_paths;
    u32 object_path_count;

    /*
     *
     * The symbol table for AFTER the first pass, the first pass register all the top level symbols for each file in j
     * parallel into an array of `LocalSymbolTable`, afterwards a merge pass merges all the local symbols and 
     * resolves imports into one single global symbol table
     *
     */
    GlobalSymbolTable global_symbol_table;

    /*
     *
     * Type table
     *
     */
    TypeTable type_table;

    /*
     *
     * NOTE: this might be removed or moved
     *
     * Shorthand for primitive types
     *
     */
    BuiltinTypeIds builtin_types;

    /*
     *
     * looks for `main` function, going to make a rule that 
     * `main` cannot contain arguments nor a return type
     *
     */
    SymbolId entry_point_id;

    /*
     *
     * Timers for compiler stats
     *
     */
    Timer frontend_timer;
    Timer backend_timer;
    Timer linker_timer;

    u32 total_lines_of_code;
} Workspace;

#endif // !LILY_WORKSPACE_TYPES_H
