#ifndef LILY_PARSER_RECOVERY_H
#define LILY_PARSER_RECOVERY_H

#include "parser/recovery/types.h"
#include "parser/types.h"

void parser_recover(Parser* p, RecoveryKind kind);
AstNodeId parser_error(Parser* p, AstNodeId id, RecoveryKind kind);

#endif // !LILY_PARSER_RECOVERY_H
