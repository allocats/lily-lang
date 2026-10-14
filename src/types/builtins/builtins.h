#ifndef LILY_TYPES_BUILTINS_H
#define LILY_TYPES_BUILTINS_H

#include "string_interner/global/types.h"
#include "symbols/table/global/types.h"
#include "types/builtins/types.h"
#include "types/table/types.h"

void type_table_register_builtin_types(
    TypeTable *table,
    GlobalStringInterner *interner,
    GlobalSymbolTable *symbol_table,
    BuiltinTypeIds *builtin_ids
);

#endif // !LILY_TYPES_BUILTINS_H
