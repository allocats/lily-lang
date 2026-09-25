#ifndef LILY_PARSER_H
#define LILY_PARSER_H

#include "ast/kinds.h"
#include "ast/types.h"
#include "files/types.h"
#include "ids.h"
#include "parser/types.h"
#include "tokens/kinds.h"

/*
 *
 * main entry point into parser
 *
 */
void parse_file(File* file);

/*
 *
 * AST wrappers for the parser for convenience
 *
 */
AstNodeId parser_create_node(Parser* p);
AstNode*  parser_get_node(Parser* p, AstNodeId id);
bool      parser_is_node(Parser* p, AstNodeId id, AstKind kind);
void      parser_set_node_flags(Parser* p, AstNodeId id, AstFlag flags);
void      parser_init_id_list(Parser* p, AstNodeIdList* list, u32 count);

/*
 *
 *  Parser movement through source tokens, do note 
 *  that all of these functions can assert and cause 
 *  program exit failure
 *
 */
Token parser_peek(Parser* p);
Token parser_peek_ahead_by(Parser* p, u32 offset);
Token parser_peek_behind_by(Parser* p, u32 offset);
Token parser_advance(Parser* p);
bool  parser_check(Parser* p, TokenKind kind);

#endif // !LILY_PARSER_H
