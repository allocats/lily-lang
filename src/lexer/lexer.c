#include "diagnostics/diagnostics.h"
#include "files/types.h"
#include "lexer/lexer.h"
#include "lexer/types.h"
#include "tokens/types.h"
#include "utils/debug.h"
#include "utils/macros.h"
#include "utils/types.h"

#include <assert.h>
#include <string.h>
#include <threads.h>

#define IS_DIGIT(c)         (CHAR_MAP[(unsigned char)(c)] & 1)
#define IS_ALPHA(c)         (CHAR_MAP[(unsigned char)(c)] & 2)
#define IS_OPERATOR(c)      (CHAR_MAP[(unsigned char)(c)] & 4)
#define IS_DELIMITER(c)     (CHAR_MAP[(unsigned char)(c)] & 8)
#define IS_WHITESPACE(c)    (CHAR_MAP[(unsigned char)(c)] & 16)
#define IS_CHAR_DELIM(c)    (CHAR_MAP[(unsigned char)(c)] & 32)
#define IS_STRING_DELIM(c)  (CHAR_MAP[(unsigned char)(c)] & 64)
#define IS_ALPHA_NUMERIC(c) (CHAR_MAP[(unsigned char)(c)] & 3)

static void delimiter_match(Lexer* lexer, Token* token);
static void delimiter_stack_push(Lexer* lexer, u32 index);

static void lex_whitespace(Lexer* lexer);
static void lex_word(Lexer* lexer);
static void lex_number(Lexer* lexer);
static void lex_operator(Lexer* lexer);
static void lex_delimiter(Lexer* lexer);
static void lex_char_lit(Lexer* lexer);
static void lex_string_lit(Lexer* lexer);
static void lex_invalid(Lexer* lexer);

static void lexer_advance(Lexer* lexer);
static void lexer_advance_by(Lexer* lexer, u32 n);
static char lexer_current(Lexer* lexer);

static void set_source_location(Lexer* lexer);
static Token* tokens_get_new_token(Lexer* lexer);

typedef void (*LexFn)(Lexer*);

static const thread_local LexFn LEXER_DISPATCH[] = {
    ['0' ... '9'] = lex_number,

    ['a' ... 'z'] = lex_word,
    ['A' ... 'Z'] = lex_word,
    ['_']         = lex_word,
    
    ['@']         = lex_operator,
    ['#']         = lex_operator,
    ['$']         = lex_operator,
    ['?']         = lex_operator,
    ['&']         = lex_operator,
    ['|']         = lex_operator,
    ['~']         = lex_operator,
    ['^']         = lex_operator,
    ['-']         = lex_operator,
    ['+']         = lex_operator,
    ['/']         = lex_operator,
    ['*']         = lex_operator,
    ['%']         = lex_operator,
    ['=']         = lex_operator,
    ['!']         = lex_operator,
    ['<']         = lex_operator,
    ['>']         = lex_operator,
    ['.']         = lex_operator,

    [',']         = lex_delimiter,
    ['[']         = lex_delimiter,
    [']']         = lex_delimiter,
    ['(']         = lex_delimiter,
    [')']         = lex_delimiter,
    ['{']         = lex_delimiter,
    ['}']         = lex_delimiter,
    [';']         = lex_delimiter,
    [':']         = lex_delimiter,
    ['\0']        = lex_delimiter,

    [' ']         = lex_whitespace, 
    ['\t']        = lex_whitespace, 
    ['\n']        = lex_whitespace,
    ['\f']        = lex_whitespace,
    ['\r']        = lex_whitespace,

    ['\'']        = lex_char_lit,
    ['\"']        = lex_string_lit,
};

void lex_file(File* file) {
    assert(file -> stage == FILE_ALLOCATED);

    debug_log("Lexing file %u", file -> id);

    str8 buffer = file -> buffer;

    const char* buffer_start = buffer.ptr;
    const char* buffer_end   = buffer_start + buffer.len;

    Lexer lexer = {
        .file = file,
        .stack = {0},
        .cursor = buffer_start,
        .end = buffer_end,
        .line = 1,
        .col = 1
    };

    while (lexer.cursor < lexer.end) {
        LexFn fn = LEXER_DISPATCH[(unsigned char) *lexer.cursor];
        fn ? fn(&lexer) : lex_invalid(&lexer);
    }

    if (file -> stage != FILE_ERROR) {
        file -> stage = FILE_LEXED;
    }

    if (lexer.stack.top != 0) {
        u32 index = lexer.stack.items[lexer.stack.top - 1];

        Token* delim_tok = &file -> tokens.items[index];

        diagnostic_add_token(
            file,
            DIAG_ERROR,
            delim_tok,
            DIAG_LOC_WHOLE_TOK,
            "unclosed delimiter",
            "add closing delimiter"
        );

        file -> stage = FILE_ERROR;
    } 

    if (file -> tokens.count == 0) { 
        file -> stage = FILE_ERROR;
    } 

    file -> lines_of_code = lexer.line;
}

