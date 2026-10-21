#ifndef LILY_SYMBOLS_RESOLVE_EXPRESSIONS_H
#define LILY_SYMBOLS_RESOLVE_EXPRESSIONS_H

#include "ids.h"
#include "symbols/resolve/types.h"

/*
 *
 * Resolve an expression and return the resulting TypeId.
 * Returns `TYPE_ID_NONE` if there is an error.
 *
 * @expected_type: pass in the expected type for basic inference, else `TYPE_ID_NONE` for none
 *
 */
TypeId resolve_expression(Resolver *r, AstNode *node, TypeId expected_type);

#endif // !LILY_SYMBOLS_RESOLVE_EXPRESSIONS_H
