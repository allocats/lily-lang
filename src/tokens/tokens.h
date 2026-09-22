#ifndef LILY_TOKENS_H
#define LILY_TOKENS_H

#include "tokens/types.h"

void token_array_init(TokenArray* array);
void source_location_array_init(SourceLocationArray* array);

void token_array_destroy(TokenArray* array);
void source_location_array_destroy(SourceLocationArray* array);

#endif // !LILY_TOKENS_H
