#include "string_interner/interner.h"
#include "hash/hash.h"
#include "ids.h"
#include "string_interner/types.h"
#include "utils/debug.h"
#include "utils/macros.h"
#include "utils/types.h"

#include <assert.h>
#include <pthread.h>
#include <string.h>

/*
 *
 *  initial sizing and capacity
 *
 */

static constexpr u32 init_cap = 128;
static constexpr f32 load_factor = 0.75f;
static constexpr u32 init_resize_factor = (u32)(load_factor * init_cap);

static_assert(init_resize_factor < init_cap);

static constexpr u32 init_bucket_size = init_cap * sizeof(StringBucket); 
static constexpr u32 init_entry_size = init_cap * sizeof(StringEntry);

static constexpr u32 init_kb = (init_bucket_size + init_entry_size) / ARENA_KB(1);

// Functions that work around the locking
static StringId string_intern_locked(StringInterner* interner, str8 str, u32 hash);
static StringId string_lookup_locked(StringInterner* interner, str8 str, u32 hash);

static void resize_buckets(StringInterner* interner);
static void resize_entries(StringInterner* interner);

/*
 *  
 *  interner creation and deletion
 *
 */

void string_interner_init(StringInterner* interner) {
    assert(interner != null);

    arena_init(&interner -> arena, ARENA_KB(init_kb), ALIGN_DEFAULT);

    interner -> buckets = arena_alloc(&interner -> arena, init_bucket_size);
    interner -> entries = arena_alloc(&interner -> arena, init_entry_size);

    interner -> bucket_capacity = init_cap;
    interner -> entry_capacity = init_cap;

    interner -> count = 0;

    interner -> resize_threshold_as_u32 = init_resize_factor;

    arena_memset(interner -> buckets, U8_MAX, init_bucket_size);

    debug_log("Allocated %u bytes for buckets", init_bucket_size);
    debug_log("Allocated %u bytes for entries", init_entry_size);

    pthread_rwlock_init(&interner -> mutex, null);
}

inline void string_interner_destroy(StringInterner* interner) {
    assert(interner != null);

    arena_destroy(&interner -> arena);

    pthread_rwlock_destroy(&interner -> mutex);
}

/*
 *  
 *  str8 interning
 *
 */

StringId string_intern_str8(StringInterner* interner, str8 str) {
    assert(interner != null);
    assert(str.ptr != null);
    assert(str.len != 0);

    debug_log("Intering string: %.*s", str.len, str.ptr);

    u32 hash = hash_str8(str);

    pthread_rwlock_rdlock(&interner -> mutex);

    StringId id = string_lookup_locked(interner, str, hash);

    // pthread_rwlock_unlock(&interner -> mutex);

    if (id != STRING_ID_NONE) {
        debug_log("Found string: %.*s", str.len, str.ptr);
        pthread_rwlock_unlock(&interner -> mutex);
        return id;
    }

    pthread_rwlock_unlock(&interner -> mutex);

    pthread_rwlock_wrlock(&interner -> mutex);

    id = string_lookup_locked(interner, str, hash);

    if (id != STRING_ID_NONE) {
        pthread_rwlock_unlock(&interner -> mutex);
        debug_log("Found string (after race): %.*s", str.len, str.ptr);
        return id;;
    }

    id = string_intern_locked(interner, str, hash);

    pthread_rwlock_unlock(&interner -> mutex);

    debug_assert(id == string_lookup_str8(interner, str));

    debug_log("Interned string (id=%u): %.*s", id, str.len, str.ptr);

    return id;
}

StringId string_lookup_str8(StringInterner* interner, str8 str) {
    assert(interner != null);
    assert(str.ptr != null);
    assert(str.len != 0);

    debug_log("Looking for string: %.*s", str.len, str.ptr);

    u32 hash = hash_str8(str);

    pthread_rwlock_rdlock(&interner -> mutex);

    StringId id = string_lookup_locked(interner, str, hash);

    pthread_rwlock_unlock(&interner -> mutex);

    if (id != STRING_ID_NONE) {
        debug_log("Found string (id=%u): %.*s", id, str.len, str.ptr);
    } else {
        debug_log("Could not find string: %.*s", str.len, str.ptr);
    }

    return id;
}

static StringId string_intern_locked(StringInterner* interner, str8 str, u32 hash) {
    if (UNLIKELY(interner -> count >= interner -> resize_threshold_as_u32)) {
        resize_buckets(interner);
    }

    u32 mask  = interner -> bucket_capacity - 1;
    u32 index = hash & mask;

    while (interner -> buckets[index].id != STRING_ID_NONE) {
        index = (index + 1) & mask;
    }

    if (UNLIKELY(interner -> count >= interner -> entry_capacity)) {
        resize_entries(interner);
    }

    StringId id = interner -> count++;

    interner -> buckets[index].id = id;
    interner -> buckets[index].hash = hash;

    interner -> entries[id] = (StringEntry) {
        .str = {
            .ptr = str.ptr,
            .len = str.len,
        },
        .hash = hash
    };

    return id;
}

static StringId string_lookup_locked(StringInterner* interner, str8 str, u32 hash) {
    u32 mask  = interner -> bucket_capacity - 1;
    u32 index = hash & mask;

    while (interner -> buckets[index].id != STRING_ID_NONE) {
        StringBucket bucket = interner -> buckets[index];

        if (bucket.hash == hash) {
            StringEntry entry = interner -> entries[bucket.id];

            if (entry.str.len == str.len && memcmp(entry.str.ptr, str.ptr, str.len) == 0) {
                return bucket.id;
            }
        }

        index = (index + 1) & mask;
    }

    return STRING_ID_NONE;
}

/*
 *
 *  C string interning
 * 
 */

inline StringId string_intern_cstr(StringInterner* interner, const char* str) {
    return string_intern_str8(interner, (str8) { .ptr = (char*) str, .len = strlen(str)});
}

inline StringId string_lookup_cstr(StringInterner* interner, const char* str) {
    return string_lookup_str8(interner, (str8) { .ptr = (char*) str, .len = strlen(str)});
}

/*
 *
 *  interner resizing
 * 
 */

static void resize_buckets(StringInterner* interner) {
    u32 new_capacity = interner -> bucket_capacity * 2;

    u64 old_size = sizeof(StringBucket) * interner -> bucket_capacity;
    u64 new_size = new_capacity * sizeof(StringBucket);

    StringBucket* new_buckets = arena_realloc(&interner -> arena, interner -> buckets, old_size, new_size);
    arena_memset(new_buckets, U8_MAX, new_size);

    debug_log("Reallocated buckets from %lu to %lu", new_size / 2, new_size);

    for (u32 i = 0; i < interner -> count; i++) {
        StringEntry entry = interner -> entries[i];

        u32 new_index = entry.hash & (new_capacity - 1);

        while (new_buckets[new_index].id != STRING_ID_NONE) {
            new_index = (new_index + 1) & (new_capacity - 1);
        }

        new_buckets[new_index].id = i;
        new_buckets[new_index].hash = entry.hash;
    }

    interner -> buckets = new_buckets;
    interner -> bucket_capacity = new_capacity;

    interner -> resize_threshold_as_u32 = (u32)(new_capacity * load_factor);
}

static void resize_entries(StringInterner* interner) {
    u64 old_size = sizeof(StringEntry) * interner -> entry_capacity; 
    u64 new_size = old_size * 2;

    interner -> entries = arena_realloc(&interner -> arena, interner -> entries, old_size, new_size);
    interner -> entry_capacity *= 2;

    debug_log("Reallocated entries from %lu to %lu bytes", old_size, new_size);
}
