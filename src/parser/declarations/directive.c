#include "ast/kinds.h"
#include "diagnostics/diagnostics.h"
#include "diagnostics/types.h"
#include "ids.h"
#include "parser/declarations/declarations.h"
#include "parser/directives/directives.h"
#include "parser/parser.h"
#include "parser/recovery/recovery.h"
#include "parser/recovery/types.h"
#include "threadpool/types.h"

#include <threads.h>

extern thread_local ThreadContext thread_context;

AstNodeId parse_directive_declaration(Parser *p, AstNodeId id, StringId name) {
    AstNodeId directive_id  = parse_directive(p);
    AstNode* directive_node = parser_get_node(p, directive_id); 

    if (directive_node -> kind == AST_ERROR) {
        return parser_error(p, id, RECOVERY_NONE);
    }

    if (directive_node -> kind != AST_IMPORT_DIRECTIVE) {
        diagnostic_add_token_span(
            thread_context.diagnostic_engine,
            p -> file,
            DIAG_ERROR,
            directive_node -> tokens,
            "only import directives can be binded",
            "expected `#import` for binded directives"
        );

        return parser_error(p, id, RECOVERY_NONE);
    }

    AstNode* node = parser_get_node(p, id);

    node -> kind = AST_IMPORT_DECL;

    node -> as.import_decl.name = name;
    node -> as.import_decl.import = directive_id;

    node -> tokens.end = p -> cursor - 1;

    p -> file -> ast.total_declaration_count += 1;

    return id;
}
