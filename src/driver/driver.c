#include "driver/driver.h"
#include "ast/types.h"
#include "diagnostics/diagnostics.h"
#include "driver/types.h"
#include "files/files.h"
#include "ids.h"
#include "string_interner/interner.h"
#include "threadpool/threadpool.h"
#include "utils/debug.h"
#include "utils/macros.h"
#include "utils/types.h"
#include "workspace/types.h"
#include "workspace/workspace.h"

#include <assert.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define STDLIB_DIR ".local/lily/std"

static char standard_library_path[PATH_MAX] = {0};

/*
 *
 * simply used for dispatching threads
 *
 */
typedef struct {
    Workspace* ws;
    str8 path;
} DispatchFileArg;


static void dispatch_file_intern_thread(void* argument);
static void standard_library_list_init(Driver* driver);

void driver_init(Driver* driver, const char* home) {
    assert(driver != null);

    /*
     *
     * Scratch arena, used for temporary allocations to avoid small malloc/free calls
     *
     */
    arena_init(&driver -> scratch_arena, ARENA_KB(2), ALIGN_DEFAULT);

    /*
     *
     *
     * Home directory & standard library directory
     *
     */
    driver -> home_directory = home;

    snprintf(standard_library_path, sizeof(standard_library_path), "%s/%s", home, STDLIB_DIR);

    driver -> standard_library_directory = standard_library_path;

    standard_library_list_init(driver);

    /*
     *
     * diagnostics
     *
     */

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

    if (thread_count > U8_MAX) {
        thread_count = U8_MAX;
    }

    debug_log("Creating %d threads", thread_count);
    debug_log("Creating %d string interners", thread_count);

    /*
     *
     * for each thread there exists a string interner attached to said thread
     *
     */
    driver -> string_interners = malloc(sizeof(StringInterner) * thread_count);
    driver -> string_interner_count = thread_count;

    for (u8 i = 0; i < thread_count; i += 1) {
        string_interner_init(&driver -> string_interners[i], i);
    }

    /*
     *
     * this function can assert, it also assigns the corresponding string interner 
     * to each thread, for example Thread 0 gets driver.string_interners[0] 
     *
     */
    threadpool_init(driver, thread_count);

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

    driver -> workspaces[0] = arena_calloc(&driver -> workspace_arena, sizeof(Workspace));

    workspace_init(driver -> workspaces[0], 0, "Lily Program <3");
}

void driver_destroy(Driver* driver) {
    /*
     *
     * Just in case.
     * TODO: maybe rather assert that the threadpool isn't busy?
     *
     */
    threadpool_wait(&driver -> thread_pool);

    threadpool_destroy(&driver -> thread_pool);

    /*
     *
     * diagnostics, TODO: might want to make this a per thread thing?
     *
     */
    diagnostic_engine_destroy();

    /*
     *
     * string interners, first free the interners then the array holding them
     *
     */
    for (u32 i = 0; i < driver -> string_interner_count; i++) {
        string_interner_destroy(&driver -> string_interners[i]);
    }

    free(driver -> string_interners);

    /*
     *
     * workspaces, first free the workspaces then the array holding them
     *
     */
    for (u32 i = 0; i < driver -> workspace_count; i++) {
        workspace_destroy(driver -> workspaces[i]);
    }

    arena_destroy(&driver -> workspace_arena);

    arena_destroy(&driver -> scratch_arena);

    arena_destroy(&driver -> standard_library_path_list.arena);
}

/*
 *
 * Command line flag parsing, interns files.
 *
 * TODO: think we make workspaces call this for their own options/flags
 *       first worksapce always should take in from the comppiler directly
 *       the first workspace is essentially the driver workspace that orchestrates
 *       the entire compilation. 
 * 
 */

