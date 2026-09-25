#ifndef LILY_PARSER_EXPRESSIONS_H
#define LILY_PARSER_EXPRESSIONS_H

#include "ids.h"
#include "parser/types.h"

AstNodeId parse_expression(Parser* p, u32 prec);

#endif // !LILY_PARSER_EXPRESSIONS_H
