#ifndef LILY_SYMBOLS_TABLE_GLOBAL_H
#define LILY_SYMBOLS_TABLE_GLOBAL_H

#include "string_interner/global/types.h"
#include "symbols/table/global/types.h"

/*
 *
 * Creates the global symbol table, also performs the merge of local scopes and symbols
 * into the global table, and frees the local tables
 *
 */
void global_symbol_table_init(GlobalSymbolTable *table);
void global_symbol_table_destroy(GlobalSymbolTable *table);
void global_symbol_table_register_builtins(GlobalSymbolTable *table, GlobalStringInterner *interner);

SymbolId global_symbol_table_create_symbol(GlobalSymbolTable *table);
ScopeId global_symbol_table_create_scope(GlobalSymbolTable *table);

SymbolId global_scope_intern_from_local_symbol(GlobalSymbolTable *table, ScopeId scope_id, Symbol *local_symbol);
SymbolId global_scope_intern(GlobalSymbolTable *table, ScopeId scope_id, StringId global_string_id);
SymbolId global_scope_lookup(GlobalSymbolTable *table, ScopeId scope_id, StringId global_string_id);
SymbolId global_scope_add_symbol(GlobalSymbolTable *table, ScopeId scope_id, SymbolId id);

ScopeId global_scope_from_local_scope(ScopeId id);

void global_scope_init(GlobalSymbolTable *table, ScopeId id);

#endif // !LILY_SYMBOLS_TABLE_GLOBAL_H
