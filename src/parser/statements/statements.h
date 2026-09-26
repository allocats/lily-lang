#ifndef LILY_PARSER_STATEMENTS_H
#define LILY_PARSER_STATEMENTS_H

#include "ids.h"
#include "parser/types.h"

/*
 *
 * Entry point for parsing statements
 *
 */
AstNodeId parse_statement(Parser* p);

AstNodeId parse_defer_statement(Parser* p);
AstNodeId parse_return_statement(Parser* p);

AstNodeId parser_for_loop(Parser* p);
AstNodeId parser_while_loop(Parser* p);

/*
 *
 * recursively making blocks! will create new blocks inside of it
 *
 * Note: the parser should be pointing at the `{` when calling this function
 *
 */
AstNodeId parse_block(Parser* p);

#endif // !LILY_PARSER_STATEMENTS_H
