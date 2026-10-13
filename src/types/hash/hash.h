#ifndef LILY_TYPES_HASH_H
#define LILY_TYPES_HASH_H

#include "ids.h"
#include "types/entries/kinds.h"

u32 hash_nominal_entry(TypeKind kind, SymbolId symbol_id, StringId name_id);

#endif // !LILY_TYPES_HASH_H
