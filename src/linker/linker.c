#include "cmd/cmd.h"
#include "driver/flags.h"
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

    char **args   = arena_alloc(&driver.scratch_arena, (file_count + 8) * sizeof(char *));
    u32 arg_count = 0;

    args[arg_count++] = "cc";

    if (driver.flags & DRIVER_FLAG_STATIC_BUILD) {
        args[arg_count++] = "-static";
    }

    args[arg_count++] = "-fuse-ld=lld";
    args[arg_count++] = threads_arg;
    args[arg_count++] = "-Wl,--gc-sections";
    args[arg_count++] = "-Wl,-z,now";
    args[arg_count++] = "-Wl,-z,relro";

    for (u32 i = 0; i < ws->object_path_count; i++) {
        str8 path = ws->object_paths[i];

        args[arg_count++] = path.ptr;
    }

    args[arg_count] = null;

    return run_command("cc", args);
}
