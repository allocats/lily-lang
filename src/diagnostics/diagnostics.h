#ifndef LILY_DIAGNOSTICS_H
#define LILY_DIAGNOSTICS_H

#include "diagnostics/types.h"
#include "utils/types.h"

#define DIAGNOSTIC_ALLOC_MSG(engine)                                            \
    (str8) {                                                                    \
        .ptr = arena_alloc(&engine -> arena, engine -> diagnostic_max_length),  \
        .len = engine -> diagnostic_max_length                                  \
    }

/*
 *
 * creation and deletion
 *
 */
void diagnostic_engine_init(DiagnosticEngine* engine);
void diagnostic_engine_destroy(DiagnosticEngine* engine);


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
bool diagnostics_print(DiagnosticEngine* engine);

#endif // !LILY_DIAGNOSTICS_H
