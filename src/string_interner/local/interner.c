#include "string_interner/local/interner.h"
#include "driver/types.h"
#include "hash/hash.h"
#include "ids.h"
#include "string_interner/local/types.h"
#include "utils/debug.h"
#include "utils/macros.h"
#include "utils/types.h"
#include "utils/wrappers.h"

#include <assert.h>
#include <string.h>

extern Driver driver;

/*
 *
 *  initial sizing and capacity
 *
 */

static constexpr u32 init_cap           = 256;
static constexpr f32 load_factor        = 0.75f;
static constexpr u32 init_resize_factor = (u32)(load_factor * init_cap);

static_assert(init_resize_factor < init_cap);

static constexpr u32 init_bucket_size = init_cap * sizeof(StringBucket);
static constexpr u32 init_entry_size  = init_cap * sizeof(StringEntry);

static void resize_buckets(LocalStringInterner *interner);
static void resize_entries(LocalStringInterner *interner);

/*
 *  
 *  interner creation and deletion
 *
 */

void local_string_interner_init(LocalStringInterner *interner, u8 id) {
    assert(interner != null);

    interner->buckets = lily_alloc(init_bucket_size);
    interner->entries = lily_alloc(init_entry_size);

    interner->bucket_capacity = init_cap;
    interner->entry_capacity  = init_cap;

    interner->count = 0;

    interner->resize_threshold_as_u32 = init_resize_factor;

    interner->id = id;

    for (u32 i = 0; i < init_cap; i += 1) {
        interner->buckets[i].id = local_string_create_id(id, LOCAL_STRING_ID_NONE);
    }

    debug_log("Allocated %u bytes for buckets", init_bucket_size);
    debug_log("Allocated %u bytes for entries", init_entry_size);

    /*
     *
     * intern all the keywords and directives :3
     *
     */
    local_string_intern_cstr(interner, "import");
    local_string_intern_cstr(interner, "include");
    local_string_intern_cstr(interner, "execute");
    local_string_intern_cstr(interner, "packed");
    local_string_intern_cstr(interner, "align");
    local_string_intern_cstr(interner, "sys_lib");
    local_string_intern_cstr(interner, "library");

    local_string_intern_cstr(interner, "fn");
    local_string_intern_cstr(interner, "enum");
    local_string_intern_cstr(interner, "struct");
    local_string_intern_cstr(interner, "union");

    local_string_intern_cstr(interner, "intrinsic");
    local_string_intern_cstr(interner, "foreign");
    local_string_intern_cstr(interner, "noinline");
    local_string_intern_cstr(interner, "inline");

    local_string_intern_cstr(interner, "const");

    local_string_intern_cstr(interner, "if");
    local_string_intern_cstr(interner, "else");
    local_string_intern_cstr(interner, "for");
    // local_string_intern_cstr(interner, "in");
    local_string_intern_cstr(interner, "while");
    local_string_intern_cstr(interner, "continue");
    local_string_intern_cstr(interner, "break");
    local_string_intern_cstr(interner, "defer");
    local_string_intern_cstr(interner, "return");

    local_string_intern_cstr(interner, "false");
    local_string_intern_cstr(interner, "true");
    local_string_intern_cstr(interner, "null");

    for (u32 i = 0; i < driver.standard_library_path_list.count; i += 1) {
        local_string_intern_str8(interner, driver.standard_library_path_list.paths[i]);
    }
}

inline void local_string_interner_destroy(LocalStringInterner *interner) {
    assert(interner != null);

    lily_free(interner->buckets);
    lily_free(interner->entries);
}

/*
 *  
 *  str8 interning
 *
 */

StringId local_string_intern_str8(LocalStringInterner *interner, str8 str) {
    assert(interner != null);
    assert(str.ptr != null);

    debug_log("Intering string: %.*s", str.len, str.ptr);

    if (UNLIKELY(interner->count >= interner->resize_threshold_as_u32)) {
        resize_buckets(interner);
    }

    u32 hash  = hash_str8(str);
    u32 mask  = interner->bucket_capacity - 1;
    u32 index = hash & mask;

    while (local_string_get_id(interner->buckets[index].id) != LOCAL_STRING_ID_NONE) {
        StringBucket bucket = interner->buckets[index];

        if (bucket.hash == hash) {
            StringId local_id = local_string_get_id(bucket.id);

            StringEntry entry = interner->entries[local_id];

            if (entry.str.len == str.len && memcmp(entry.str.ptr, str.ptr, str.len) == 0) {
                return bucket.id;
            }
        }

        index = (index + 1) & mask;
    }

    if (UNLIKELY(interner->count >= interner->entry_capacity)) {
        resize_entries(interner);
    }

    StringId id = local_string_create_id(interner->id, interner->count++);

    assert(interner->count < LOCAL_STRING_ID_NONE);

    interner->buckets[index].id   = id;
    interner->buckets[index].hash = hash;

    StringId local_id = local_string_get_id(id);

    interner->entries[local_id] = (StringEntry){
        .str = {
            .ptr = str.ptr,
            .len = str.len,
        },
        .hash = hash
    };

    return id;
}

