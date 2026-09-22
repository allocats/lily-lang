#include "driver/driver.h"
#include "diagnostics/diagnostics.h"
#include "driver/types.h"
#include "files/files.h"
#include "threadpool/threadpool.h"
#include "workspace/types.h"
#include "workspace/workspace.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void dispatch_file_intern_thread(void* argument);

void driver_init(Driver* driver, const char* home) {
    assert(driver != null);

    diagnostic_engine_init();

    /*
    *
    * Threadpool
    *
    */
    i32 thread_count = sysconf(_SC_NPROCESSORS_ONLN);

    if (thread_count <= 0) {
        thread_count = 1; // default to 1 thread if sysconf fails
    }

    // This function can assert 
    threadpool_init(&driver -> thread_pool, thread_count);

    /*
    *
    * Workspaces
    *
    */
    arena_init(&driver -> workspace_arena, ARENA_KB(1), ALIGN_DEFAULT);

    // array of pointers because files can spawn new workspaces, lifetimes will be headache with threading 
    // if it was an array of structs as opposed to an array of pointers i think
    driver -> workspaces = arena_calloc(&driver -> workspace_arena, sizeof(Workspace*));
    driver -> workspace_count = 1;

    driver -> workspaces[0] = arena_alloc(&driver -> workspace_arena, sizeof(Workspace));

    workspace_init(driver -> workspaces[0], 0, "Lily Program <3");

    /*
     *
     * Home directory for standard library
     *
     */
    driver -> home_directory = home;
}

void driver_destroy(Driver* driver) {
    threadpool_destroy(&driver -> thread_pool);

    for (u32 i = 0; i < driver -> workspace_count; i++) {
        workspace_destroy(driver -> workspaces[i]);
    }

    arena_destroy(&driver -> workspace_arena);
}

/*
 *
 * Command line flag parsing, interns files
 * 
 */

bool driver_parse_flags(Driver* driver, i32 argc, char** argv) {
    bool result = true;

    for (i32 i = 0; i < argc; i++) {
        char* arg = argv[i];
        usize arg_len = strlen(arg);

        if (arg_len < 2) {
            // TODO: diagnostics
            printf("invalid argument\n");
            result = false;
            continue;
        }

        if (arg[0] == '-') {

        } else {
            if (arg_len < 6) {
                printf("invalid arg");
                result = false;
                continue;
            }

            str8 path = {
                .ptr = arg,
                .len = arg_len
            };

            // DispatchFileArg dispatch_arg = {
            //     .ws = driver -> workspaces[0],
            //     .path = path
            // };

            DispatchFileArg* dispatch_arg = malloc(sizeof(DispatchFileArg));

            dispatch_arg -> ws = driver -> workspaces[0];
            dispatch_arg -> path = path;

            threadpool_add_task(&driver -> thread_pool, dispatch_file_intern_thread, dispatch_arg);
        }
    }

    threadpool_wait(&driver -> thread_pool);

    return result;
}

static void dispatch_file_intern_thread(void* argument) {
    DispatchFileArg* arg = (DispatchFileArg*) argument;

    file_intern(&arg -> ws -> file_interner, arg -> path);

    free(arg);
}
