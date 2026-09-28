#ifndef LILY_TOKENS_H
#define LILY_TOKENS_H

#include "files/types.h"
#include "tokens/types.h"

void token_array_init(TokenArray* array, u64 capacity);

void token_array_destroy(TokenArray* array);

i64  get_integer_literal_from_token(File* file, Token token);
f64  get_floating_literal_from_token(File* file, Token token);
char get_character_literal_from_token(File* file, Token token);

#endif // !LILY_TOKENS_H
