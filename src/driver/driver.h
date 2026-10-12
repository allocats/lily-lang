#ifndef LILY_DRIVER_H
#define LILY_DRIVER_H

#include "driver/types.h"

void driver_init(Driver *driver, const char *home);
void driver_destroy(Driver *driver);

bool driver_parse_flags(Driver *driver, i32 argc, char **argv);

#endif // !LILY_DRIVER_H
