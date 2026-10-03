#include "driver/types.h"
#include "hash/hash.h"
#include "ids.h"
#include "string_interner/interner.h"
#include "string_interner/types.h"
#include "utils/debug.h"
#include "utils/macros.h"
#include "utils/types.h"

#include <assert.h>
#include <string.h>

extern Driver driver;

/*
 *
 *  initial sizing and capacity
 *
 */

static constexpr u32 init_cap = 256;
static constexpr f32 load_factor = 0.75f;
static constexpr u32 init_resize_factor = (u32)(load_factor * init_cap);

static_assert(init_resize_factor < init_cap);

static constexpr u32 init_bucket_size = init_cap * sizeof(StringBucket); 
static constexpr u32 init_entry_size = init_cap * sizeof(StringEntry);

static void resize_buckets(StringInterner* interner);
static void resize_entries(StringInterner* interner);

/*
 *  
 *  interner creation and deletion
 *
 */

void string_interner_init(StringInterner* interner, u8 id) {
    assert(interner != null);

    interner -> buckets = malloc(init_bucket_size);
    interner -> entries = malloc(init_entry_size);

    interner -> bucket_capacity = init_cap;
    interner -> entry_capacity = init_cap;

    interner -> count = 0;

    interner -> resize_threshold_as_u32 = init_resize_factor;

    interner -> id = id;

    for (u32 i = 0; i < init_cap; i += 1) {
        interner -> buckets[i].id = create_string_id(id, STRING_ID_NONE);
    }

    debug_log("Allocated %u bytes for buckets", init_bucket_size);
    debug_log("Allocated %u bytes for entries", init_entry_size);

    /*
     *
     * intern all the keywords and directives :3
     *
     */
    string_intern_cstr(interner, "import");
    string_intern_cstr(interner, "include");
    string_intern_cstr(interner, "execute");
    string_intern_cstr(interner, "packed");
    string_intern_cstr(interner, "align");

    string_intern_cstr(interner, "fn");
    string_intern_cstr(interner, "enum");
    string_intern_cstr(interner, "struct");
    string_intern_cstr(interner, "union");

    string_intern_cstr(interner, "intrinsic");
    string_intern_cstr(interner, "foreign");
    string_intern_cstr(interner, "noinline");
    string_intern_cstr(interner, "inline");

    string_intern_cstr(interner, "const");

    string_intern_cstr(interner, "if");
    string_intern_cstr(interner, "else");
    string_intern_cstr(interner, "for");
    // string_intern_cstr(interner, "in");
    string_intern_cstr(interner, "while");
    string_intern_cstr(interner, "continue");
    string_intern_cstr(interner, "break");
    string_intern_cstr(interner, "defer");
    string_intern_cstr(interner, "return");

    string_intern_cstr(interner, "false");
    string_intern_cstr(interner, "true");
    string_intern_cstr(interner, "null");

    for (u32 i = 0; i < driver.standard_library_path_list.count; i += 1) {
        string_intern_str8(interner, driver.standard_library_path_list.paths[i]);
    }
}

inline void string_interner_destroy(StringInterner* interner) {
    assert(interner != null);

    free(interner -> buckets);
    free(interner -> entries);
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

    if (UNLIKELY(interner -> count >= interner -> resize_threshold_as_u32)) {
        resize_buckets(interner);
    }

    u32 hash  = hash_str8(str);
    u32 mask  = interner -> bucket_capacity - 1;
    u32 index = hash & mask;

    while (get_local_string_id(interner -> buckets[index].id) != STRING_ID_NONE) {
        StringBucket bucket = interner -> buckets[index];

        if (bucket.hash == hash) {
            StringId local_id = get_local_string_id(bucket.id);

            StringEntry entry = interner -> entries[local_id];

            if (entry.str.len == str.len && memcmp(entry.str.ptr, str.ptr, str.len) == 0) {
                return bucket.id;
            }
        }

        index = (index + 1) & mask;
    }

    if (UNLIKELY(interner -> count >= interner -> entry_capacity)) {
        resize_entries(interner);
    }

    StringId id = create_string_id(interner -> id, interner -> count++);

    assert(interner -> count < STRING_ID_NONE);

    interner -> buckets[index].id = id;
    interner -> buckets[index].hash = hash;

    StringId local_id = get_local_string_id(id);

    interner -> entries[local_id] = (StringEntry) {
        .str = {
            .ptr = str.ptr,
            .len = str.len,
        },
        .hash = hash
    };

    return id;
}

