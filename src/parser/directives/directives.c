#include "ast/kinds.h"
#include "ast/types.h"
#include "diagnostics/diagnostics.h"
#include "diagnostics/types.h"
#include "driver/types.h"
#include "files/files.h"
#include "ids.h"
#include "parser/directives/directives.h"
#include "parser/parser.h"
#include "parser/recovery/recovery.h"
#include "parser/recovery/types.h"
#include "string_interner/interner.h"
#include "threadpool/threadpool.h"
#include "tokens/kinds.h"
#include "utils/debug.h"
#include "utils/macros.h"
#include "workspace/types.h"
#include "workspace/workspace.h"

#include <assert.h>
#include <linux/limits.h>
#include <stdio.h>
#include <string.h>
#include <threads.h>

extern Driver driver;

static thread_local char relative_scratch[PATH_MAX] = {0}; 

/*
 *
 * this MUST be updated as new directives are added
 *
 */
static constexpr u32 directive_count = 3;
static AstKind directive_lut[directive_count] = {0};

/*
 *
 * helper functions
 *
 */

static AstNodeId parse_import_directive(Parser* p, AstNodeId id);
static AstNodeId parse_include_directive(Parser* p, AstNodeId id);
static AstNodeId parse_execute_directive(Parser* p, AstNodeId id);

/*
 *
 * this function does assert must be called by `workspace_init()`
 *
 */

void init_directive_lut(Workspace* ws) {
    assert(string_intern_cstr(&ws -> string_interner, "import") == 0);

    directive_lut[string_intern_cstr(&ws -> string_interner, "import")]  = AST_IMPORT_DIRECTIVE;
    directive_lut[string_intern_cstr(&ws -> string_interner, "include")] = AST_INCLUDE_DIRECTIVE;
    directive_lut[string_intern_cstr(&ws -> string_interner, "execute")] = AST_EXECUTE_DIRECTIVE;

    debug_log("Directive lookup table has been set up");
}

/*
 *
 * actual directive parsing, dispatches to other directive parsing helpers 
 *
 */

AstNodeId parse_directive(Parser* p) {
    AstNodeId id = parser_create_node(p);

    parser_advance(p); // advance past '#'

    Token directive_token = parser_advance(p); // advance past `IDENTIFIER`

    if (directive_token.kind != TOK_IDENTIFIER) {
        diagnostic_add_token(
            p -> file,
            DIAG_ERROR,
            &directive_token,
            DIAG_LOC_WHOLE_TOK,
            "expected identifier for directive name",
            "add a valid identifier here"
        );

        return parser_error(p, id, RECOVERY_NONE);
    }

    StringId string_id = string_lookup_token(&p -> workspace -> string_interner, p -> file, directive_token);

    if (string_id > directive_count) {
        diagnostic_add_token(
            p -> file,
            DIAG_ERROR,
            &directive_token,
            DIAG_LOC_WHOLE_TOK,
            "unknown directive",
            "add a valid directive here"
        );

        return parser_error(p, id, RECOVERY_NONE);
    }

    AstKind directive_kind = directive_lut[string_id];

    AstNode* node = parser_get_node(p, id);

    node -> kind = directive_kind;

    switch (directive_kind) {
        case AST_IMPORT_DIRECTIVE:
            return parse_import_directive(p, id);

        case AST_INCLUDE_DIRECTIVE:
            return parse_include_directive(p, id);

        case AST_EXECUTE_DIRECTIVE:
            return parse_execute_directive(p, id);

        default:
            UNREACHABLE("Found %s", AST_KIND_STRINGS[directive_kind]);
    }
}

