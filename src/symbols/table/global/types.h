#ifndef LILY_SYMBOLS_TABLE_GLOBAL_TYPES_H
#define LILY_SYMBOLS_TABLE_GLOBAL_TYPES_H

#include "meowrena/meowrena.h"
#include "symbols/symbol/types.h"
#include "symbols/table/common_types.h"

/*
 *
 * Static variable in `global/table.c` that translates local ids into global ids
 * 
 */
typedef struct {
    ScopeId **scope_ids;
} GlobalSymbolTableRemapper;

typedef struct {
    Scope *scopes;
    Symbol *symbols;

    u32 scope_count;
    u32 scope_capacity;

    u32 symbol_count;
    u32 symbol_capacity;

    Arena scope_data_arena;
    Arena symbol_data_arena;
} GlobalSymbolTable;

#endif // !LILY_SYMBOLS_TABLE_GLOBAL_TYPES_H