StringId local_string_lookup_str8(LocalStringInterner *interner, str8 str) {
    assert(interner != null);
    assert(str.ptr != null);

    debug_log("Looking for string: %.*s", str.len, str.ptr);

    u32 hash  = hash_str8(str);
    u32 mask  = interner->bucket_capacity - 1;
    u32 index = hash & mask;

    while (local_string_get_id(interner->buckets[index].id) != LOCAL_STRING_ID_NONE) {
        StringBucket bucket = interner->buckets[index];

        if (bucket.hash == hash) {
            StringId local_id = local_string_get_id(bucket.id);

            StringEntry entry = interner->entries[local_id];

            if (entry.str.len == str.len && memcmp(entry.str.ptr, str.ptr, str.len) == 0) {
                return bucket.id;
            }
        }

        index = (index + 1) & mask;
    }

    return LOCAL_STRING_ID_NONE;
}

/*
 *
 *  C string interning
 * 
 */

inline StringId local_string_intern_cstr(LocalStringInterner *interner, const char *str) {
    return local_string_intern_str8(interner, (str8){.ptr = (char *)str, .len = strlen(str)});
}

inline StringId local_string_lookup_cstr(LocalStringInterner *interner, const char *str) {
    return local_string_lookup_str8(interner, (str8){.ptr = (char *)str, .len = strlen(str)});
}

/*
 *
 *  Intering strings from tokens
 * 
 */

StringId local_string_intern_token(LocalStringInterner *interner, File *file, Token token) {
    str8 str = {
        .ptr = file->buffer.ptr + token.start,
        .len = token.length
    };

    assert(str.ptr < (file->buffer.ptr + file->buffer.len));
    assert(str.ptr + str.len < (file->buffer.ptr + file->buffer.len));

    return local_string_intern_str8(interner, str);
}

StringId local_string_lookup_token(LocalStringInterner *interner, File *file, Token token) {
    str8 str = {
        .ptr = file->buffer.ptr + token.start,
        .len = token.length
    };

    assert(str.ptr < (file->buffer.ptr + file->buffer.len));
    assert(str.ptr + str.len < (file->buffer.ptr + file->buffer.len));

    return local_string_lookup_str8(interner, str);
}

/*
 *
 *  helper to get str8 from a StringId
 * 
 */

inline str8 local_string_get_str8_from_id(LocalStringInterner *interner, StringId id) {
    return interner->entries[local_string_get_id(id)].str;
}

/*
 *
 *  interner resizing
 * 
 */

static void resize_buckets(LocalStringInterner *interner) {
    u32 new_capacity = interner->bucket_capacity * 2;

    u64 new_size = new_capacity * sizeof(StringBucket);

    StringBucket *new_buckets = lily_realloc(interner->buckets, new_size);

    for (u32 i = 0; i < new_capacity; i += 1) {
        new_buckets[i].id = local_string_create_id(interner->id, LOCAL_STRING_ID_NONE);
    }

    debug_log("Reallocated buckets from %lu to %lu", new_size / 2, new_size);

    for (u32 i = 0; i < interner->count; i++) {
        StringEntry entry = interner->entries[i];

        u32 new_index = entry.hash & (new_capacity - 1);

        while (local_string_get_id(new_buckets[new_index].id) != LOCAL_STRING_ID_NONE) {
            new_index = (new_index + 1) & (new_capacity - 1);
        }

        new_buckets[new_index].id   = local_string_create_id(interner->id, i);
        new_buckets[new_index].hash = entry.hash;
    }

    interner->buckets         = new_buckets;
    interner->bucket_capacity = new_capacity;

    interner->resize_threshold_as_u32 = (u32)(new_capacity * load_factor);
}

static void resize_entries(LocalStringInterner *interner) {
    u64 old_size = sizeof(StringEntry) * interner->entry_capacity;
    u64 new_size = old_size * 2;

    interner->entries = lily_realloc(interner->entries, new_size);
    interner->entry_capacity *= 2;

    debug_log("Reallocated entries from %lu to %lu bytes", old_size, new_size);
}

/*
 *
 * New helpers to make use of the new StringId system
 *
 */

inline u8 local_string_get_interner_id(StringId id) {
    return (id >> LOCAL_STRING_ID_BIT_OFFSET) & U8_MAX;
}

inline StringId local_string_get_id(StringId id) {
    return id & LOCAL_STRING_ID_NONE;
}

inline StringId local_string_create_id(u8 interner_id, StringId string_id) {
    return (interner_id << LOCAL_STRING_ID_BIT_OFFSET) | string_id;
}

bool local_string_are_strings_equal(StringId a, StringId b) {
    if (a == b) {
        return true;
    }

    u8 interner_b_id = local_string_get_interner_id(b);
    u8 interner_a_id = local_string_get_interner_id(a);

    if (interner_a_id == interner_b_id) {
        return false;
    }

    StringEntry entry_a = driver.local_string_interners[interner_a_id].entries[local_string_get_id(a)];
    StringEntry entry_b = driver.local_string_interners[interner_b_id].entries[local_string_get_id(b)];

    if (
        entry_a.hash == entry_b.hash &&
        entry_a.str.len == entry_b.str.len &&
        memcmp(entry_a.str.ptr, entry_b.str.ptr, entry_b.str.len) == 0
    ) {
        return true;
    }

    return false;
}