static void lex_whitespace(Lexer* lexer) {
    while (IS_WHITESPACE(lexer_current(lexer))) {
        // TODO: perhaps profile this, might be a nothing burger
        if (lexer_current(lexer) == '\n') {
            lexer -> line += 1;
            lexer -> col = 0; // set this to zero because advance will make it 1
        } 

        lexer_advance(lexer);
    }
}

static void lex_word(Lexer* lexer) {
    Token* token = tokens_get_new_token(lexer);

    const char* start = lexer -> cursor;

    while (IS_ALPHA_NUMERIC(lexer_current(lexer))) {
        lexer_advance(lexer);
    }

    u32 length = lexer -> cursor - start;

    assert(length < U16_MAX);

    token -> start  = start - lexer -> file -> buffer.ptr;
    token -> length = length;

    switch (length) {
        case 2: {
            if (start[0] == 'f' && start[1] == 'n') { token -> kind = TOK_KW_FN; break; }
            if (start[0] == 'i' && start[1] == 'f') { token -> kind = TOK_KW_IF; break; }
            token -> kind = TOK_IDENTIFIER;
        } break;

        case 3: {
            switch (start[0]) {
                case 'f': 
                    token -> kind = (start[1]=='o' && start[2]=='r') ? TOK_KW_FOR : TOK_IDENTIFIER; 
                    break;

                default:  
                    token -> kind = TOK_IDENTIFIER; 
            }
        } break;

        case 4: {
            switch (start[0]) {
                case 'e':
                    if (start[1] == 'l' && start[2] == 's' && start[3] == 'e') { token -> kind = TOK_KW_ELSE; break; }
                    if (start[1] == 'n' && start[2] == 'u' && start[3] == 'm') { token -> kind = TOK_KW_ENUM; break; }
                    token -> kind = TOK_IDENTIFIER;
                    break;

                case 'n': 
                    if (start[1] == 'u' && start[2] == 'l' && start[3] == 'l') { token -> kind = TOK_KW_NULL; break; }
                    token -> kind = TOK_IDENTIFIER;
                    break;

                case 't':
                    if (start[1] == 'r' && start[2] == 'u' && start[3] == 'e') { token -> kind = TOK_KW_TRUE; break; }
                    token -> kind = TOK_IDENTIFIER;
                    break;

                default:  
                    token -> kind = TOK_IDENTIFIER;
            }
        } break;

        case 5: {
            switch (start[0]) {
                case 'b': token -> kind = (memcmp(start, "break", 5) == 0) ? TOK_KW_BREAK : TOK_IDENTIFIER; break;
                case 'c': token -> kind = (memcmp(start, "const", 5) == 0) ? TOK_KW_CONST : TOK_IDENTIFIER; break;
                case 'd': token -> kind = (memcmp(start, "defer", 5) == 0) ? TOK_KW_DEFER : TOK_IDENTIFIER; break;
                case 'f': token -> kind = (memcmp(start, "false", 5) == 0) ? TOK_KW_FALSE : TOK_IDENTIFIER; break;
                case 'u': token -> kind = (memcmp(start, "union", 5) == 0) ? TOK_KW_UNION : TOK_IDENTIFIER; break;
                case 'w': token -> kind = (memcmp(start, "while", 5) == 0) ? TOK_KW_WHILE : TOK_IDENTIFIER; break;
                default:  token -> kind = TOK_IDENTIFIER;
            }
        } break;

        case 6: {
            switch (start[0]) {
                case 'i': token -> kind = (memcmp(start, "inline", 6) == 0) ? TOK_KW_INLINE : TOK_IDENTIFIER; break;
                case 'r': token -> kind = (memcmp(start, "return", 6) == 0) ? TOK_KW_RETURN : TOK_IDENTIFIER; break;
                case 's': 
                    if (memcmp(start, "struct", 6) == 0) { token -> kind = TOK_KW_STRUCT; break; }
                    token -> kind = TOK_IDENTIFIER; 
                    break;
                default:  token -> kind = TOK_IDENTIFIER;
            }
        } break;

        case 7: {
            switch (start[0]) {
                case 'f': token -> kind = (memcmp(start, "foreign", 7) == 0) ? TOK_KW_FOREIGN : TOK_IDENTIFIER; break;
                default:  token -> kind = TOK_IDENTIFIER;
            }
        } break;

        case 8: {
            switch (start[0]) {
                case 'c': token -> kind = (memcmp(start, "continue", 8) == 0) ? TOK_KW_CONTINUE : TOK_IDENTIFIER; break;
                case 'n': token -> kind = (memcmp(start, "noinline", 8) == 0) ? TOK_KW_NOINLINE : TOK_IDENTIFIER; break;
                default:  token -> kind = TOK_IDENTIFIER;
            }
        } break;

        case 9: {
            switch (start[0]) {
                case 'i': token -> kind = (memcmp(start, "intrinsic", 9) == 0) ? TOK_KW_INTRINSIC : TOK_IDENTIFIER; break;
                default:  token -> kind = TOK_IDENTIFIER;
            }
        } break;

        default: {
            token -> kind = TOK_IDENTIFIER;
        } break;
    }
}

