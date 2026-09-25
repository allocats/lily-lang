#ifndef LILY_PARSER_DECLARATIONS_H
#define LILY_PARSER_DECLARATIONS_H

#include "parser/types.h"

AstNodeId parse_top_level_declaration(Parser *p, AstNodeId id);
AstNodeId parse_function_declaration(Parser* p, AstNodeId id, StringId name);

#endif // !LILY_PARSER_DECLARATIONS_H
