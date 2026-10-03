#ifndef LILY_STRING_INTERNER_GLOBAL_TYPES_H
#define LILY_STRING_INTERNER_GLOBAL_TYPES_H

#include "meowrena/meowrena.h"
#include "string_interner/commmon_types.h"
#include "utils/types.h"

/*
 *
 * get the number of arrays from `driver.local_string_interner_count`
 *
 */
typedef struct {
    StringId** ids;
} GlobalStringRemapper;

/*
 *
 * The one true interner after parsing
 *
 */
typedef struct {
    StringBucket* buckets;
    StringEntry* entries;

    u32 entry_capacity;
    u32 bucket_capacity;

    u32 count;

    u32 resize_threshold_as_u32;
} GlobalStringInterner;

#endif // !LILY_STRING_INTERNER_GLOBAL_TYPES_H
