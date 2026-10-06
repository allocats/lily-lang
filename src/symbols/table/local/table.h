#ifndef LILY_SYMBOLS_TABLE_LOCAL_H
#define LILY_SYMBOLS_TABLE_LOCAL_H

#include "symbols/table/local/types.h"

static constexpr u32 LOCAL_SCOPE_ID_BIT_OFFSET  = 24u;
static constexpr u32 LOCAL_SYMBOL_ID_BIT_OFFSET = 24u;

void local_symbol_table_init(LocalSymbolTable* table, u32 scope_count_estimate, u32 symbol_count_estimate, u8 id);
void local_symbol_table_destroy(LocalSymbolTable* table);

SymbolId local_symbol_table_create_symbol(LocalSymbolTable* table);
ScopeId  local_symbol_table_create_scope(LocalSymbolTable* table);

void local_scope_init(LocalSymbolTable* table, u32 index);

ScopeId local_scope_enter(LocalSymbolTable* table, ScopeId parent);
ScopeId local_scope_exit(LocalSymbolTable* table, ScopeId id);

SymbolId local_table_lookup(LocalSymbolTable* table, ScopeId scope_id, StringId global_string_id);
SymbolId local_scope_intern(LocalSymbolTable* table, ScopeId scope_id, StringId global_string_id);
SymbolId local_scope_lookup(LocalSymbolTable* table, ScopeId scope_id, StringId global_string_id);

Symbol*  local_symbol_get(SymbolId id);

/*
 * 
 * Helpers to use/access local ID's
 *
 */


/*
 * Gets the thread table ID from a Scope ID
 */
u8       local_scope_get_table_id(ScopeId id);


/*
 * Gets the local ScopeId from an encoded SymbolId
 */
ScopeId  local_scope_get_id(ScopeId id);


/*
 * Creates an encoded ScopeId baking in the table and local IDs 
 */
ScopeId  local_scope_create_id(u8 table_id, ScopeId local_scope_id);


/*
 * Gets the thread table ID from a Symbol ID
 */
u8       local_symbol_get_table_id(SymbolId id);


/*
 * Gets the local Symbol ID from an encoded Symbol ID
 */
SymbolId local_symbol_get_id(SymbolId id);


/*
 * Creates an encoded SymbolId baking in the table and local IDs 
 */
SymbolId local_symbol_create_id(u8 table_id, SymbolId local_symbol_id);


#endif // !LILY_SYMBOLS_TABLE_LOCAL_H
