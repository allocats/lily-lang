#include "types/table/table.h"
#include "hash/hash.h"
#include "ids.h"
#include "types/entries/family.h"
#include "types/entries/kinds.h"
#include "types/entries/types.h"
#include "types/table/types.h"
#include "utils/macros.h"
#include "utils/types.h"
#include "utils/wrappers.h"

#include <assert.h>
#include <string.h>

/*
 *
 * default initial values for sizing
 *
 */
static constexpr u32 init_capacity         = 128;
static constexpr f32 load_factor           = 0.75f;
static constexpr u32 init_resize_threshold = (u64)(init_capacity * load_factor);
static constexpr u64 bucket_size           = sizeof(TypeBucket) * init_capacity;

static void resize_entries(TypeTable *table);
static void resize_nominal_buckets(TypeTable *table);
static void resize_structural_buckets(TypeTable *table);

/*
 *
 * Creation and deletion
 *
 */
void type_table_init(TypeTable *table) {
    assert(table != null);

    arena_init(&table->gpa, ARENA_KB(1), ALIGN_DEFAULT);

    table->entries            = lily_calloc(init_capacity, sizeof(Type));
    table->nominal_buckets    = lily_alloc(bucket_size);
    table->structural_buckets = lily_alloc(bucket_size);

    table->entry_count                   = 0;
    table->entry_capacity                = init_capacity;
    table->entry_resize_threshold_as_u32 = init_resize_threshold;

    table->nominal_count                   = 0;
    table->nominal_capacity                = init_capacity;
    table->nominal_resize_threshold_as_u32 = init_resize_threshold;

    table->structural_count                   = 0;
    table->structural_capacity                = init_capacity;
    table->structural_resize_threshold_as_u32 = init_resize_threshold;

    memset(table->nominal_buckets, U8_MAX, bucket_size);
    memset(table->structural_buckets, U8_MAX, bucket_size);
}

void type_table_destroy(TypeTable *table) {
    arena_destroy(&table->gpa);

    lily_free(table->entries);
    lily_free(table->nominal_buckets);
    lily_free(table->structural_buckets);
}

/*
 *
 * Interning
 *
 */

TypeId type_table_intern_nominal(TypeTable *table, TypeKind kind, SymbolId symbol_id) {
    assert(table != null);
    assert(kind < TYPE_KIND_COUNT);
    assert(symbol_id != GLOBAL_SYMBOL_ID_NONE);

    if (UNLIKELY(table->nominal_count >= table->nominal_resize_threshold_as_u32)) {
        resize_nominal_buckets(table);
    }

    u32 hash  = hash_u32(symbol_id);
    u32 mask  = table->nominal_capacity - 1;
    u32 index = hash & mask;

    while (table->nominal_buckets[index].id != TYPE_ID_NONE) {
        TypeBucket bucket = table->nominal_buckets[index];

        if (bucket.hash == hash) {
            Type entry = table->entries[bucket.id];

            if (entry.kind == kind && entry.symbol_id == symbol_id) {
                return bucket.id;
            }
        }

        index = (index + 1) & mask;
    }

    if (UNLIKELY(table->entry_count >= table->entry_resize_threshold_as_u32)) {
        resize_entries(table);
    }

    TypeId id = table->entry_count++;

    table->nominal_buckets[index].id   = id;
    table->nominal_buckets[index].hash = hash;

    table->nominal_count += 1;

    Type *type = &table->entries[id];

    type->hash = hash;
    type->kind = kind;

    type->symbol_id = symbol_id;

    type->alignment = 0;
    type->size      = 0;

    return id;
}

TypeId type_table_intern_pointer(TypeTable *table, TypeId pointee_type_id) {
    assert(table != null);
    assert(pointee_type_id != TYPE_ID_NONE);

    if (UNLIKELY(table->structural_count >= table->structural_resize_threshold_as_u32)) {
        resize_structural_buckets(table);
    }

    u32 hash  = hash_u32_with_base(hash_u32(TYPE_POINTER), pointee_type_id);
    u32 mask  = table->structural_capacity - 1;
    u32 index = hash & mask;

    while (table->structural_buckets[index].id != TYPE_ID_NONE) {
        TypeBucket bucket = table->structural_buckets[index];

        if (bucket.hash == hash) {
            Type entry = table->entries[bucket.id];

            if (entry.kind == TYPE_POINTER && entry.as.pointer_type.pointee_id == pointee_type_id) {
                return bucket.id;
            }
        }

        index = (index + 1) & mask;
    }

    if (UNLIKELY(table->entry_count >= table->entry_resize_threshold_as_u32)) {
        resize_entries(table);
    }

    TypeId id = table->entry_count++;

    table->structural_buckets[index].id   = id;
    table->structural_buckets[index].hash = hash;

    table->structural_count += 1;

    Type *type = &table->entries[id];

    type->hash = hash;
    type->kind = TYPE_POINTER;

    type->symbol_id = GLOBAL_SYMBOL_ID_NONE;

    type->alignment = alignof(void *);
    type->size      = sizeof(void *);

    type->as.pointer_type.pointee_id = pointee_type_id;

    return id;
}

