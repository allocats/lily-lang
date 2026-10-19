#include "symbols/symbol/symbol.h"
#include "ids.h"
#include "symbols/symbol/kinds.h"
#include "utils/macros.h"

TypeId symbol_get_type(GlobalSymbolTable *table, SymbolId id) {
    Symbol symbol = table->symbols[id];

    switch (symbol.kind) {
    case SYMBOL_ERROR:
        return TYPE_ID_NONE;

    case SYMBOL_ENUM:
        return symbol.as.enum_symbol.resolved_type_id;

    case SYMBOL_TYPE:
        return symbol.as.type_symbol.type_id;

    case SYMBOL_FIELD:
        return symbol.as.field_symbol.type_id;

    case SYMBOL_FUNCTION:
        return symbol.as.function_symbol.return_type_id;

    case SYMBOL_PARAMETER:
        return symbol.as.parameter_symbol.type_id;

    case SYMBOL_STRUCT:
        return symbol.as.struct_symbol.resolved_type_id;

    case SYMBOL_UNION:
        return symbol.as.union_symbol.resolved_type_id;

    case SYMBOL_VARIABLE:
        return symbol.as.variable_symbol.type_id;

    case SYMBOL_VARIANT:
        return symbol.as.variant_symbol.type_id;

    case SYMBOL_IMPORT:
        UNREACHABLE("Found import O_O");
    }
}

ScopeId symbol_get_members_scope(Symbol *symbol) {
    switch (symbol->kind) {
    case SYMBOL_IMPORT:
        return symbol->as.import_symbol.scope_id;

    case SYMBOL_STRUCT:
        return symbol->as.struct_symbol.scope_id;

    case SYMBOL_UNION:
        return symbol->as.union_symbol.scope_id;

    case SYMBOL_ENUM:
        return symbol->as.enum_symbol.scope_id;

    default:
        return GLOBAL_SCOPE_ID_NONE;
    }
}
