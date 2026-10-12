#ifndef LILY_PARSER_EXPRESSIONS_H
#define LILY_PARSER_EXPRESSIONS_H

#include "ids.h"
#include "parser/types.h"

AstNodeId parse_expression(Parser *p);
AstNodeId parse_expression_for_type_expression(Parser *p);

#endif // !LILY_PARSER_EXPRESSIONS_H
