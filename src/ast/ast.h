#ifndef LILY_AST_H
#define LILY_AST_H

#include "ast/types.h"
#include "ids.h"

void ast_init(Ast* ast);
void ast_destroy(Ast* ast);

AstNodeId ast_alloc_node(Ast* ast);
AstNode*  ast_get_node(Ast* ast, AstNodeId id);

void ast_id_list_init(Ast* ast, AstNodeIdList* list, u32 count);
void ast_append_id_list(Ast* ast, AstNodeIdList* list, AstNodeId id);

#endif // !LILY_AST_H
