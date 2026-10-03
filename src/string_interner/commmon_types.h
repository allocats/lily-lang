#ifndef LILY_STRING_INTERNER_COMMON_TYPES_H
#define LILY_STRING_INTERNER_COMMON_TYPES_H

#include "ids.h"
#include "utils/types.h"

typedef struct {
    str8 str;
    u32 hash;
} StringEntry;

static_assert(sizeof(StringEntry) == 16);


typedef struct {
    StringId id;
    u32 hash;
} StringBucket;

static_assert(sizeof(StringBucket) == 8);

#endif // !LILY_STRING_INTERNER_COMMON_TYPES_H
