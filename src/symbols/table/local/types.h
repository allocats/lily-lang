#ifndef LILY_SYMBOLS_TABLE_LOCAL_TYPES_H
#define LILY_SYMBOLS_TABLE_LOCAL_TYPES_H

#include "meowrena/meowrena.h"
#include "symbols/symbol/types.h"
#include "symbols/table/common_types.h"

typedef struct {
    Scope *scopes;
    Symbol *symbols;

    u32 scope_count;
    u32 scope_capacity;

    u32 symbol_count;
    u32 symbol_capacity;

    u32 id;

    Arena scope_data_arena;
} __attribute__((aligned(64))) LocalSymbolTable;

#endif // !LILY_SYMBOLS_TABLE_LOCAL_TYPES_H
