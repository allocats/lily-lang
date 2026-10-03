#include "driver/types.h"
#include "hash/hash.h"
#include "ids.h"
#include "string_interner/commmon_types.h"
#include "string_interner/global/interner.h"
#include "string_interner/global/types.h"
#include "string_interner/local/interner.h"
#include "string_interner/local/types.h"
#include "utils/debug.h"
#include "utils/macros.h"
#include "utils/types.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

extern Driver driver; 

static GlobalStringRemapper remapper = {0};

static constexpr f64 load_factor = 0.75f;

static StringId global_string_intern_local_entry(GlobalStringInterner* interner, StringEntry entry);

static void resize_buckets(GlobalStringInterner* interner);
static void resize_entries(GlobalStringInterner* interner);

/*
 *
 * Creation, merging all the interners and deletion
 *
 */

void global_string_interner_init(GlobalStringInterner *interner) {
    LocalStringInterner* local_interners = driver.local_string_interners;

    u32 local_count = driver.local_string_interner_count;

    remapper.ids = malloc(sizeof(StringId*) * local_count); 

    u32 local_string_count = 0;

    for (u8 i = 0; i < local_count; i += 1) {
        u32 count = local_interners[i].count;

        remapper.ids[i] = malloc(sizeof(StringId) * count);

        local_string_count += count;
    }

    /*
     *
     * i know it's over allocating, but removes all growing during the merge
     *
     */
    u32 capacity = NEXT_POWER_OF_TWO(local_string_count * 1.3f);

    interner -> buckets = malloc(sizeof(StringBucket) * capacity);
    interner -> entries = malloc(sizeof(StringEntry) * capacity);

    memset(interner -> buckets, U8_MAX, sizeof(StringBucket) * capacity);

    interner -> bucket_capacity = capacity;
    interner -> entry_capacity  = capacity;

    interner -> count = 0;

    interner -> resize_threshold_as_u32 = (u32)(capacity * load_factor);

    for (u8 i = 0; i < local_count; i += 1) {
        LocalStringInterner current_interner = local_interners[i];

        for (u32 n = 0; n < current_interner.count; n += 1) {
            StringEntry entry = current_interner.entries[n];

            remapper.ids[i][n] = global_string_intern_local_entry(interner, entry);
        }
    }

    debug_log("Created GlobalStringInterner with %u strings and %u capacity", interner -> count, interner -> entry_capacity);

    for (u8 i = 0; i < driver.local_string_interner_count; i += 1) {
        local_string_interner_destroy(&driver.local_string_interners[i]);
    }

    free(driver.local_string_interners);
}

void global_string_interner_destroy(GlobalStringInterner *interner) {
    assert(interner != null);

    u32 local_count = driver.local_string_interner_count;

    free(interner -> buckets);
    free(interner -> entries);

    for (u8 i = 0; i < local_count; i += 1) {
        free(remapper.ids[i]);
    }

    free(remapper.ids);
}

/*
 *
 * actual interning!
 *
 */

