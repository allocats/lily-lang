#include "ast/ast.h"
#include "diagnostics/diagnostics.h"
#include "files/files.h"
#include "files/types.h"
#include "hash/hash.h"
#include "ids.h"
#include "threadpool/types.h"
#include "tokens/tokens.h"
#include "utils/debug.h"
#include "utils/macros.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/limits.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <threads.h>
#include <unistd.h>

extern thread_local ThreadContext thread_context;

/*
 *
 *  initial sizing and capacity
 *
 */

static constexpr u32 init_cap = 16;
static constexpr f32 load_factor = 0.75f;
static constexpr u32 init_resize_factor = (u32)(load_factor * init_cap);

static_assert(init_resize_factor < init_cap);

static constexpr u32 init_bucket_size = init_cap * sizeof(FileBucket); 
static constexpr u32 init_entry_size = init_cap * sizeof(File*);

static constexpr u32 init_interner_kb = 4;
static constexpr u32 init_buffer_mb = 32;


static void resize_buckets(FileInterner* interner);
static void resize_entries(FileInterner* interner);

static bool allocate_buffer(FileInterner* interner, str8 path, str8* buffer);
static void file_init(File* file, FileId id, str8 path, u32 path_hash, str8 buffer);
static void file_destroy(File* file);

static str8 get_absolute_path(str8 path);

/*
 *  
 *  interner creation and deletion
 *
 */

void file_interner_init(FileInterner* interner) {
    assert(interner);

    arena_init(&interner -> interner_arena, ARENA_KB(init_interner_kb), ALIGN_DEFAULT);
    debug_log("Init interner arena with %uKB", ARENA_KB(init_interner_kb));

    arena_init(&interner -> buffer_arena, ARENA_MB(init_buffer_mb), ALIGN_DEFAULT);
    debug_log("Init buffer arena with %uMB", ARENA_KB(init_buffer_mb));

    // interner -> buckets = arena_alloc(&interner -> interner_arena, init_bucket_size);
    interner -> buckets = malloc(init_bucket_size);
    // interner -> entries = arena_calloc(&interner -> interner_arena, init_entry_size);
    interner -> entries = malloc(init_entry_size);

    interner -> bucket_capacity = init_cap;
    interner -> entry_capacity = init_cap;

    interner -> count = 0;

    interner -> resize_threshold_as_u32 = init_resize_factor;

    debug_log("Allocated %u bytes for buckets", init_bucket_size);
    debug_log("Allocated %u bytes for entries", init_entry_size);

    memset(interner -> buckets, U8_MAX, init_bucket_size);

    pthread_mutex_init(&interner -> mutex, null);
}

inline void file_interner_destroy(FileInterner* interner) {
    assert(interner);
    
    for (u32 i = 0; i < interner -> count; i++) {
        file_destroy(interner -> entries[i]);
    }

    arena_destroy(&interner -> interner_arena);
    arena_destroy(&interner -> buffer_arena);

    free(interner -> buckets);
    free(interner -> entries);

    pthread_mutex_destroy(&interner -> mutex);
}

/*
 *  
 *  file interning and lookups
 *
 */

FileId file_intern(FileInterner* interner, str8 input_path) {
    assert(interner != null);
    assert(input_path.ptr != null);
    assert(input_path.len != 0);

    debug_log("Looking to intern %.*s", STR8_FMT(input_path));

    str8 path = get_absolute_path(input_path);

    if (path.ptr == null) {
        // diagnostic_add_generic(DIAG_ERROR, "cannot find file %.*s", input_path.len, input_path.ptr);
        return FILE_ID_NONE;
    }

    str8 buffer = {0};

    if (!allocate_buffer(interner, path, &buffer)) {
        diagnostic_add_generic(
            thread_context.diagnostic_engine,
            DIAG_ERROR,
            "cannot allocate buffer for file %.*s",
            path.len,
            path.ptr
        );
        return FILE_ID_NONE;
    }

    pthread_mutex_lock(&interner -> mutex);

    debug_log("Intering file: %.*s", path.len, path.ptr);

    if (UNLIKELY(interner -> count >= interner -> resize_threshold_as_u32)) {
        resize_buckets(interner);
    }

    u32 hash  = hash_str8(path);
    u32 mask  = interner -> bucket_capacity - 1;
    u32 index = hash & mask;

    while (interner -> buckets[index].id != FILE_ID_NONE) {
        FileBucket bucket = interner -> buckets[index];

        if (bucket.hash == hash) {
            File* file = interner -> entries[bucket.id];

            if (file -> path.len == path.len && memcmp(file -> path.ptr, path.ptr, path.len) == 0) {
                debug_log("Found file: %.*s", path.len, path.ptr);

                pthread_mutex_unlock(&interner -> mutex);

                return bucket.id;
            }
        }

        index = (index + 1) & mask;
    }

    if (UNLIKELY(interner -> count >= interner -> entry_capacity)) {
        resize_entries(interner);
    }

    FileId id = interner -> count++;

    interner -> buckets[index].id = id;
    interner -> buckets[index].hash = hash;

    File* file = arena_calloc(&interner -> interner_arena, sizeof(File));

    interner -> entries[id] = file;

    file_init(file, id, path, hash, buffer);

    debug_log("Intered file (id=%u): %.*s", id, path.len, path.ptr);

    // free(path.ptr);
    
    pthread_mutex_unlock(&interner -> mutex);

    debug_assert(id == file_lookup(interner, path));

    return id;
}

