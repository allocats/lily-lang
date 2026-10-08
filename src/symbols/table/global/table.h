#ifndef LILY_SYMBOLS_TABLE_GLOBAL_H
#define LILY_SYMBOLS_TABLE_GLOBAL_H

#include "symbols/table/global/types.h"

void global_symbol_table_init(GlobalSymbolTable* table);
void global_symbol_table_destroy(GlobalSymbolTable* table);

SymbolId global_symbol_table_create_symbol(GlobalSymbolTable* table);
ScopeId  global_symbol_table_create_scope(GlobalSymbolTable* table);

SymbolId global_scope_intern_from_local_symbol(GlobalSymbolTable* table, ScopeId scope_id, Symbol* local_symbol);
SymbolId global_scope_intern(GlobalSymbolTable* table, ScopeId scope_id, StringId global_string_id);
SymbolId global_scope_lookup(GlobalSymbolTable* table, ScopeId scope_id, StringId global_string_id);

ScopeId global_scope_from_local_scope(ScopeId id);

void global_scope_init(GlobalSymbolTable* table, ScopeId id);

#endif // !LILY_SYMBOLS_TABLE_GLOBAL_H
