#include "diagnostics/diagnostics.h"
#include "driver/types.h"
#include "symbols/resolve/types.h"
#include "types/entries/entries.h"
#include "utils/macros.h"
#include <stdarg.h>
#include <stdio.h>

extern Driver driver;

static str8 type_to_diagnostic_str(Resolver *r, DiagnosticEngine *engine, TypeId type_id);

void resolver_error_symbol_redefined(Resolver *r, AstNode *node, SymbolId symbol_id, StringId name) {
    DiagnosticEngine *engine = &driver.global_diagnostic_engine;

    Symbol *symbol = &r->symbol_table->symbols[symbol_id];

    str8 name_str = r->ws->global_string_interner.entries[name].str;

    str8 msg  = DIAGNOSTIC_ALLOC_MSG(engine);
    str8 help = DIAGNOSTIC_ALLOC_MSG(engine);

    snprintf(msg.ptr, msg.len, "'%.*s' is redefined", STR8_FMT(name_str));
    snprintf(help.ptr, help.len, "'%.*s' is redefined here", STR8_FMT(name_str));

    if (symbol->ast_node_id == AST_NODE_ID_NONE) {
        diagnostic_add_token_span(
            engine,
            r->file,
            DIAG_ERROR,
            node->tokens,
            msg.ptr,
            help.ptr
        );
    } else {
        str8 original_msg  = DIAGNOSTIC_ALLOC_MSG(engine);
        str8 original_help = DIAGNOSTIC_ALLOC_MSG(engine);

        snprintf(original_msg.ptr, original_msg.len, "'%.*s' is defined here", STR8_FMT(name_str));
        snprintf(original_help.ptr, original_help.len, "'%.*s' is defined here", STR8_FMT(name_str));

        File *file            = r->ws->file_interner.entries[symbol->file_id];
        AstNode *defined_node = &file->ast.nodes[symbol->ast_node_id];

        diagnostic_add_token_span(
            engine,
            file,
            DIAG_NOTE,
            defined_node->tokens,
            original_msg.ptr,
            original_help.ptr
        );

        diagnostic_add_token_span(
            engine,
            r->file,
            DIAG_ERROR,
            node->tokens,
            msg.ptr,
            help.ptr
        );
    }
}

void resolver_undeclared_identifier(Resolver *r, AstNode *node, StringId name_id) {
    DiagnosticEngine *engine = &driver.global_diagnostic_engine;

    str8 msg = DIAGNOSTIC_ALLOC_MSG(engine);

    str8 name = r->interner->entries[name_id].str;

    snprintf(msg.ptr, msg.len, "undeclared identifier: '%.*s'", STR8_FMT(name));

    diagnostic_add_token_span(
        engine,
        r->file,
        DIAG_ERROR,
        node->tokens,
        msg.ptr,
        null
    );
}

void resolver_cannot_reference_rvalue(Resolver *r, AstNode *node) {
    DiagnosticEngine *engine = &driver.global_diagnostic_engine;

    diagnostic_add_token_span(
        engine,
        r->file,
        DIAG_ERROR,
        node->tokens,
        "cannot take the reference of an rvalue",
        "is not an lvalue"
    );
}

void resolver_cannot_dereference_rvalue(Resolver *r, AstNode *node) {
    DiagnosticEngine *engine = &driver.global_diagnostic_engine;

    diagnostic_add_token_span(
        engine,
        r->file,
        DIAG_ERROR,
        node->tokens,
        "cannot dereference an rvalue",
        "is not an lvalue"
    );
}

void resolver_cannot_derefence_non_pointer(Resolver *r, AstNode *node, TypeId type_id) {
    DiagnosticEngine *engine = &driver.global_diagnostic_engine;

    str8 type = type_to_diagnostic_str(r, engine, type_id);

    str8 msg = DIAGNOSTIC_ALLOC_MSG(engine);

    snprintf(msg.ptr, msg.len, "cannot deference non-pointer type '%.*s'", STR8_FMT(type));

    diagnostic_add_token_span(
        engine,
        r->file,
        DIAG_ERROR,
        node->tokens,
        msg.ptr,
        null
    );
}

