#include "libraries/libraries.h"
#include "ids.h"
#include "libraries/types.h"
#include "utils/wrappers.h"

static constexpr u32 init_capacity = 16;

void foreign_library_init(ForeignLibraries *libraries) {
    libraries->items    = lily_alloc(sizeof(ForeignLibrary) * init_capacity);
    libraries->count    = 0;
    libraries->capacity = init_capacity;
}

void foreign_library_destroy(ForeignLibraries *libraries) {
    if (libraries->capacity == 0) {
        return;
    }

    free(libraries->items);
}

void foreign_library_add(ForeignLibraries *libraries, StringId path, StringId name) {
    if (libraries->count >= libraries->capacity) {
        u64 old_size = sizeof(ForeignLibrary) * libraries->capacity;
        u64 new_size = old_size * 2;

        libraries->items = lily_realloc(libraries->items, new_size);
        libraries->capacity *= 2;
    }

    ForeignLibrary lib = {0};

    if (path == GLOBAL_STRING_ID_NONE) {
        lib.kind                      = LIBRARY_SYSTEM;
        lib.as.system_library.name_id = name;
    } else {
        lib.kind                     = LIBRARY_LOCAL;
        lib.as.local_library.path_id = path;
        lib.as.local_library.name_id = name;
    }

    libraries->items[libraries->count++] = lib;
}
