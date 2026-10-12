#define MEOWRENA_IMPL
#include "meowrena/meowrena.h"
#undef MEOWRENA_IMPL

#include "cli/cli.h"
#include "diagnostics/diagnostics.h"
#include "driver/driver.h"
#include "utils/types.h"
#include "workspace/workspace.h"

#include <assert.h>
#include <stdlib.h>

Driver driver = {0};

int **testing = 0;

i32 main(i32 argc, char **argv) {
    assert(argc > 0);
    assert(argv != null);

    // loads terminal colours or none if unable to
    cli_init_ansi_codes();

    if (argc < 2) {
        cli_print_usage(argv[0]);
        return 1;
    }

    const char *home_dir = getenv("HOME");
    if (!home_dir) {
        cli_print_home_error(argv[0]);
        return 1;
    }

    driver_init(&driver, home_dir);

    if (!driver_parse_flags(&driver, argc - 1, argv + 1)) {
        for (u8 i = 0; i < driver.local_diagnostic_engine_count; i += 1) {
            diagnostics_print(&driver.local_diagnostic_engines[i]);
        }

        diagnostics_print(&driver.global_diagnostic_engine);

        driver_destroy(&driver);

        printf("\n%s%s%s:%s compiler %sfailed%s\n", ANSI_BOLD, ANSI_RED, argv[0], ANSI_RESET, ANSI_BOLD, ANSI_RESET);

        return 1;
    }

    /*
     *
     * call driving compilation loop for workspaces,
     * in theory should grow the array if need be and this 
     * keeps going on. might want to multithread this too, but 
     * has its own implications, right now not a win at all so I am
     * just going to mark this here and deal with it later when it matters
     *
     */
    for (u32 i = 0; i < driver.workspace_count; i += 1) {
        Workspace *ws = driver.workspaces[i];

        compile_workspace(ws);

        // threadpool_add_task(&driver.thread_pool, dispatch_compile_workspace, ws);
    }

    // threadpool_wait(&driver.thread_pool);

    bool has_errors = false;

    for (u8 i = 0; i < driver.local_diagnostic_engine_count; i += 1) {
        bool errored = diagnostics_print(&driver.local_diagnostic_engines[i]);

        if (errored) {
            has_errors = true;
        }
    }

    bool errored = diagnostics_print(&driver.global_diagnostic_engine);

    if (errored) {
        has_errors = true;
    }

    for (u32 i = 0; i < driver.workspace_count; i += 1) {
        Workspace *ws = driver.workspaces[i];

        cli_print_workspace_stats(ws);
    }

    i32 exit_code = has_errors == true ? 1 : 0;

    if (!has_errors) {
        printf(
            "%s%s%s:%s compiled %ssuccessfully%s\n",
            ANSI_BOLD,
            ANSI_CYAN,
            argv[0],
            ANSI_RESET,
            ANSI_BOLD,
            ANSI_RESET
        );
    } else {
        printf(
            "%s%s%s:%s compiler %sfailed%s\n",
            ANSI_BOLD,
            ANSI_RED,
            argv[0],
            ANSI_RESET,
            ANSI_BOLD,
            ANSI_RESET
        );
    }

    driver_destroy(&driver);

    return exit_code;
}
