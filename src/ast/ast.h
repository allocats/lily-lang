#ifndef LILY_AST_H
#define LILY_AST_H

#include "ast/types.h"
#include "ids.h"

void ast_init(Ast* ast);
void ast_destroy(Ast* ast);

AstNodeId ast_alloc_node(Ast* ast);

#endif // !LILY_AST_H
