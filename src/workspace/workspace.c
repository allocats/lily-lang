#include "driver/types.h"
#include "files/files.h"
#include "files/types.h"
#include "ids.h"
#include "lexer/lexer.h"
#include "string_interner/interner.h"
#include "threadpool/threadpool.h"
#include "utils/timer.h"
#include "utils/types.h"
#include "workspace/types.h"
#include "workspace/workspace.h"

#include <assert.h>

extern Driver driver;

static void lex_and_parse_workspace(Workspace* ws);
static void dispatch_lex_and_parse_file(void* arg);

/*
 *
 * Creation and deletion
 *
 */

void workspace_init(Workspace* ws, WorkspaceId id, const char* name) {
    assert(ws != null);

    ws -> id = id;
    ws -> name = STR8_FROM_CSTR(name);

    file_interner_init(&ws -> file_interner);

    string_interner_init(&ws -> string_interner);
}

void workspace_destroy(Workspace* ws) {
    assert(ws != null);

    string_interner_destroy(&ws -> string_interner);

    file_interner_destroy(&ws -> file_interner);
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

        threadpool_add_task(&driver.thread_pool, dispatch_lex_and_parse_file, file);
    }

    threadpool_wait(&driver.thread_pool);
}

static void dispatch_lex_and_parse_file(void* arg) {
    File* file = (File*) arg;

    if (file -> stage == FILE_ALLOCATED) {
        lex_file(file);

        if (file -> stage == FILE_LEXED) {
            // parse_file(file);
        }
    }
}
