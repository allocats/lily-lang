#ifndef LILY_SYMBOLS_TABLE_COMMON_TYPES_H
#define LILY_SYMBOLS_TABLE_COMMON_TYPES_H

#include "ids.h"

static constexpr u32 scope_init_capacity = 16;
static constexpr f32 scope_load_factor   = 0.75f;

typedef struct {
    /*
     *
     * Just grab it from the global string interner during remapping if possible
     *
     */
    u32 hash;

    /*
     *
     * This will be a global string id, not the local on the node. Make sure to remap it!
     *
     */
    StringId string_id;
} ScopeBucket;

typedef struct {
    ScopeBucket *buckets;
    SymbolId *entries;

    u32 count;
    u32 capacity; // must be power of two

    u32 resize_threshold_as_u32;

    ScopeId parent; // (GLOBAL/LOCAL)_SCOPE_ID_NONE for no parent
} Scope;

#endif // !LILY_SYMBOLS_TABLE_COMMON_TYPES_H
