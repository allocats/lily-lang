#ifndef LILY_PARSER_TYPE_EXPRESSIONS_H
#define LILY_PARSER_TYPE_EXPRESSIONS_H

#include "ids.h"
#include "parser/types.h"

AstNodeId parse_type_expression(Parser *p);
AstNodeId parse_parameter_type_expression(Parser *p);

#endif // !LILY_PARSER_TYPE_EXPRESSIONS_H