FileId file_lookup(FileInterner* interner, str8 path) {
    assert(interner != null);
    assert(path.ptr != null);
    assert(path.len != 0);

    path = get_absolute_path(path);

    // In theory, should not be happening
    assert(path.ptr != null);

    pthread_mutex_lock(&interner -> mutex);

    debug_log("Looking for file: %.*s", path.len, path.ptr);

    u32 hash  = hash_str8(path);
    u32 mask  = interner -> bucket_capacity - 1;
    u32 index = hash & mask;

    while (interner -> buckets[index].id != FILE_ID_NONE) {
        FileBucket bucket = interner -> buckets[index];

        if (bucket.hash == hash) {
            File* file = interner -> entries[bucket.id];

            if (file -> path.len == path.len && memcmp(file -> path.ptr, path.ptr, path.len) == 0) {
                debug_log("Found file (id=%u): %.*s", bucket.id, path.len, path.ptr);

                pthread_mutex_unlock(&interner -> mutex);

                return bucket.id;
            }
        }

        index = (index + 1) & mask;
    }

    debug_log("Could not find file: %.*s", path.len, path.ptr);

    pthread_mutex_unlock(&interner -> mutex);

    return FILE_ID_NONE;
}

/*
 *  
 *  interner resizing functions
 *
 */

static void resize_buckets(FileInterner* interner) {
    u64 old_size = interner -> bucket_capacity * sizeof(FileBucket);
    u64 new_size = old_size * 2;

    u32 new_capacity = interner -> bucket_capacity * 2;

    // FileBucket* new_buckets = arena_realloc(&interner -> interner_arena, interner -> buckets, old_size, new_size);
    FileBucket* new_buckets = realloc(interner -> buckets, new_size);
    memset(new_buckets, U8_MAX, new_size);

    debug_log("Reallocated buckets from %lu -> %lu bytes", old_size , new_size);

    for (u32 i = 0; i < interner -> count; i++) {
        File* entry = interner -> entries[i];

        u32 new_index = entry -> path_hash & (new_capacity - 1);

        while (new_buckets[new_index].id != FILE_ID_NONE) {
            new_index = (new_index + 1) & (new_capacity - 1);
        }

        new_buckets[new_index].id = i;
        new_buckets[new_index].hash = entry -> path_hash;
    }

    interner -> buckets = new_buckets;
    interner -> bucket_capacity = new_capacity;
    interner -> resize_threshold_as_u32 = (u32)(new_capacity * load_factor);
}

static void resize_entries(FileInterner* interner) {
    u64 old_size = interner -> entry_capacity * sizeof(File*);
    u64 new_size = old_size * 2;

    // interner -> entries = arena_realloc(&interner -> interner_arena, interner -> entries, old_size, new_size);
    interner -> entries = realloc(interner -> entries, new_size);
    interner -> entry_capacity *= 2;

    debug_log("Reallocated entries from %lu to %lu bytes", old_size, new_size);
}

/*
 *  
 *  file creation an deletion functions, setting values
 *  reading and loading the buffers
 *
 */

static void file_init(File* file, FileId id, str8 path, u32 path_hash, str8 buffer) {
    file -> id = id;
    file -> stage = FILE_ALLOCATED;
    file -> scope_id = GLOBAL_SCOPE_ID_NONE;

    file -> path_hash = path_hash;
    file -> path = path;

    file -> buffer = buffer;

    u64 initial_token_capacity = NEXT_POWER_OF_TWO(buffer.len / 3);

    debug_log("Creating token array with %lu tokens, buffer.len=%u", initial_token_capacity, buffer.len);

    token_array_init(&file -> tokens, initial_token_capacity);

    ast_init(&file -> ast);
}

inline void file_destroy(File* file) {
    assert(file);

    free(file -> path.ptr);

    token_array_destroy(&file -> tokens);
    ast_destroy(&file -> ast);
}

static bool allocate_buffer(FileInterner* interner, str8 path, str8* buffer) {
    i32 fd = open(path.ptr, O_RDONLY);
    if (fd < 0) {
        return false;
    }

    struct stat st;
    if (fstat(fd, &st) < 0) {
        close(fd);

        return false;
    }

    u32 buffer_size = st.st_size + 1;

    pthread_mutex_lock(&interner -> mutex);

    buffer -> ptr = arena_calloc(&interner -> buffer_arena, buffer_size);
    buffer -> len = buffer_size;

    pthread_mutex_unlock(&interner -> mutex);

    u64 bytes_read = 0;

    while (bytes_read < buffer_size) {
        i64 n = (i64) read(fd, buffer -> ptr + bytes_read, buffer_size - bytes_read);

        if (n == 0) break;
        if (n == -1) {
            if (errno == EINTR) {
                continue;
            }

            diagnostic_add_generic(
                thread_context.diagnostic_engine,
                DIAG_ERROR,
                "failed to read file '%.*s'",
                STR8_FMT(path)
            );

            buffer -> ptr = null;
            buffer -> len = 0;

            close(fd);

            return false;
        }

        bytes_read += n;
    }

    close(fd);

    debug_log("Allocated file (%u bytes): %.*s", buffer_size, path.len, path.ptr);

    return true;
}

/*
 *  
 *  path normalizing, gets the absolute true file path,
 *  expects the path.ptr to be a null terminated string
 *
 *  example:
 *      path = "./examples/helloworld.lily"
 *      returns = "/home/angel/Projects/lily-lang/examples/helloworld.lily"
 *
 */

static str8 get_absolute_path(str8 path) {
    assert(path.ptr != null);
    assert(path.len != 0);
    assert(*(path.ptr + path.len) == 0);

    char* real_path = realpath(path.ptr, null);

    if (real_path != null) {
        path.ptr = real_path;
        path.len = strnlen(real_path, PATH_MAX);
    } else {
        path.ptr = null;
        path.len = 0;
    }

    return path;
}
