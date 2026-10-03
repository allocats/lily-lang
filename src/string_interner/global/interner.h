#ifndef LILY_STRING_INTERNER_GLOBAL_H
#define LILY_STRING_INTERNER_GLOBAL_H

#include "string_interner/global/types.h"

void global_string_interner_init(GlobalStringInterner *interner);
void global_string_interner_destroy(GlobalStringInterner *interner);

StringId global_string_intern_str8(GlobalStringInterner *interner, str8 str);
StringId global_string_lookup_str8(GlobalStringInterner *interner, str8 str);

StringId global_string_intern_cstr(GlobalStringInterner *interner, char *cstr);
StringId global_string_lookup_cstr(GlobalStringInterner *interner, char *cstr);

StringId global_string_from_local_id(StringId local_id);

#endif // !LILY_STRING_INTERNER_GLOBAL_H
