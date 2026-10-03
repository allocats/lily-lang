#ifndef LILY_STRING_INTERNER_H
#define LILY_STRING_INTERNER_H

#include "files/types.h"
#include "string_interner/types.h"

static constexpr u32 LOCAL_STRING_ID_BIT_OFFSET = 24u;

void string_interner_init(StringInterner* interner, u8 id);
void string_interner_destroy(StringInterner* interner);

StringId string_intern_str8(StringInterner* interner, str8 str);
StringId string_lookup_str8(StringInterner* interner, str8 str);

StringId string_intern_cstr(StringInterner* interner, const char* str);
StringId string_lookup_cstr(StringInterner* interner, const char* str);

StringId string_intern_token(StringInterner* interner, File* file, Token token);
StringId string_lookup_token(StringInterner* interner, File* file, Token token);

str8 string_get_str8_from_id(StringInterner* interner, StringId id);

u8       get_string_interner_id(StringId id);
StringId get_local_string_id(StringId id);
StringId create_string_id(u8 interner_id, StringId string_id);
bool     are_strings_equal(StringId a, StringId b);

#endif // !LILY_STRING_INTERNER_H