void resolver_cannot_apply_unary_op(Resolver *r, AstNode *node, char op, TypeId type_id) {
    DiagnosticEngine *engine = &driver.global_diagnostic_engine;

    str8 type = type_to_diagnostic_str(r, engine, type_id);

    str8 msg = DIAGNOSTIC_ALLOC_MSG(engine);

    snprintf(msg.ptr, msg.len, "cannot apply unary operator '%c' to type '%.*s'", op, STR8_FMT(type));

    diagnostic_add_token_span(
        engine,
        r->file,
        DIAG_ERROR,
        node->tokens,
        msg.ptr,
        null
    );
}

/*
 *
 * Type diagnostics
 *
 */

typedef struct {
    char *buffer;
    usize capacity;
    usize count;
} TypeWriter;

static void type_writer_printf(TypeWriter *tw, const char *fmt, ...) {
    if (tw->capacity == 0) {
        return;
    }

    va_list ap;

    va_start(ap, fmt);

    i32 n = vsnprintf(tw->buffer + tw->count, tw->capacity - tw->count, fmt, ap);

    va_end(ap);

    if (n < 0) {
        return;
    }

    tw->count += n;

    if (tw->count > tw->capacity - 1) {
        tw->count = tw->capacity - 1;
    }
}

static void type_write(Resolver *r, TypeWriter *tw, TypeId type_id) {
    Type *type = &r->type_table->entries[type_id];

    switch (type->kind) {
    case TYPE_BASE:
    case TYPE_STRUCT:
    case TYPE_UNION:
    case TYPE_ENUM: {
        Symbol *symbol = &r->symbol_table->symbols[type->symbol_id];
        str8 name      = r->interner->entries[symbol->name_id].str;

        type_writer_printf(tw, "%.*s", STR8_FMT(name));

        break;
    }

    case TYPE_POINTER: {
        type_writer_printf(tw, "*");
        type_write(r, tw, type->as.pointer_type.pointee_id);
        break;
    }

    case TYPE_ARRAY: {
        type_writer_printf(tw, "[%llu]", (unsigned long long)type->as.array_type.size);
        type_write(r, tw, type->as.array_type.element_id);
        break;
    }

    case TYPE_FUNCTION: {
        TypeFunction *fn = &type->as.function_type;

        type_writer_printf(tw, "fn(");

        for (u32 i = 0; i < fn->argument_count; i++) {
            if (i > 0) {
                type_writer_printf(tw, ", ");
            }

            type_write(r, tw, fn->arguments[i]);
        }

        type_writer_printf(tw, ") -> ");

        type_write(r, tw, fn->return_type);

        break;
    }

    case TYPE_ERROR: {
        type_writer_printf(tw, "<error>");
        break;
    }

    default: {
        type_writer_printf(tw, "<unknown>");
        break;
    }
    }
}

static str8 type_to_diagnostic_str(Resolver *r, DiagnosticEngine *engine, TypeId type_id) {
    str8 out = DIAGNOSTIC_ALLOC_MSG(engine);

    TypeWriter tw = {
        .buffer   = out.ptr,
        .capacity = out.len,
        .count    = 0,
    };

    if (tw.capacity > 0) {
        tw.buffer[0] = '\0';
    }

    type_write(r, &tw, type_id);

    return out;
}

void resolver_mismatched_types(Resolver *r, AstNode *node, TypeId expected, TypeId found) {
    DiagnosticEngine *engine = &driver.global_diagnostic_engine;

    str8 expected_str = type_to_diagnostic_str(r, engine, expected);
    str8 found_str    = type_to_diagnostic_str(r, engine, found);

    str8 msg = DIAGNOSTIC_ALLOC_MSG(engine);
    snprintf(msg.ptr, msg.len, "expected '%s', but found '%s'", expected_str.ptr, found_str.ptr);

    char *help_ptr = null;

    if (type_can_cast_to(r->type_table, &r->builtin_types, found, expected)) {
        str8 help = DIAGNOSTIC_ALLOC_MSG(engine);

        snprintf(help.ptr, help.len, "try casting to '%s', cast(%s)(...)", expected_str.ptr, expected_str.ptr);

        help_ptr = help.ptr;
    }

    diagnostic_add_token_span(
        engine,
        r->file,
        DIAG_ERROR,
        node->tokens,
        msg.ptr,
        help_ptr
    );
}
