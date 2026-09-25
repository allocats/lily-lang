#ifndef LILY_STRING_INTERNER_H
#define LILY_STRING_INTERNER_H

#include "files/types.h"
#include "string_interner/types.h"

void string_interner_init(StringInterner* interner);
void string_interner_destroy(StringInterner* interner);

StringId string_intern_str8(StringInterner* interner, str8 str);
StringId string_lookup_str8(StringInterner* interner, str8 str);

StringId string_intern_cstr(StringInterner* interner, const char* str);
StringId string_lookup_cstr(StringInterner* interner, const char* str);

StringId string_intern_token(StringInterner* interner, File* file, Token token);
StringId string_lookup_token(StringInterner* interner, File* file, Token token);

str8 string_get_str8_from_id(StringInterner* interner, StringId id);

#endif // !LILY_STRING_INTERNER_H
