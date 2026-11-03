#include "workspace/workspace.h"
#include "ast/types.h"
#include "codegen/codegen.h"
#include "diagnostics/diagnostics.h"
#include "diagnostics/types.h"
#include "driver/types.h"
#include "files/files.h"
#include "files/types.h"
#include "ids.h"
#include "lexer/lexer.h"
#include "linker/linker.h"
#include "parser/directives/directives.h"
#include "parser/parser.h"
#include "string_interner/global/interner.h"
#include "string_interner/local/interner.h"
#include "symbols/register/register.h"
#include "symbols/resolve/merge/merge.h"
#include "symbols/resolve/resolve.h"
#include "symbols/table/global/table.h"
#include "symbols/table/local/table.h"
#include "symbols/table/local/types.h"
#include "threadpool/threadpool.h"
#include "types/builtins/builtins.h"
#include "types/table/table.h"
#include "utils/debug.h"
#include "utils/macros.h"
#include "utils/timer.h"
#include "utils/types.h"

#include <assert.h>
#include <linux/limits.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>

extern Driver driver;

static void lex_and_parse_workspace(Workspace *ws);
static void register_top_level_symbols_for_workspace(Workspace *ws);
static void dispatch_top_level_symbol_registration(void *arg);
static void standard_library_map_init(Workspace *ws);

typedef struct {
    Workspace *ws;
    File *file;
} DispatchArg;

/*
 *
 * Creation and deletion
 *
 */

void workspace_init(Workspace *ws, WorkspaceId id, const char *name) {
    assert(ws != null);

    ws->id             = id;
    ws->name           = STR8_FROM_CSTR(name);
    ws->target_os      = OS_LINUX;
    ws->entry_point_id = GLOBAL_SYMBOL_ID_NONE;

    file_interner_init(&ws->file_interner);

    /*
     *
     * used for AST parser, must be called after `string_interner_init()`
     *
     */
    init_directive_lut(&driver.local_string_interners[0]);

    standard_library_map_init(ws);

    type_table_init(&ws->type_table);
}

void workspace_destroy(Workspace *ws) {
    assert(ws != null);

    type_table_destroy(&ws->type_table);

    global_symbol_table_destroy(&ws->global_symbol_table);

    file_interner_destroy(&ws->file_interner);

    global_string_interner_destroy(&ws->global_string_interner);

    arena_destroy(&ws->standard_library_map.arena);

    free(ws->object_paths);
}

/*
 *
 * helpers 
 *
 */

File *workspace_get_file(Workspace *ws, FileId id) {
    FileInterner *interner = &ws->file_interner;

    pthread_mutex_lock(&interner->mutex);

    assert(id < interner->count);

    File *file = ws->file_interner.entries[id];

    pthread_mutex_unlock(&interner->mutex);

    return file;
}

/*
 *
 * main driver for workspaces
 *
 */

void compile_workspace(Workspace *ws) {
    timer_start(&ws->frontend_timer);

    lex_and_parse_workspace(ws);

    /*
     *
     * note: this function frees the local string interners
     *
     */
    global_string_interner_init(&ws->global_string_interner);

    /*
     *
     * get estimates and tallies of number of scopes and symbols in the target workspace
     *
     */

    u32 scope_count_estimate  = ws->file_interner.count;
    u32 symbol_count_estimate = 0;

    for (u32 i = 0; i < ws->file_interner.count; i += 1) {
        File *file = ws->file_interner.entries[i];

        scope_count_estimate += file->ast.top_level_declaration_count;
        symbol_count_estimate += file->ast.total_declaration_count;
    }

    /*
     *
     * local symbol tables, we do this here to avoid as many reallocs as possible for the rest of symbol resolution
     *
     */

    u32 local_scope_count_estimate  = scope_count_estimate / driver.local_symbol_table_count;
    u32 local_symbol_count_estimate = symbol_count_estimate / driver.local_symbol_table_count;

    for (u8 i = 0; i < driver.local_symbol_table_count; i += 1) {
        LocalSymbolTable *local_table = &driver.local_symbol_tables[i];

        local_symbol_table_init(local_table, local_scope_count_estimate, local_symbol_count_estimate, i);

        // register builtin types
        local_symbol_table_register_builtins(local_table, &ws->global_string_interner);
    }

    register_top_level_symbols_for_workspace(ws);

    /*
     *
     * create the global symbol table, and performs the merge from local tables into one global table 
     *
     * note: this frees the local symbol tables
     *
     */
    global_symbol_table_init(&ws->global_symbol_table);

    // register builtin types
    global_symbol_table_register_builtins(&ws->global_symbol_table, &ws->global_string_interner);

    /*
     *
     * Update files to use the new global scope id
     *
     */
    for (u32 i = 0; i < ws->file_interner.count; i += 1) {
        File *file = ws->file_interner.entries[i];

        if (file->stage != FILE_PARSED) {
            timer_end(&ws->frontend_timer);
            return;
        }

        file->scope_id = global_scope_from_local_scope(file->scope_id);
    }

    /*
     *
     * first pass
     *
     */
    resolve_imports(ws);

    /*
     *
     * Register all the builtin types into the type table
     *
     */
    type_table_register_builtin_types(
        &ws->type_table,
        &ws->global_string_interner,
        &ws->global_symbol_table,
        &ws->builtin_types
    );

    if (!resolve_symbols(ws)) {
        timer_end(&ws->frontend_timer);
        return;
    }

    timer_end(&ws->frontend_timer);

    timer_start(&ws->backend_timer);

    if (!codegen(ws)) {
        timer_end(&ws->backend_timer);
        return;
    }

    timer_end(&ws->backend_timer);

    timer_start(&ws->linker_timer);

    if (linker_run(ws) != 0) {
        diagnostic_add_generic(
            &driver.global_diagnostic_engine,
            DIAG_ERROR,
            "Linker failed!"
        );
    }

    timer_end(&ws->linker_timer);
}

