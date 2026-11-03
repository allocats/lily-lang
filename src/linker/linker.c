#include "cmd/cmd.h"
#include "driver/types.h"
#include "workspace/types.h"

#include <stdio.h>

extern char **environ;

extern Driver driver;

i32 linker_run(Workspace *ws) {
    u32 file_count = ws->file_interner.count;

    arena_reset(&driver.scratch_arena);

    char threads_arg[32];

    snprintf(threads_arg, sizeof(threads_arg), "-Wl,--threads=%u", driver.thread_pool.thread_count);

    char **args   = arena_alloc(&driver.scratch_arena, (file_count + 6) * sizeof(char *));
    u32 arg_count = 0;

    args[arg_count++] = "cc";
    args[arg_count++] = "-fuse-ld=lld";
    args[arg_count++] = threads_arg;
    args[arg_count++] = "-Wl,--gc-sections";

    for (u32 i = 0; i < file_count; i++) {
        char *path = ws->file_interner.entries[i]->object_path.ptr;

        args[arg_count++] = path;
    }

    args[arg_count] = null;

    return run_command("cc", args);
}
