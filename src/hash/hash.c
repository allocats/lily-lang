#include "hash/hash.h"
#include "utils/types.h"

#include <immintrin.h>
#include <string.h>

u32 hash_str8(str8 str) {
    u64 hash = U64_MAX;

    char* ptr = str.ptr;
    u32 len = str.len;

    while (len >= 8) {
        u64 chunk;
        memcpy(&chunk, ptr, 8);

        hash = _mm_crc32_u64(hash, chunk);

        ptr += 8;
        len -= 8;
    }

    while (len >= 4) {
        u32 chunk;
        memcpy(&chunk, ptr, 4);

        hash = _mm_crc32_u32((u32) hash, chunk);

        ptr += 4;
        len -= 4;
    }

    while (len--) {
        hash = _mm_crc32_u8((u32) hash, *ptr++);
    }

    return (u32)(hash);
}

/*
 *
 * U32 Hashing, used for ID hashing
 *
 */

inline u32 hash_u32(u32 n) {
    return _mm_crc32_u32(U32_MAX, n);
}

inline u32 hash_u32_with_base(u32 base, u32 n) {
    return _mm_crc32_u32(base, n);
}
