#ifndef LILY_SYMBOLS_RESOLVE_ERRORS_H
#define LILY_SYMBOLS_RESOLVE_ERRORS_H

#include "symbols/resolve/types.h"

void resolver_error_symbol_redefined(Resolver *r, AstNode *node, SymbolId symbol_id, StringId name);
void resolver_undeclared_identifier(Resolver *r, AstNode *node, StringId name_id);
void resolver_mismatched_types(Resolver *r, AstNode *node, TypeId expected, TypeId found);

#endif // !LILY_SYMBOLS_RESOLVE_ERRORS_H
