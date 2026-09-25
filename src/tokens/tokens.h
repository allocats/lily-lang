#ifndef LILY_TOKENS_H
#define LILY_TOKENS_H

#include "files/types.h"
#include "tokens/types.h"

void token_array_init(TokenArray* array);
void source_location_array_init(SourceLocationArray* array);

void token_array_destroy(TokenArray* array);
void source_location_array_destroy(SourceLocationArray* array);

i64  get_integer_literal_from_token(File* file, Token token);
f64  get_floating_literal_from_token(File* file, Token token);
char get_character_literal_from_token(File* file, Token token);

#endif // !LILY_TOKENS_H
