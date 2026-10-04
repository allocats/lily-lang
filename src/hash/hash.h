#ifndef LILY_HASH_H
#define LILY_HASH_H

#include "utils/types.h"

/*
 *
 * String hashing
 *
 */
u32 hash_str8(str8 str);

/*
 *
 * U32 hashing, mostly for IDs
 *
 */
u32 hash_u32(u32 n);
u32 hash_u32_with_base(u32 base, u32 n);

#endif // !LILY_HASH_H
