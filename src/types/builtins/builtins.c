#include "ids.h"
#include "string_interner/global/interner.h"
#include "string_interner/global/types.h"
#include "symbols/table/global/table.h"
#include "symbols/table/global/types.h"
#include "types/builtins/types.h"
#include "types/table/table.h"
#include "types/table/types.h"

#include <assert.h>

void type_table_register_builtin_types(
    TypeTable *table,
    GlobalStringInterner *interner,
    GlobalSymbolTable *symbol_table,
    BuiltinTypeIds *builtin_ids
) {
    for (u32 i = 0; i < builtin_type_count; i += 1) {
        BuiltinType type = builtin_types[i];

        StringId name_id = global_string_lookup_str8(interner, type.name);

        SymbolId symbol_id = global_scope_lookup(symbol_table, COMPILER_SCOPE_ID, name_id);

        assert(symbol_id != GLOBAL_SYMBOL_ID_NONE);

        TypeId id = type_table_intern_nominal(table, TYPE_BASE, symbol_id);

        table->entries[id].size      = type.size;
        table->entries[id].alignment = type.alignment;

        TypeId *pointer_hack = (TypeId *)builtin_ids;

        pointer_hack[i] = id;
    }
}