StringId global_string_intern_str8(GlobalStringInterner *interner, str8 str) {
    assert(interner != null);
    assert(str.ptr  != null);
    assert(str.len  != 0);

    if (UNLIKELY(interner -> count >= interner -> resize_threshold_as_u32)) {
        resize_buckets(interner);
    }

    u32 hash  = hash_str8(str);
    u32 mask  = interner -> bucket_capacity - 1;
    u32 index = hash & mask;

    while (interner -> buckets[index].id != GLOBAL_STRING_ID_NONE) {
        StringBucket bucket = interner -> buckets[index];

        if (bucket.hash == hash) {
            StringEntry entry = interner -> entries[bucket.id];

            if (entry.str.len == str.len && memcmp(entry.str.ptr, str.ptr, str.len) == 0) {
                return bucket.id;
            }
        }

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

StringId global_string_lookup_str8(GlobalStringInterner *interner, str8 str) {
    assert(interner != null);
    assert(str.ptr  != null);
    assert(str.len  != 0);

    u32 hash  = hash_str8(str);
    u32 mask  = interner -> bucket_capacity - 1;
    u32 index = hash & mask;

    while (interner -> buckets[index].id != GLOBAL_STRING_ID_NONE) {
        StringBucket bucket = interner -> buckets[index];

        if (bucket.hash == hash) {
            StringEntry entry = interner -> entries[bucket.id];

            if (entry.str.len == str.len && memcmp(entry.str.ptr, str.ptr, str.len) == 0) {
                return bucket.id;
            }
        }

        index = (index + 1) & mask;
    }

    return GLOBAL_STRING_ID_NONE;
}

inline StringId global_string_intern_cstr(GlobalStringInterner *interner, char *cstr) {
    return global_string_intern_str8(interner, (str8) { .ptr = cstr, .len = strlen(cstr)});
}

inline StringId global_string_lookup_cstr(GlobalStringInterner *interner, char *cstr) {
    return global_string_lookup_str8(interner, (str8) { .ptr = cstr, .len = strlen(cstr)});
}

/*
 *
 * To avoid rehashing every string
 *
 */

static StringId global_string_intern_local_entry(GlobalStringInterner* interner, StringEntry entry) {
    assert(interner != null);
    assert(entry.hash != 0);
    assert(entry.str.ptr != null);
    assert(entry.str.len != 0);

    str8 str = entry.str;

    u32 hash  = entry.hash;
    u32 mask  = interner -> bucket_capacity - 1;
    u32 index = hash & mask;

    while (interner -> buckets[index].id != GLOBAL_STRING_ID_NONE) {
        StringBucket bucket = interner -> buckets[index];

        if (bucket.hash == hash) {
            StringEntry slot = interner -> entries[bucket.id];

            if (slot.str.len == str.len && memcmp(slot.str.ptr, str.ptr, str.len) == 0) {
                return bucket.id;
            }
        }

        index = (index + 1) & mask;
    }

    StringId id = interner -> count++;

    assert(id < interner -> entry_capacity);

    interner -> buckets[index].id = id;
    interner -> buckets[index].hash = hash;

    interner -> entries[id] = entry;

    return id;
}


/*
 *
 *  interner resizing
 * 
 */

static void resize_buckets(GlobalStringInterner* interner) {
    u32 new_capacity = interner -> bucket_capacity * 2;

    u64 new_size = new_capacity * sizeof(StringBucket);

    StringBucket* new_buckets = realloc(interner -> buckets, new_size);
    memset(new_buckets, U8_MAX, new_size);

    debug_log("Reallocated GlobalStringInterner -> buckets from %lu to %lu", new_size / 2, new_size);

    for (u32 i = 0; i < interner -> count; i++) {
        StringEntry entry = interner -> entries[i];

        u32 new_index = entry.hash & (new_capacity - 1);

        while (new_buckets[new_index].id != GLOBAL_STRING_ID_NONE) {
            new_index = (new_index + 1) & (new_capacity - 1);
        }

        new_buckets[new_index].id = i;
        new_buckets[new_index].hash = entry.hash;
    }

    interner -> buckets = new_buckets;
    interner -> bucket_capacity = new_capacity;

    interner -> resize_threshold_as_u32 = (u32)(new_capacity * load_factor);
}

static void resize_entries(GlobalStringInterner* interner) {
    u64 old_size = sizeof(StringEntry) * interner -> entry_capacity; 
    u64 new_size = old_size * 2;

    interner -> entries = realloc(interner -> entries, new_size);
    interner -> entry_capacity *= 2;

    debug_log("Reallocated GlobalStringInterner -> entries from %lu to %lu bytes", old_size, new_size);
}

inline StringId global_string_from_local_id(StringId id) {
    StringId local_id = local_string_get_id(id);
    u8 interner_id = local_string_get_interner_id(id);

    return remapper.ids[interner_id][local_id];
}
