#include "hash/hash.h"

#include <immintrin.h>
#include <string.h>

u32 hash_str8(str8 str) {
    u64 hash = 0xFFFFFFFF;

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

        hash = _mm_crc32_u32((u32)hash, chunk);

        ptr += 4;
        len -= 4;
    }

    while (len--) {
        hash = _mm_crc32_u8((u32) hash, *ptr++);
    }

    return (u32)(hash);
}
