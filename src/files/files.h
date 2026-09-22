#ifndef LILY_FILES_H
#define LILY_FILES_H

#include "files/types.h"

void file_interner_init(FileInterner* interner);
void file_interner_destroy(FileInterner* interner);

FileId file_intern(FileInterner* interner, str8 path);
FileId file_lookup(FileInterner* interner, str8 path);

#endif // !LILY_FILES_H
