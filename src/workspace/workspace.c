#include "ast/types.h"
#include "driver/types.h"
#include "files/files.h"
#include "files/types.h"
#include "ids.h"
#include "lexer/lexer.h"
#include "parser/directives/directives.h"
#include "parser/parser.h"
#include "string_interner/interner.h"
#include "threadpool/threadpool.h"
#include "utils/debug.h"
#include "utils/macros.h"
#include "utils/timer.h"
#include "utils/types.h"
#include "workspace/types.h"
#include "workspace/workspace.h"

#include <assert.h>
#include <linux/limits.h>
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

    file_interner_init(&ws -> file_interner);

    string_interner_init(&ws -> string_interner);

    standard_library_map_init(ws);

    /*
     *
     * used for AST parser, must be called after `string_interner_init()`
     *
     */
    init_directive_lut(ws);
}

void workspace_destroy(Workspace* ws) {
    assert(ws != null);

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
    assert(id < ws -> file_interner.count);
    return ws -> file_interner.entries[id];
}

/*
 *
 * main driver for workspaces
 *
 */

void compile_workspace(Workspace* ws) {
    timer_start(&ws -> frontend_timer);

    lex_and_parse_workspace(ws);

    timer_end(&ws -> frontend_timer);

    timer_start(&ws -> backend_timer);
    timer_end(&ws -> backend_timer);

    timer_start(&ws -> linker_timer);
    timer_end(&ws -> linker_timer);
}

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

    if (file -> stage == FILE_ALLOCATED) {
        lex_file(file);

        if (file -> stage == FILE_LEXED) {
            parse_file(file);
        }
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
