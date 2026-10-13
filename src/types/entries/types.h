#ifndef LILY_TYPE_SYSTEM_TYPES_TYPES_H
#define LILY_TYPE_SYSTEM_TYPES_TYPES_H

#include "types/entries/kinds.h"
#include <assert.h>

typedef struct {
    // TypeId id;
    TypeKind kind;

    u16 alignment;
    u32 size;

    u32 hash;

    /*
     *
     * GLOBAL_SYMBOL_ID_NONE for structural types!
     *
     */
    SymbolId symbol_id;

    union {
        TypePointer pointer_type;
        TypeArray array_type;
        TypeStruct struct_type;
        TypeUnion union_type;
        TypeEnum enum_type;
        TypeFunction function_type;
    } as;
} Type;

static_assert(32 == sizeof(Type)); // Size of Type changed!

#endif // !LILY_TYPE_SYSTEM_TYPES_TYPES_H