TypeId type_table_intern_slice(TypeTable *table, TypeId element_type_id) {
    assert(table != null);
    assert(element_type_id != TYPE_ID_NONE);

    if (UNLIKELY(table->structural_count >= table->structural_resize_threshold_as_u32)) {
        resize_structural_buckets(table);
    }

    u32 hash  = hash_u32_with_base(hash_u32(TYPE_SLICE), element_type_id);
    u32 mask  = table->structural_capacity - 1;
    u32 index = hash & mask;

    while (table->structural_buckets[index].id != TYPE_ID_NONE) {
        TypeBucket bucket = table->structural_buckets[index];

        if (bucket.hash == hash) {
            Type entry = table->entries[bucket.id];

            if (entry.kind == TYPE_SLICE && entry.as.slice_type.element_id == element_type_id) {
                return bucket.id;
            }
        }

        index = (index + 1) & mask;
    }

    if (UNLIKELY(table->entry_count >= table->entry_resize_threshold_as_u32)) {
        resize_entries(table);
    }

    TypeId id = table->entry_count++;

    table->structural_buckets[index].id   = id;
    table->structural_buckets[index].hash = hash;

    table->structural_count += 1;

    Type *type = &table->entries[id];

    type->hash = hash;
    type->kind = TYPE_SLICE;

    type->symbol_id = GLOBAL_SYMBOL_ID_NONE;

    type->alignment = alignof(void *);
    type->size      = sizeof(void *);

    type->as.slice_type.element_id = element_type_id;

    return id;
}

/*
 *
 * Lookups
 *
 */

TypeId type_table_lookup_nominal(TypeTable *table, SymbolId symbol_id) {
    assert(table != null);
    assert(symbol_id != GLOBAL_SYMBOL_ID_NONE);

    u32 hash  = hash_u32(symbol_id);
    u32 mask  = table->nominal_capacity - 1;
    u32 index = hash & mask;

    while (table->nominal_buckets[index].id != TYPE_ID_NONE) {
        TypeBucket bucket = table->nominal_buckets[index];

        if (bucket.hash == hash) {
            Type entry = table->entries[bucket.id];

            if (entry.symbol_id == symbol_id) {
                return bucket.id;
            }
        }

        index = (index + 1) & mask;
    }

    return TYPE_ID_NONE;
}

TypeId type_table_lookup_pointer(TypeTable *table, TypeId pointee_type_id) {
    assert(table != null);
    assert(pointee_type_id != TYPE_ID_NONE);

    u32 hash  = hash_u32_with_base(hash_u32(TYPE_POINTER), pointee_type_id);
    u32 mask  = table->structural_capacity - 1;
    u32 index = hash & mask;

    while (table->structural_buckets[index].id != TYPE_ID_NONE) {
        TypeBucket bucket = table->structural_buckets[index];

        if (bucket.hash == hash) {
            Type entry = table->entries[bucket.id];

            if (entry.kind == TYPE_POINTER && entry.as.pointer_type.pointee_id == pointee_type_id) {
                return bucket.id;
            }
        }

        index = (index + 1) & mask;
    }

    return TYPE_ID_NONE;
}

TypeId type_table_lookup_slice(TypeTable *table, TypeId element_type_id) {
    assert(table != null);
    assert(element_type_id != TYPE_ID_NONE);

    u32 hash  = hash_u32_with_base(hash_u32(TYPE_SLICE), element_type_id);
    u32 mask  = table->structural_capacity - 1;
    u32 index = hash & mask;

    while (table->structural_buckets[index].id != TYPE_ID_NONE) {
        TypeBucket bucket = table->structural_buckets[index];

        if (bucket.hash == hash) {
            Type entry = table->entries[bucket.id];

            if (entry.kind == TYPE_SLICE && entry.as.slice_type.element_id == element_type_id) {
                return bucket.id;
            }
        }

        index = (index + 1) & mask;
    }

    return TYPE_ID_NONE;
}

/*
 *
 * Resizing functions
 *
 */

static void resize_entries(TypeTable *table) {
    u64 old_size = table->entry_capacity * sizeof(Type);
    u64 new_size = old_size * 2;

    table->entries = lily_realloc(table->entries, new_size);
    table->entry_capacity *= 2;

    table->entry_resize_threshold_as_u32 = (u32)(table->entry_capacity * load_factor);
}

static void resize_nominal_buckets(TypeTable *table) {
    u64 old_size = table->nominal_capacity * sizeof(TypeBucket);
    u64 new_size = old_size * 2;

    u32 new_capacity = table->nominal_capacity * 2;

    u32 mask = new_capacity - 1;

    TypeBucket *buckets = lily_realloc(table->nominal_buckets, new_size);
    memset(buckets, U8_MAX, new_size);

    for (u32 i = 0; i < table->entry_count; i += 1) {
        Type entry = table->entries[i];

        if (type_family_lut[entry.kind] != TYPE_FAMILY_NOMINAL) {
            continue;
        }

        u32 index = entry.hash & mask;

        while (buckets[index].id != TYPE_ID_NONE) {
            index = (index + 1) & mask;
        }

        buckets[index].id   = i;
        buckets[index].hash = entry.hash;
    }

    table->nominal_buckets                 = buckets;
    table->nominal_capacity                = new_capacity;
    table->nominal_resize_threshold_as_u32 = (u32)(new_capacity * load_factor);
}

static void resize_structural_buckets(TypeTable *table) {
    u64 old_size = table->structural_capacity * sizeof(TypeBucket);
    u64 new_size = old_size * 2;

    u32 new_capacity = table->structural_capacity * 2;

    u32 mask = new_capacity - 1;

    TypeBucket *buckets = lily_realloc(table->structural_buckets, new_size);
    memset(buckets, U8_MAX, new_size);

    for (u32 i = 0; i < table->entry_count; i += 1) {
        Type entry = table->entries[i];

        if (type_family_lut[entry.kind] != TYPE_FAMILY_STRUCTURAL) {
            continue;
        }

        u32 index = entry.hash & mask;

        while (buckets[index].id != TYPE_ID_NONE) {
            index = (index + 1) & mask;
        }

        buckets[index].id   = i;
        buckets[index].hash = entry.hash;
    }

    table->structural_buckets                 = buckets;
    table->structural_capacity                = new_capacity;
    table->structural_resize_threshold_as_u32 = (u32)(new_capacity * load_factor);
}
