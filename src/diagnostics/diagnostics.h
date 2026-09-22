#ifndef LILY_DIAGNOSTICS_H
#define LILY_DIAGNOSTICS_H

#include "diagnostics/types.h"
#include "ids.h"
#include "utils/types.h"

/*
 *
 * creation and deletion
 *
 */
void diagnostic_engine_init(void);
void diagnostic_engine_destroy(void);


/*
 *
 * Generic, doesn't have a source location, used for things like flags, allocations, etc.
 *
 */
void diagnostic_add_generic(DiagKind kind, char* fmt, ...);

/*
 *
 * Token related
 *
 */
void diagnostic_add_token(File* file, DiagKind kind, Token* tok, u8 loc, const char* msg, const char* help);
void diagnostic_add_token_span(File* file, DiagKind kind, SpanU32 span, const char* msg, const char* help);

/*
 *
 * Returns true if it has errors
 *
 */
bool diagnostics_print();

#endif // !LILY_DIAGNOSTICS_H