static AstNodeId parse_import_directive(Parser* p, AstNodeId id) {
    Token module_tok = parser_advance(p); // advance past expected string literal 

    if (module_tok.kind != TOK_STRING_LIT) {
        diagnostic_add_token(
            p -> file,
            DIAG_ERROR,
            &module_tok,
            DIAG_LOC_WHOLE_TOK,
            "expected string literal",
            "imports take in a string literal, i.e #import \"Module\""
        );

        return parser_error(p, id, RECOVERY_NONE);
    }

    if (!parser_check(p, TOK_SEMI)) {
        diagnostic_add_token(
            p -> file,
            DIAG_ERROR,
            &module_tok,
            DIAG_LOC_END_OF_TOK,
            "expected ';'",
            "add a ';' here"
        );

        return parser_error(p, id, RECOVERY_NONE);
    }

    if (module_tok.length == 2) {
        diagnostic_add_token(
            p -> file,
            DIAG_LOC_WHOLE_TOK,
            &module_tok,
            DIAG_LOC_WHOLE_TOK,
            "no module provided for import",
            "add a module here i.e. \"Module\""
        );

        return parser_error(p, id, RECOVERY_NONE);
    }

    module_tok.start  += 1;
    module_tok.length -= 2;

    StringId module_string_id = string_intern_token(&p -> workspace -> string_interner, p -> file, module_tok);

    module_tok.start  -= 1;
    module_tok.length += 2;

    AstNode* node = parser_get_node(p, id);

    node -> as.import_directive.module  = module_string_id;
    node -> as.import_directive.file_id = FILE_ID_NONE;

    /*
     *
     * load the module, first try standard library, if that fails then we try relative
     *
     */

    FileId module_file_id = FILE_ID_NONE;

    /*
     *
     *  check to see if the module is a standard library module
     *
     */
    str8 stdlib_path_maybe = standard_library_map_get_module(
        &p -> workspace -> standard_library_map,
        module_string_id
    );

    if (stdlib_path_maybe.ptr != null) {
        module_file_id = file_intern(&p -> workspace -> file_interner, stdlib_path_maybe);
    } else {
        str8 module_name_str = string_get_str8_from_id(&p -> workspace -> string_interner, module_string_id);

        /*
         *
         * bulid path for a relative module
         *
         */
        i32 bytes_written_relative = snprintf(
            relative_scratch,
            sizeof(relative_scratch),
            "%.*s/module.lily",
            module_name_str.len,
            module_name_str.ptr
        );

        str8 relative_path_to_module = {
            .ptr = relative_scratch,
            .len = bytes_written_relative
        };

        module_file_id = file_intern(&p -> workspace -> file_interner, relative_path_to_module);
    }

    if (module_file_id == FILE_ID_NONE) {
        diagnostic_add_token(
            p -> file,
            DIAG_ERROR,
            &module_tok,
            DIAG_LOC_WHOLE_TOK,
            "import does not exist",
            "cannot find this import's module.lily"
        );

        return parser_error(p, id, RECOVERY_NONE);
    }

    node -> as.import_directive.file_id = module_file_id;
    node -> tokens.end = p -> cursor;

    parser_advance(p); // advance past ';'

    File* file = workspace_get_file(p -> workspace, module_file_id);

    threadpool_add_task(&driver.thread_pool, workspace_dispatch_lex_and_parse_file, file);

    return id;
}

static AstNodeId parse_include_directive(Parser* p, AstNodeId id) {
    Token file_tok = parser_advance(p); // advance past expected string literal 

    if (file_tok.kind != TOK_STRING_LIT) {
        diagnostic_add_token(
            p -> file,
            DIAG_ERROR,
            &file_tok,
            DIAG_LOC_WHOLE_TOK,
            "expected string literal",
            "includes take in a string literal, i.e #include \"path/to/file.lily\""
        );

        return parser_error(p, id, RECOVERY_NONE);
    }

    if (!parser_check(p, TOK_SEMI)) {
        diagnostic_add_token(
            p -> file,
            DIAG_ERROR,
            &file_tok,
            DIAG_LOC_END_OF_TOK,
            "expected ';'",
            "add a ';' here"
        );

        return parser_error(p, id, RECOVERY_NONE);
    }

    if (file_tok.length == 2) {
        diagnostic_add_token(
            p -> file,
            DIAG_LOC_WHOLE_TOK,
            &file_tok,
            DIAG_LOC_WHOLE_TOK,
            "no file path provided for include",
            "add a file path here i.e. \"file.lily\""
        );

        return parser_error(p, id, RECOVERY_NONE);
    }

    file_tok.start  += 1;
    file_tok.length -= 2;

    StringId file_string_id = string_intern_token(&p -> workspace -> string_interner, p -> file, file_tok);

    file_tok.start  -= 1;
    file_tok.length += 2;

    AstNode* node = parser_get_node(p, id);

    node -> as.include_directive.path = file_string_id;
    node -> as.include_directive.file_id = FILE_ID_NONE;

    str8 file_path_str = string_get_str8_from_id(&p -> workspace -> string_interner, file_string_id);

    str8 current_path = p -> file -> path;

    char* last_slash = strrchr(current_path.ptr, '/');

    i32 bytes_written = 0;

    if (!last_slash) {
        bytes_written = snprintf(relative_scratch, sizeof(relative_scratch), "%.*s", STR8_FMT(file_path_str));
    } else {
        bytes_written = snprintf(
            relative_scratch,
            sizeof(relative_scratch),
            "%.*s/%.*s",
            (i32)(last_slash - current_path.ptr),
            current_path.ptr,
            STR8_FMT(file_path_str)
        );
    }

    str8 final_path = {
        .ptr = relative_scratch,
        .len = bytes_written
    };

    /*
     *
     * load the file relative to the current file
     *
     */

    FileId file_id = file_intern(&p -> workspace -> file_interner, final_path);

    if (file_id == FILE_ID_NONE) {
        diagnostic_add_token(
            p -> file,
            DIAG_ERROR,
            &file_tok,
            DIAG_LOC_WHOLE_TOK,
            "file does not exist",
            "cannot find this file"
        );

        return parser_error(p, id, RECOVERY_NONE);
    }

    node -> as.include_directive.file_id = file_id;
    node -> tokens.end = p -> cursor;

    parser_advance(p); // advance past ';'

    File* file = workspace_get_file(p -> workspace, file_id);

    threadpool_add_task(&driver.thread_pool, workspace_dispatch_lex_and_parse_file, file);

    return id;
}

static AstNodeId parse_execute_directive(Parser* p, AstNodeId id) {
    AstNodeId expr_id = AST_NODE_ID_NONE; //= parse_expression(p);

    AstNode* node = parser_get_node(p, id);

    node -> as.exec_directive.expr = expr_id;

    return id;
}