static void lex_number(Lexer* lexer) {
    Token* token = tokens_get_new_token(lexer);

    bool is_floating_point = false;

    const char* start = lexer -> cursor;

    while (IS_DIGIT(lexer_current(lexer))) {
        lexer_advance(lexer);
    }

    if (lexer_current(lexer) == '.') {
        is_floating_point = true;

        lexer_advance(lexer);

        while (IS_DIGIT(lexer_current(lexer))) {
            lexer_advance(lexer);
        }
    }

    token -> kind = is_floating_point ? TOK_FLOAT_LIT : TOK_INTEGER_LIT;
    token -> start  = start - lexer -> file -> buffer.ptr;
    token -> length = lexer -> cursor - start;
}

static void lex_operator(Lexer* lexer) {
    Token* token = tokens_get_new_token(lexer);

    const char* start = lexer -> cursor;

    lexer_advance(lexer);

    switch (*start) {
        case '@': {
            token -> kind = TOK_AT;
        } break;

        case '#': {
            token -> kind = TOK_HASHTAG;
        } break;

        case '$': {
            token -> kind = TOK_DOLLAR;
        } break;

        case '?': {
            token -> kind = TOK_QUESTION;
        } break;

        case '=': {
            if (lexer_current(lexer) == '=') {
                token -> kind = TOK_EQ_EQ;
                lexer_advance(lexer);
                break;
            }

            token -> kind = TOK_EQ;
        } break;

        case '!': {
            if (lexer_current(lexer) == '=') {
                token -> kind = TOK_BANG_EQ;
                lexer_advance(lexer);
                break;
            }

            token -> kind = TOK_BANG;
        } break;

        case '+': {
            if (lexer_current(lexer) == '=') {
                token -> kind = TOK_PLUS_EQ;
                lexer_advance(lexer);
                break;
            }

            if (lexer_current(lexer) == '+') {
                lexer_advance(lexer);

                token -> kind = TOK_ERROR;
                token -> start  = start - lexer -> file -> buffer.ptr;
                token -> length = lexer -> cursor - start;

                diagnostic_add_token(
                    lexer -> file,
                    DIAG_ERROR,
                    token,
                    DIAG_LOC_WHOLE_TOK,
                    "invalid operator '++'",
                    "use '+= 1' instead"
                );

                lexer -> file -> stage = FILE_ERROR;

                break;
            }

            token -> kind = TOK_PLUS;
        } break;

        case '-': {
            if (lexer_current(lexer) == '=') {
                token -> kind = TOK_MINUS_EQ;
                lexer_advance(lexer);
                break;
            }

            if (lexer_current(lexer) == '-') {
                lexer_advance(lexer);

                token -> kind = TOK_ERROR;
                token -> start  = start - lexer -> file -> buffer.ptr;
                token -> length = lexer -> cursor - start;

                diagnostic_add_token(
                    lexer -> file,
                    DIAG_ERROR,
                    token,
                    DIAG_LOC_WHOLE_TOK,
                    "invalid operator '--'",
                    "use '-= 1' instead"
                );

                lexer -> file -> stage = FILE_ERROR;

                break;
            }

            if (lexer_current(lexer) == '>') {
                token -> kind = TOK_ARROW;
                lexer_advance(lexer);
                break;
            }

            token -> kind = TOK_MINUS;
        } break;

        case '*': {
            if (lexer_current(lexer) == '=') {
                token -> kind = TOK_STAR_EQ;
                lexer_advance(lexer);
                break;
            }

            token -> kind = TOK_STAR;
        } break;

        case '/': {
            if (lexer_current(lexer) == '=') {
                token -> kind = TOK_SLASH_EQ;
                lexer_advance(lexer);
                break;
            }

            if (lexer_current(lexer) == '/') {
                lexer -> file -> tokens.count--;
                lexer -> file -> source_locations.count--;

                while (lexer_current(lexer) != 0 && lexer_current(lexer) != '\n') {
                    lexer_advance(lexer);
                }

                if (lexer_current(lexer) != 0) {
                    lexer_advance(lexer);
                }

                break;
            }

            token -> kind = TOK_SLASH;
        } break;

        case '%': {
            if (lexer_current(lexer) == '=') {
                token -> kind = TOK_PERCENT_EQ;
                lexer_advance(lexer);
                break;
            }

            token -> kind = TOK_PERCENT;
        } break;

        case '~': {
            token -> kind = TOK_TILDE;
        } break;

        case '^': {
            if (lexer_current(lexer) == '=') {
                token -> kind = TOK_CARET_EQ;
                lexer_advance(lexer);
                break;
            }

            token -> kind = TOK_CARET;
        } break;

        case '&': {
            if (lexer_current(lexer) == '=') {
                token -> kind = TOK_AMP_EQ;
                lexer_advance(lexer);
                break;
            }

            if (lexer_current(lexer) == '&') {
                token -> kind = TOK_AMP_AMP;
                lexer_advance(lexer);
                break;
            }

            token -> kind = TOK_AMP;
        } break;

        case '|': {
            if (lexer_current(lexer) == '=') {
                token -> kind = TOK_PIPE_EQ;
                lexer_advance(lexer);
                break;
            }

            if (lexer_current(lexer) == '|') {
                token -> kind = TOK_PIPE_PIPE;
                lexer_advance(lexer);
                break;
            }

            token -> kind = TOK_PIPE;
        } break;

        case '>': {
            if (lexer_current(lexer) == '=') {
                token -> kind = TOK_GT_EQ;
                lexer_advance(lexer);
                break;
            }

            if (lexer_current(lexer) == '>') {
                token -> kind = TOK_SHR;
                lexer_advance(lexer);

                if (lexer_current(lexer) == '=') {
                    token -> kind = TOK_SHR_EQ;
                    lexer_advance(lexer);
                }

                break;
            }

            token -> kind = TOK_GT;
        } break;

        case '<': {
            if (lexer_current(lexer) == '=') {
                token -> kind = TOK_LT_EQ;
                lexer_advance(lexer);
                break;
            }

            if (lexer_current(lexer) == '<') {
                token -> kind = TOK_SHL;
                lexer_advance(lexer);

                if (lexer_current(lexer) == '=') {
                    token -> kind = TOK_SHL_EQ;
                    lexer_advance(lexer);
                }

                break;
            }

            token -> kind = TOK_LT;
        } break;

        case '.': {
            if (lexer_current(lexer) == '.') {
                token -> kind = TOK_DOT_DOT;
                lexer_advance(lexer);

                if (lexer_current(lexer) == '.') {
                    token -> kind = TOK_ELLIPSIS;
                    lexer_advance(lexer);
                }

                break;
            }

            token -> kind = TOK_DOT;
        } break;
    }

    token -> start  = start - lexer -> file -> buffer.ptr;
    token -> length = lexer -> cursor - start;
}

