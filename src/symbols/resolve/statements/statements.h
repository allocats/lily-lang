#ifndef LILY_SYMBOLS_RESOLVE_STATEMENTS_H
#define LILY_SYMBOLS_RESOLVE_STATEMENTS_H

#include "symbols/resolve/types.h"

bool resolve_variable_declaration(Resolver *r, AstNode *node);
bool resolve_defer_statement(Resolver *r, AstNode *node);
bool resolve_return_statement(Resolver *r, AstNode *node);
bool resolve_if_statement(Resolver *r, AstNode *node);
bool resolve_while_loop(Resolver *r, AstNode *node);

#endif // !LILY_SYMBOLS_RESOLVE_STATEMENTS_H
