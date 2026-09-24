#ifndef LILY_PARSER_H
#define LILY_PARSER_H

#include "files/types.h"
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