static void lex_delimiter(Lexer* lexer) {
    Token* token = tokens_get_new_token(lexer);

    const char* start = lexer -> cursor;

    lexer_advance(lexer);
    
    token -> start = start - lexer -> file -> buffer.ptr;
    token -> length = lexer -> cursor - start;

    switch (*start) {
        case ',': {
            token -> kind = TOK_COMMA;
        } break;

        case ';': {
            token -> kind = TOK_SEMI;
        } break;

        case ':': {
            token -> kind = TOK_COLON;

            if (lexer_current(lexer) == ':') {
                token -> kind = TOK_COLON_COLON;
                token -> length += 1;

                lexer_advance(lexer);
            }
        } break;

        case '(': {
            token -> kind = TOK_L_PAREN;

            delimiter_stack_push(lexer, lexer -> file -> tokens.count - 1);
        } break;

        case ')': {
            token -> kind = TOK_R_PAREN;

            delimiter_match(lexer, token);
        } break;

        case '[': {
            token -> kind = TOK_L_BRACKET;

            delimiter_stack_push(lexer, lexer -> file -> tokens.count - 1);
        } break;

        case ']': {
            token -> kind = TOK_R_BRACKET;

            delimiter_match(lexer, token);
        } break;

        case '{': {
            token -> kind = TOK_L_BRACE;

            delimiter_stack_push(lexer, lexer -> file -> tokens.count - 1);
        } break;

        case '}': {
            token -> kind = TOK_R_BRACE;

            delimiter_match(lexer, token);
        } break;

        case '\0': {
            token -> kind = TOK_EOF;

            lexer -> cursor = lexer -> end;
        } break;
    }
}

