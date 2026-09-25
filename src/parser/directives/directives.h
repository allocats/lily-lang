#ifndef LILY_PARSER_DIRECTIVES_H
#define LILY_PARSER_DIRECTIVES_H

#include "ids.h"
#include "parser/types.h"

void init_directive_lut(Workspace* ws);

AstNodeId parse_directive(Parser* p);

#endif // !LILY_PARSER_DIRECTIVES_H
