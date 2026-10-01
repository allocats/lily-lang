#include "ast/types.h"
#include "driver/types.h"
#include "files/files.h"
#include "files/types.h"
#include "ids.h"
#include "lexer/lexer.h"
#include "parser/directives/directives.h"
#include "parser/parser.h"
#include "string_interner/interner.h"
#include "symbols/table/table.h"
#include "threadpool/threadpool.h"
#include "types/table/table.h"
#include "utils/debug.h"
#include "utils/macros.h"
#include "utils/timer.h"
#include "utils/types.h"
#include "workspace/types.h"
#include "workspace/workspace.h"

#include <assert.h>
#include <linux/limits.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <threads.h>

extern Driver driver;

static void lex_and_parse_workspace(Workspace* ws);
static void standard_library_map_init(Workspace* ws);

/*
 *
 * Creation and deletion
 *
 */

void workspace_init(Workspace* ws, WorkspaceId id, const char* name) {
    assert(ws != null);

    ws -> id = id;
    ws -> name = STR8_FROM_CSTR(name);
    ws -> target_os = OS_LINUX;
    ws -> entry_point_id = SYMBOL_ID_NONE;

    file_interner_init(&ws -> file_interner);

    string_interner_init(&ws -> string_interner);

    /*
     *
     * used for AST parser, must be called after `string_interner_init()`
     *
     */
    init_directive_lut(&driver.string_interners[0]);

    standard_library_map_init(ws);

    type_table_init(&ws -> type_table);
}

void workspace_destroy(Workspace* ws) {
    assert(ws != null);

    type_table_destroy(&ws -> type_table);

    symbol_table_destory(&ws -> symbol_table);

    string_interner_destroy(&ws -> string_interner);

    file_interner_destroy(&ws -> file_interner);

    arena_destroy(&ws -> standard_library_map.arena);
}

/*
 *
 * helpers 
 *
 */

File* workspace_get_file(Workspace* ws, FileId id) {
    FileInterner* interner = &ws -> file_interner;
    
    pthread_mutex_lock(&interner -> mutex);

    assert(id < interner -> count);

    File* file = ws -> file_interner.entries[id];

    pthread_mutex_unlock(&interner -> mutex);

    return file;
}

/*
 *
 * main driver for workspaces
 *
 */

void compile_workspace(Workspace* ws) {
    timer_start(&ws -> frontend_timer);

    lex_and_parse_workspace(ws);

    u32 scope_count_estimate  = ws -> file_interner.count;
    u32 symbol_count_estimate = 0;

    for (u32 i = 0; i < ws -> file_interner.count; i += 1) {
        File* file = ws -> file_interner.entries[i];

        scope_count_estimate  += file -> ast.top_level_declaration_count;
        symbol_count_estimate += file -> ast.total_declaration_count;
    }

    symbol_table_init(&ws -> symbol_table, scope_count_estimate, symbol_count_estimate);

    timer_end(&ws -> frontend_timer);

    timer_start(&ws -> backend_timer);
    timer_end(&ws -> backend_timer);

    timer_start(&ws -> linker_timer);
    timer_end(&ws -> linker_timer);
}

/*
 *
 * Does wait for threads to finish
 *
 */
static void lex_and_parse_workspace(Workspace* ws) {
    u32 file_count = ws -> file_interner.count;

    for (u32 i = 0; i < file_count; i += 1) {
        File* file = ws -> file_interner.entries[i];

        threadpool_add_task(&driver.thread_pool, workspace_dispatch_lex_and_parse_file, file);
    }

    threadpool_wait(&driver.thread_pool);
}

void workspace_dispatch_lex_and_parse_file(void* arg) {
    File* file = (File*) arg;

    FileStage expected_stage = FILE_ALLOCATED;

    if (!atomic_compare_exchange_strong(&file -> stage, &expected_stage, FILE_LEXING)) {
        return;
    }

    lex_file(file);

    if (file -> stage == FILE_LEXED) {
        parse_file(file);
    }
}

/*
 *
 * Standard Library Mapping
 *
 * TODO: Perhaps make it its own interner, ideally in lily :p I could just 
 *       run code at compile time to calculate the number of entries and get 
 *       a list of said entries, and actually probably be able to create the whole 
 *       map for standard library at compile time. sighh
 *
 * TODO: Make this per thread? Need consistent IDs for it to function correctly
 *
 */

static void standard_library_map_init(Workspace* ws) {
    StandardLibraryList* list = &driver.standard_library_path_list;

    StandardLibraryMap* map = &ws -> standard_library_map;

    arena_init(&map -> arena, list -> count * 256, ALIGN_DEFAULT);

    map -> entries = arena_alloc(&map -> arena, sizeof(StandardLibraryEntry) * list -> count);
    map -> count = list -> count;

    for (u32 i = 0; i < list -> count; i += 1) {
        debug_log("Registering STD module: %.*s", STR8_FMT(list -> paths[i]));

        StringId string_id = string_intern_str8(&ws -> string_interner, list -> paths[i]);

        if (i == 0) {
            map -> range.start = string_id;
        } else {
            assert(string_id == map -> range.start + i);
        } 

        if (i == list -> count - 1) {
            map -> range.end = string_id;
        }

        static thread_local char scratch[PATH_MAX];

        i32 n = snprintf(
            scratch,
            sizeof(scratch),
            "%s/.local/lily/%.*s/module.lily",
            driver.home_directory,
            STR8_FMT(list -> paths[i])
        );

        debug_log("Mapping std module: %.*s", n, scratch);

        char* ptr = arena_alloc(&map -> arena, n + 1);

        snprintf(ptr, n + 1, "%.*s", n, scratch);

        str8 path = {
            .ptr = ptr,
            .len = n
        };

        map -> entries[i].path = path;
        map -> entries[i].file_id = file_intern(&ws -> file_interner, path);
    }

    debug_log("Set Workspace %u standard library", ws -> id);
}

FileId standard_library_map_get_module(StandardLibraryMap* map, StringId id) {
    if (id < map -> range.start || id > map -> range.end) {
        return FILE_ID_NONE;
    }

    u32 index = id - map -> range.start;

    assert(index < map -> count);

    return map -> entries[index].file_id;
}
