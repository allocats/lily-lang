#ifndef LILY_STRING_INTERNER_LOCAL_TYPES_H
#define LILY_STRING_INTERNER_LOCAL_TYPES_H

#include "meowrena/meowrena.h"
#include "string_interner/commmon_types.h"
#include "utils/types.h"

typedef struct {
    StringBucket* buckets;
    StringEntry* entries;

    u32 entry_capacity;
    u32 bucket_capacity;

    u32 count;

    u32 resize_threshold_as_u32;

    u8 id;
} LocalStringInterner;

#endif // !LILY_STRING_INTERNER_LOCAL_TYPES_H