StringId string_lookup_str8(StringInterner* interner, str8 str) {
    assert(interner != null);
    assert(str.ptr != null);
    assert(str.len != 0);

    debug_log("Looking for string: %.*s", str.len, str.ptr);

    u32 hash  = hash_str8(str);
    u32 mask  = interner -> bucket_capacity - 1;
    u32 index = hash & mask;

    while (get_local_string_id(interner -> buckets[index].id) != STRING_ID_NONE) {
        StringBucket bucket = interner -> buckets[index];

        if (bucket.hash == hash) {
            StringId local_id = get_local_string_id(bucket.id);

            StringEntry entry = interner -> entries[local_id];

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
 *  Intering strings from tokens
 * 
 */

StringId string_intern_token(StringInterner* interner, File* file, Token token) {
    str8 str = {
        .ptr = file -> buffer.ptr + token.start,
        .len = token.length
    };

    assert(str.ptr < (file -> buffer.ptr + file -> buffer.len));
    assert(str.ptr + str.len < (file -> buffer.ptr + file -> buffer.len));

    return string_intern_str8(interner, str);
}

StringId string_lookup_token(StringInterner* interner, File* file, Token token) {
    str8 str = {
        .ptr = file -> buffer.ptr + token.start,
        .len = token.length
    };

    assert(str.ptr < (file -> buffer.ptr + file -> buffer.len));
    assert(str.ptr + str.len < (file -> buffer.ptr + file -> buffer.len));

    return string_lookup_str8(interner, str);
}

/*
 *
 *  helper to get str8 from a StringId
 * 
 */

inline str8 string_get_str8_from_id(StringInterner* interner, StringId id) {
    return interner -> entries[get_local_string_id(id)].str;
}

/*
 *
 *  interner resizing
 * 
 */

static void resize_buckets(StringInterner* interner) {
    u32 new_capacity = interner -> bucket_capacity * 2;

    u64 new_size = new_capacity * sizeof(StringBucket);

    StringBucket* new_buckets = realloc(interner -> buckets, new_size);

    for (u32 i = 0; i < new_capacity; i += 1) {
        new_buckets[i].id = create_string_id(interner -> id, STRING_ID_NONE);
    }

    debug_log("Reallocated buckets from %lu to %lu", new_size / 2, new_size);

    for (u32 i = 0; i < interner -> count; i++) {
        StringEntry entry = interner -> entries[i];

        u32 new_index = entry.hash & (new_capacity - 1);

        while (get_local_string_id(new_buckets[new_index].id) != STRING_ID_NONE) {
            new_index = (new_index + 1) & (new_capacity - 1);
        }

        new_buckets[new_index].id = create_string_id(interner -> id, i);
        new_buckets[new_index].hash = entry.hash;
    }

    interner -> buckets = new_buckets;
    interner -> bucket_capacity = new_capacity;

    interner -> resize_threshold_as_u32 = (u32)(new_capacity * load_factor);
}

static void resize_entries(StringInterner* interner) {
    u64 old_size = sizeof(StringEntry) * interner -> entry_capacity; 
    u64 new_size = old_size * 2;

    interner -> entries = realloc(interner -> entries, new_size);
    interner -> entry_capacity *= 2;

    debug_log("Reallocated entries from %lu to %lu bytes", old_size, new_size);
}

/*
 *
 * New helpers to make use of the new StringId system
 *
 */

inline u8 get_string_interner_id(StringId id) {
    return (id >> LOCAL_STRING_ID_BIT_OFFSET) & U8_MAX;
}

inline StringId get_local_string_id(StringId id) {
    return id & STRING_ID_NONE;
}

inline StringId create_string_id(u8 interner_id, StringId string_id) {
    return (interner_id << LOCAL_STRING_ID_BIT_OFFSET) | string_id;
}

bool are_strings_equal(StringId a, StringId b) {
    if (a == b) {
        return true;
    }

    u8 interner_b_id = get_string_interner_id(b);
    u8 interner_a_id = get_string_interner_id(a);

    if (interner_a_id == interner_b_id) {
        return false;
    }

    StringEntry entry_a = driver.string_interners[interner_a_id].entries[get_local_string_id(a)];
    StringEntry entry_b = driver.string_interners[interner_b_id].entries[get_local_string_id(b)];

    if (
        entry_a.hash == entry_b.hash && 
        entry_a.str.len == entry_b.str.len && 
        memcmp(entry_a.str.ptr, entry_b.str.ptr, entry_b.str.len) == 0
    ) {
        return true;
    }

    return false;
}
