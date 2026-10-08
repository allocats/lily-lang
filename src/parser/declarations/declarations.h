#ifndef LILY_PARSER_DECLARATIONS_H
#define LILY_PARSER_DECLARATIONS_H

#include "ids.h"
#include "parser/types.h"

AstNodeId parse_top_level_declaration(Parser *p, AstNodeId id);
AstNodeId parse_directive_declaration(Parser *p, AstNodeId id, StringId name);
AstNodeId parse_function_declaration(Parser* p, AstNodeId id, StringId name);
AstNodeId parse_struct_declaration(Parser* p, AstNodeId id, StringId name);
AstNodeId parse_union_declaration(Parser* p, AstNodeId id, StringId name);
AstNodeId parse_enum_declaration(Parser* p, AstNodeId id, StringId name);

AstNodeId parse_variable_declaration(Parser* p);

#endif // !LILY_PARSER_DECLARATIONS_H
