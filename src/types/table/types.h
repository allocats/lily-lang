#ifndef LILY_TYPES_TABLE_TYPES_H
#define LILY_TYPES_TABLE_TYPES_H

#include "ids.h"
#include "meowrena/meowrena.h"
#include "types/entries/types.h"

typedef struct {
    TypeId  id;
    u32     hash;
} TypeBucket;

typedef struct {
    TypeBucket* nominal_buckets;
    TypeBucket* structural_buckets;
    Type* entries;

    u32 nominal_count;
    u32 nominal_capacity;

    u32 structural_count;
    u32 structural_capacity;

    u32 entry_count;
    u32 entry_capacity;

    u32 nominal_resize_threshold_as_u32;
    u32 structural_resize_threshold_as_u32;
    u32 entry_resize_threshold_as_u32;

    // pthread_rwlock_t mutex;

    u8 __padding[8];

    Arena gpa;
    Arena nominal_arena;
    Arena structural_arena;
    Arena entry_arena;
} TypeTable;

#endif // !LILY_TYPES_TABLE_TYPES_H