bool driver_parse_flags(Driver* driver, i32 argc, char** argv) {
    bool result = true;

    for (i32 i = 0; i < argc; i++) {
        char* arg = argv[i];
        usize arg_len = strlen(arg);

        if (arg_len < 2) {
            diagnostic_add_generic(DIAG_ERROR, "invalid argument: %.*s", arg_len, arg);
            result = false;
            continue;
        }

        if (arg[0] == '-') {

        } else {
            if (arg_len < 6) {
                diagnostic_add_generic(DIAG_ERROR, "invalid file: %.*s", arg_len, arg);
                result = false;
                continue;
            }

            /*
             * path.lily (len = 9)
             * ^ 
             * 
             * ptr + len?
             * path.lily
             *          ^
             *
             * so then we do ptr + len - 1
             * path.lily
             *         ^
             *
             * subtract 4 from that, ptr + len - 5
             * path.lily
             *     ^
             *
             * then strncmp(ptr, ".lily", 5) :3
             *
             */

            char* extension_ptr = arg + (arg_len - 5);

            if (strncmp(extension_ptr, ".lily", 5) != 0) {
                diagnostic_add_generic(DIAG_ERROR, "%s is not a lily file", arg);
                result = false;
                continue;
            }

            str8 path = {
                .ptr = arg,
                .len = arg_len
            };

            DispatchFileArg* dispatch_arg = arena_alloc(&driver -> scratch_arena, sizeof(DispatchFileArg));

            dispatch_arg -> ws = driver -> workspaces[0];
            dispatch_arg -> path = path;

            threadpool_add_task(&driver -> thread_pool, dispatch_file_intern_thread, dispatch_arg);
        }
    }

    threadpool_wait(&driver -> thread_pool);

    /*
     *
     * ensure that this is called after `threadpool_wait()`, #lifetimes moment
     *
     */
    arena_reset(&driver -> scratch_arena);

    return result;
}

static void dispatch_file_intern_thread(void* argument) {
    DispatchFileArg* arg = (DispatchFileArg*) argument;

    FileId id = file_intern(&arg -> ws -> file_interner, arg -> path);

    if (id == FILE_ID_NONE) {
        diagnostic_add_generic(DIAG_ERROR, "cannot find file '%.*s'", STR8_FMT(arg -> path));
    }
}

/*
 *
 * Standard Library Path Mapping
 *
 */
static void standard_library_list_init(Driver* driver) {
    StandardLibraryList* list = &driver -> standard_library_path_list;

    constexpr u32 init_kb = 1;
    constexpr u32 init_capacity = ARENA_KB(init_kb) / sizeof(str8);

    arena_init(&list -> arena, ARENA_KB(init_kb), ALIGN_DEFAULT);

    list -> paths = arena_calloc(&list -> arena, init_capacity * sizeof(str8));
    list -> count = 0;
    list -> capacity = init_capacity;

    DIR* directory = opendir(driver -> standard_library_directory);

    if (!directory) {
        diagnostic_add_generic(
            DIAG_ERROR,
            "unable to load standard library!\nPath: %s",
            driver -> standard_library_directory
        );

        return;
    }

    struct dirent* entry = null;

    while ((entry = readdir(directory)) != null) {
        if (entry -> d_type != DT_DIR) continue;

        if (strcmp(entry -> d_name, "." ) == 0) continue;
        if (strcmp(entry -> d_name, "..") == 0) continue;

        const usize size = strlen(entry -> d_name) + sizeof("std/");

        char* name = arena_alloc(&list -> arena, size);

        i32 length = snprintf(name, size, "std/%s", entry -> d_name);

        str8 path = {
            .ptr = name,
            .len = length
        };

        if (UNLIKELY(list -> count >= list -> capacity)) {
            u64 old_size = list -> capacity * sizeof(str8);
            u64 new_size = old_size * 2;

            list -> paths = arena_realloc(&list -> arena, list -> paths, old_size, new_size);
            list -> capacity *= 2;
        }

        list -> paths[list -> count++] = path;
    }

    closedir(directory);
}
