#ifndef LILY_TYPE_SYSTEM_TYPES_TYPES_H
#define LILY_TYPE_SYSTEM_TYPES_TYPES_H

#include "types/entries/kinds.h"

typedef struct {
    TypeId id;
    TypeKind kind;

    u16 alignment;
    u32 size;

    u32 hash;

    StringId name_id;
    SymbolId symbol_id;

    union {
        TypeBase base_type;
        TypePointer pointer_type;
        TypeArray array_type;
        TypeStruct struct_type;
        TypeUnion union_type;
        TypeEnum enum_type;
        TypeFunction function_type;
    } as;
} Type;

[[deprecated("Do not use this, just to see size here")]]
static constexpr u64 __type_size = sizeof(Type);

#endif // !LILY_TYPE_SYSTEM_TYPES_TYPES_H
