#ifndef LILY_SYMBOLS_RESOLVE_ERRORS_H
#define LILY_SYMBOLS_RESOLVE_ERRORS_H

#include "symbols/resolve/types.h"

void resolver_error_symbol_redefined(Resolver *r, AstNode *node, SymbolId symbol_id, StringId name);

#endif // !LILY_SYMBOLS_RESOLVE_ERRORS_H
