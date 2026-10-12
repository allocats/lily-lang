#ifndef LILY_STRING_INTERNER_LOCAL_INTERNER_H
#define LILY_STRING_INTERNER_LOCAL_INTERNER_H

#include "files/types.h"
#include "string_interner/local/types.h"

static constexpr u32 LOCAL_STRING_ID_BIT_OFFSET = 24u;

void local_string_interner_init(LocalStringInterner *interner, u8 id);
void local_string_interner_destroy(LocalStringInterner *interner);

StringId local_string_intern_str8(LocalStringInterner *interner, str8 str);
StringId local_string_lookup_str8(LocalStringInterner *interner, str8 str);

StringId local_string_intern_cstr(LocalStringInterner *interner, const char *str);
StringId local_string_lookup_cstr(LocalStringInterner *interner, const char *str);

StringId local_string_intern_token(LocalStringInterner *interner, File *file, Token token);
StringId local_string_lookup_token(LocalStringInterner *interner, File *file, Token token);

str8 local_string_get_str8_from_id(LocalStringInterner *interner, StringId id);

u8 local_string_get_interner_id(StringId id);
StringId local_string_get_id(StringId id);
StringId local_string_create_id(u8 interner_id, StringId string_id);
bool local_string_are_strings_equal(StringId a, StringId b);

#endif // !LILY_STRING_INTERNER_LOCAL_INTERNER_H
