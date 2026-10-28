#include "driver/types.h"
#include "workspace/types.h"

#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <wait.h>

extern char **environ;

extern Driver driver;

static i32 run_command(const char *path, char **args) {
    pid_t pid;

    i32 rc = posix_spawnp(&pid, path, NULL, NULL, args, environ);

    if (rc != 0) {
        fprintf(stderr, "posix_spawnp failed for %s: %s\n", path, strerror(rc));
        return -1;
    }

    i32 status = 0;

    if (waitpid(pid, &status, 0) < 0) {
        perror("waitpid");
        return -1;
    }

    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }

    if (WIFSIGNALED(status)) {
        fprintf(stderr, "%s terminated by signal %d\n", path, WTERMSIG(status));
    }

    return -1;
}

i32 linker_run(Workspace *ws) {
    u32 file_count = ws->file_interner.count;

    arena_reset(&driver.scratch_arena);

    char threads_arg[32];

    snprintf(threads_arg, sizeof(threads_arg), "-Wl,--threads=%u", driver.thread_pool.thread_count);

    char **args   = arena_alloc(&driver.scratch_arena, (file_count + 5) * sizeof(char *));
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