static void lex_char_lit(Lexer* lexer) {
    Token* token = tokens_get_new_token(lexer);

    token -> kind = TOK_CHAR_LIT;
    token -> start = lexer -> cursor - lexer -> file -> buffer.ptr; 

    const char* start = lexer -> cursor;

    lexer_advance(lexer);

    // empty char literal
    if (lexer_current(lexer) == '\'') {
        token -> kind = TOK_ERROR;
        token -> length = lexer -> cursor - start;

        lexer -> file -> stage = FILE_ERROR;

        diagnostic_add_token(
            lexer -> file,
            DIAG_ERROR,
            token,
            DIAG_LOC_WHOLE_TOK,
            "empty char literal",
            "add a char to this literal"
        );

        return;
    }

    if (lexer_current(lexer) == '\\') {
        lexer_advance(lexer);
    }

    lexer_advance(lexer);

    // unterminated char literal
    if (lexer_current(lexer) != '\'') {
        token -> kind = TOK_ERROR;
        token -> length = lexer -> cursor - start;

        lexer -> file -> stage = FILE_ERROR;

        diagnostic_add_token(
            lexer -> file,
            DIAG_ERROR,
            token,
            DIAG_LOC_WHOLE_TOK,
            "unterminated char literal",
            "add the closing delimiter to this char literal"
        );

        lexer_advance(lexer);

        return;
    }

    token -> start = start - lexer -> file -> buffer.ptr;
    token -> length = ++lexer -> cursor - start; 
}

static void lex_string_lit(Lexer* lexer) {
    Token* token = tokens_get_new_token(lexer);

    const char* start = lexer -> cursor;

    token -> kind = TOK_STRING_LIT;
    token -> start = start - lexer -> file -> buffer.ptr; 

    lexer_advance(lexer);

    while (lexer_current(lexer) != '\"' && lexer_current(lexer) != 0) {
        lexer_advance(lexer);

        if (lexer_current(lexer) == '\\') {
            lexer_advance_by(lexer, 2);
        }
    }

    // unterminated string literal
    if (lexer_current(lexer) != '\"') {
        token -> kind = TOK_ERROR;
        token -> length = lexer -> cursor - start;

        lexer -> file -> stage = FILE_ERROR;

        diagnostic_add_token(
            lexer -> file,
            DIAG_ERROR,
            token,
            DIAG_LOC_WHOLE_TOK,
            "unterminated string literal",
            "add the closing delimiter to this string literal"
        );
    }

    lexer_advance(lexer);

    u32 length = lexer -> cursor - start;

    assert(length < U16_MAX);

    token -> length = length;
}