/*
 *
 * Does wait for threads to finish
 *
 */
static void lex_and_parse_workspace(Workspace *ws) {
    u32 file_count = ws->file_interner.count;

    for (u32 i = 0; i < file_count; i += 1) {
        File *file = ws->file_interner.entries[i];

        threadpool_add_task(&driver.thread_pool, workspace_dispatch_lex_and_parse_file, file);
    }

    threadpool_wait(&driver.thread_pool);
}

void workspace_dispatch_lex_and_parse_file(void *arg) {
    File *file = (File *)arg;

    FileStage expected_stage = FILE_ALLOCATED;

    if (!atomic_compare_exchange_strong(&file->stage, &expected_stage, FILE_LEXING)) {
        return;
    }

    lex_file(file);

    if (file->stage == FILE_LEXED) {
        parse_file(file);
    }
}

/*
 *
 * Symbol registration and resolution
 *
 */

static void register_top_level_symbols_for_workspace(Workspace *ws) {
    u32 file_count = ws->file_interner.count;

    for (u32 i = 0; i < file_count; i += 1) {
        File *file = ws->file_interner.entries[i];

        DispatchArg *arg = arena_alloc(&driver.scratch_arena, sizeof(DispatchArg));

        arg->ws   = ws;
        arg->file = file;

        threadpool_add_task(&driver.thread_pool, dispatch_top_level_symbol_registration, arg);
    }

    threadpool_wait(&driver.thread_pool);

    arena_reset(&driver.scratch_arena);
}

static void dispatch_top_level_symbol_registration(void *arg) {
    DispatchArg *dispatched_arg = (DispatchArg *)arg;

    if (dispatched_arg->file->stage == FILE_PARSED) {
        symbols_register_top_level(dispatched_arg->ws, dispatched_arg->file);
    }
}

/*
 *
 * Standard Library Mapping
 *
 */

static void standard_library_map_init(Workspace *ws) {
    StandardLibraryList *list = &driver.standard_library_path_list;

    StandardLibraryMap *map = &ws->standard_library_map;

    arena_init(&map->arena, list->count * 256, ALIGN_DEFAULT);

    map->entries = arena_alloc(&map->arena, sizeof(StandardLibraryEntry) * list->count);
    map->count   = list->count;

    for (u32 i = 0; i < list->count; i += 1) {
        debug_log("Registering STD module: %.*s", STR8_FMT(list->paths[i]));

        StringId string_id = local_string_lookup_str8(&driver.local_string_interners[0], list->paths[i]);

        string_id = local_string_get_id(string_id);

        if (i == 0) {
            map->range.start = string_id;
        } else {
            assert(string_id == map->range.start + i);
        }

        if (i == list->count - 1) {
            map->range.end = string_id;
        }

        static thread_local char scratch[PATH_MAX];

        i32 n = snprintf(
            scratch,
            sizeof(scratch),
            "%s/.local/lily/%.*s/module.lily",
            driver.home_directory,
            STR8_FMT(list->paths[i])
        );

        debug_log("Mapping std module: %.*s", n, scratch);

        char *ptr = arena_alloc(&map->arena, n + 1);

        snprintf(ptr, n + 1, "%.*s", n, scratch);

        str8 path = {
            .ptr = ptr,
            .len = n
        };

        map->entries[i].path    = path;
        map->entries[i].file_id = file_intern(&ws->file_interner, path);
    }

    debug_log("Set Workspace %u standard library", ws->id);
}

FileId standard_library_map_get_module(StandardLibraryMap *map, StringId id) {
    StringId local_id = local_string_get_id(id);

    if (local_id < map->range.start || local_id > map->range.end) {
        return FILE_ID_NONE;
    }

    u32 index = local_id - map->range.start;

    assert(index < map->count);

    return map->entries[index].file_id;
}
