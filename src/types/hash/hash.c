#include "types/hash/hash.h"
#include "hash/hash.h"

u32 hash_nominal_entry(TypeKind kind, SymbolId symbol_id, StringId name_id) {
    u32 hash = hash_u32(kind);

    hash = hash_u32_with_base(hash, symbol_id);
    hash = hash_u32_with_base(hash, name_id);

    return hash;
}