static void lex_invalid(Lexer* lexer) {
    lexer -> file -> stage = FILE_ERROR;

    Token* token = tokens_get_new_token(lexer);

    const char* start = lexer -> cursor;

    while (
        !IS_ALPHA(lexer_current(lexer))            &&
        !IS_DIGIT(lexer_current(lexer))            &&
        !IS_OPERATOR(lexer_current(lexer))         &&
        !IS_CHAR_DELIM(lexer_current(lexer))       &&
        !IS_STRING_DELIM(lexer_current(lexer))     &&
        !IS_DELIMITER(lexer_current(lexer))        &&
        !IS_WHITESPACE(lexer_current(lexer))
    ) {
        lexer_advance(lexer);
    }

    token -> kind = TOK_ERROR;
    token -> start = start - lexer -> file -> buffer.ptr;

    u32 length = lexer -> cursor - start;

    assert(length < U16_MAX);

    token -> length = length;

    diagnostic_add_token(
        lexer -> file,
        DIAG_ERROR,
        token,
        DIAG_LOC_WHOLE_TOK,
        "unknown token",
        null 
    );
}

static bool delimiter_matches(TokenKind open, TokenKind close) {
    switch (open) {
        case TOK_L_PAREN:   return close == TOK_R_PAREN;
        case TOK_L_BRACKET: return close == TOK_R_BRACKET;
        case TOK_L_BRACE:   return close == TOK_R_BRACE;
        default:            return false;
    }
}

static void delimiter_match(Lexer* lexer, Token* token) {
    if (lexer -> stack.top == 0) {
        diagnostic_add_token(
            lexer -> file,
            DIAG_ERROR,
            token,
            DIAG_LOC_WHOLE_TOK,
            "unopened delimiter",
            null
        );

        lexer -> file -> stage = FILE_ERROR;
        return;
    }

    Token* open = &lexer -> file -> tokens.items[lexer -> stack.items[lexer -> stack.top - 1]];

    if (!delimiter_matches(open -> kind, token -> kind)) {
        diagnostic_add_token(
            lexer -> file,
            DIAG_ERROR,
            token,
            DIAG_LOC_WHOLE_TOK,
            "mismatched delimiters",
            null
        );

        lexer -> file -> stage = FILE_ERROR;
        return;
    }

    lexer -> stack.top--;
}

static void delimiter_stack_push(Lexer* lexer, u32 index) {
    assert(lexer -> stack.top < DELIMITER_STACK_MAX_DEPTH && "Max delimiter stack depth");
    lexer -> stack.items[lexer -> stack.top++] = index;
}

static inline void lexer_advance(Lexer* lexer) {
    lexer -> cursor++;
    lexer -> col++;
}

static inline void lexer_advance_by(Lexer* lexer, u32 n) {
    lexer -> cursor += n;
    lexer -> col += n;
}

static inline char lexer_current(Lexer* lexer) {
    return *(lexer -> cursor);
}

// asserts that tokens.count == locations.count
static void set_source_location(Lexer* lexer) {
    SourceLocationArray* arr = &lexer -> file -> source_locations;

    if (UNLIKELY(arr -> count >= arr -> capacity)) {
        u64 old_size = arr -> capacity * sizeof(SourceLocation);
        u64 new_size = old_size * 2;

        assert(new_size > old_size);

        arr -> items = arena_realloc(&arr -> arena, arr -> items, old_size, new_size);
        arr -> capacity *= 2;

        debug_log("Reallocated SourceLocationArray arr -> items from %lu to %lu bytes", old_size, new_size);
    }

    arr -> items[arr -> count++] = (SourceLocation) {
        .line = lexer -> line,
        .col = lexer -> col
    };

    assert(lexer -> file -> tokens.count == arr -> count);
}

static Token* tokens_get_new_token(Lexer* lexer) {
    TokenArray* arr = &lexer -> file -> tokens;

    debug_assert(arr != null);
    debug_assert(arr -> items != null);
    debug_assert(arr -> capacity > 0);
    debug_assert(arr -> count <= arr -> capacity);

    if (UNLIKELY(arr -> count >= arr -> capacity)) {
        u64 old_size = arr -> capacity * sizeof(Token);
        u64 new_size = old_size * 2;

        assert(new_size > old_size);

        arr -> items = arena_realloc(&arr -> arena, arr -> items, old_size, new_size);
        arr -> capacity *= 2;

        debug_log("Reallocated TokenArray arr -> items from %lu to %lu bytes", old_size, new_size);
    }

    Token* token = &arr -> items[arr -> count++];

    set_source_location(lexer);

    return token;
}
