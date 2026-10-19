#include "ids.h"
#include "resolver_stack/types.h"
#include "string_interner/global/interner.h"
#include "string_interner/global/types.h"
#include "symbols/symbol/kinds.h"
#include "symbols/table/global/table.h"
#include "symbols/table/global/types.h"
#include "types/builtins/types.h"
#include "types/entries/kinds.h"
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

        Symbol *symbol = &symbol_table->symbols[symbol_id];

        symbol->state                  = RESOLVE_RESOLVED;
        symbol->as.type_symbol.type_id = id;

        TypeId *pointer_hack = (TypeId *)builtin_ids;

        pointer_hack[i] = id;
    }

    /*
     *
     * 'string' type
     * ALL strings will use this type, a simple `ptr` + `len` struct
     *
     *  string :: struct {
     *      ptr: *u8;
     *      len: usize;
     *  }
     *
     */
    {
        TypeId u8_ptr_id = type_table_intern_pointer(table, builtin_ids->type_u8);
        TypeId usize_id  = builtin_ids->type_usize;

        StringId name_id = global_string_intern_cstr(interner, "string");
        assert(name_id != GLOBAL_STRING_ID_NONE);

        SymbolId symbol_id = global_scope_intern(symbol_table, COMPILER_SCOPE_ID, name_id);
        assert(symbol_id != GLOBAL_STRING_ID_NONE);

        TypeId type_id = type_table_intern_nominal(table, TYPE_STRUCT, symbol_id);
        assert(type_id != TYPE_ID_NONE);

        /*
         *
         * Create the type
         *
         */
        Type *type = &table->entries[type_id];

        type->as.struct_type.fields      = arena_alloc(&table->gpa, sizeof(TypeId) * 2);
        type->as.struct_type.field_count = 2;

        type->as.struct_type.fields[0] = u8_ptr_id;
        type->as.struct_type.fields[1] = usize_id;

        /*
         *
         * Create the struct for `string`
         *
         */
        Symbol *symbol = &symbol_table->symbols[symbol_id];

        symbol->kind  = SYMBOL_STRUCT;
        symbol->state = RESOLVE_RESOLVED;

        symbol->as.struct_symbol.fields           = arena_alloc(&symbol_table->symbol_data_arena, sizeof(SymbolId) * 2);
        symbol->as.struct_symbol.field_count      = 2;
        symbol->as.struct_symbol.resolved_type_id = type_id;

        /*
         *
         * Create the scope for the `string` struct symbol
         *
         */
        ScopeId scope_id = global_symbol_table_create_scope(symbol_table);

        symbol->as.struct_symbol.scope_id = scope_id;

        /*
         *
         * Create the StringIds and SymbolIds for the fields `ptr` + `len`
         *
         */
        StringId ptr_name_id = global_string_intern_cstr(interner, "ptr");
        StringId len_name_id = global_string_intern_cstr(interner, "len");

        SymbolId ptr_field_id = global_scope_intern(symbol_table, scope_id, ptr_name_id);
        SymbolId len_field_id = global_scope_intern(symbol_table, scope_id, len_name_id);

        /*
         *
         * Create the symbol for the `ptr` field
         *
         */

        Symbol *ptr_symbol = &symbol_table->symbols[ptr_field_id];

        ptr_symbol->kind  = SYMBOL_FIELD;
        ptr_symbol->state = RESOLVE_RESOLVED;

        ptr_symbol->as.field_symbol.index   = 0;
        ptr_symbol->as.field_symbol.type_id = u8_ptr_id;

        /*
         *
         * Create the symbol for the `len` field
         *
         */
        Symbol *len_symbol = &symbol_table->symbols[len_field_id];

        len_symbol->kind  = SYMBOL_FIELD;
        len_symbol->state = RESOLVE_RESOLVED;

        len_symbol->as.field_symbol.index   = 1;
        len_symbol->as.field_symbol.type_id = usize_id;

        // Refetch the struct symbol as it could have been moved in a reallocation
        symbol = &symbol_table->symbols[symbol_id];

        symbol->as.struct_symbol.fields[0] = ptr_field_id;
        symbol->as.struct_symbol.fields[1] = len_field_id;

        builtin_ids->type_string = type_id;
    }
}
