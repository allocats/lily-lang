#ifndef LILY_LIBRARIES_TYPES_H
#define LILY_LIBRARIES_TYPES_H

#include "ids.h"

typedef enum {
    LIBRARY_SYSTEM,
    LIBRARY_LOCAL,
} LibraryKind;

typedef struct {
    LibraryKind kind;

    union {
        struct {
            StringId name_id;
        } system_library;

        struct {
            StringId path_id;
            StringId name_id;
        } local_library;
    } as;
} ForeignLibrary;

typedef struct {
    ForeignLibrary *items;
    u32 count;
    u32 capacity;
} ForeignLibraries;

#endif // !LILY_LIBRARIES_TYPES_H
