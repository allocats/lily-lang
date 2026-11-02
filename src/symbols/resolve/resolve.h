#ifndef LILY_SYMBOLS_RESOLVE_H
#define LILY_SYMBOLS_RESOLVE_H

#include "symbols/resolve/types.h"
#include "workspace/types.h"

bool resolve_symbols(Workspace *ws);
bool resolve_symbol(Workspace *ws, SymbolId id);
bool resolve_function_signature(Resolver *r, SymbolId id);

bool resolve_block(Resolver *r, AstNodeId id);

ScopeId resolver_enter_scope(Resolver *r);
ScopeId resolver_exit_scope(Resolver *r);

Symbol *resolver_get_symbol(Resolver *r, SymbolId id);

#endif // !LILY_SYMBOLS_RESOLVE_H
