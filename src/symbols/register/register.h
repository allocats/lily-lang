#ifndef LILY_SYMBOLS_REGISTER_H
#define LILY_SYMBOLS_REGISTER_H

#include "files/types.h"
#include "symbols/register/types.h"
#include "workspace/types.h"

void symbols_register_top_level(Workspace *ws, File *file);

ScopeId scope_enter(Registrar *r);
ScopeId scope_exit(Registrar *r);

#endif // !LILY_SYMBOLS_REGISTER_H
