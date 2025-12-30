/*
 * lexer.c - SQL lexer implementation
 */

#include "sql/lexer.h"
#include <string.h>
#include <ctype.h>
#include <stdio.h>

/* Forward declarations */
static void skip_whitespace(struct sql_lexer *lex);
static void skip_comment(struct sql_lexer *lex);
static int read_identifier_or_keyword(struct sql_lexer *lex, struct sql_token *token);
static int read_number(struct sql_lexer *lex, struct sql_token *token);
static int read_string(struct sql_lexer *lex, struct sql_token *token);
static int read_symbol(struct sql_lexer *lex, struct sql_token *token);
static void advance(struct sql_lexer *lex);
static char peek(struct sql_lexer *lex);
static char peek_next(struct sql_lexer *lex);

/*
 * Initialize lexer
 */
void lexer_init(struct sql_lexer *lex, const char *input) {
    lex->input = input;
    lex->current = input;
    lex->line = 1;
    lex->column = 1;
}

/*
 * Check if text is a keyword
 */
uint32_t lexer_keyword_id(const char *text) {
    /* Convert to uppercase for comparison */
    char upper[256];
    int i;

    for (i = 0; text[i] && i < 255; i++) {
        upper[i] = toupper((unsigned char)text[i]);
    }
    upper[i] = '\0';

    /* Check all keywords */
    if (strcmp(upper, "SELECT") == 0) return KW_SELECT;
    if (strcmp(upper, "INSERT") == 0) return KW_INSERT;
    if (strcmp(upper, "UPDATE") == 0) return KW_UPDATE;
    if (strcmp(upper, "DELETE") == 0) return KW_DELETE;
    if (strcmp(upper, "CREATE") == 0) return KW_CREATE;
    if (strcmp(upper, "DROP") == 0) return KW_DROP;
    if (strcmp(upper, "TABLE") == 0) return KW_TABLE;
    if (strcmp(upper, "INDEX") == 0) return KW_INDEX;
    if (strcmp(upper, "FROM") == 0) return KW_FROM;
    if (strcmp(upper, "WHERE") == 0) return KW_WHERE;
    if (strcmp(upper, "INTO") == 0) return KW_INTO;
    if (strcmp(upper, "VALUES") == 0) return KW_VALUES;
    if (strcmp(upper, "SET") == 0) return KW_SET;
    if (strcmp(upper, "ORDER") == 0) return KW_ORDER;
    if (strcmp(upper, "BY") == 0) return KW_BY;
    if (strcmp(upper, "LIMIT") == 0) return KW_LIMIT;
    if (strcmp(upper, "PRIMARY") == 0) return KW_PRIMARY;
    if (strcmp(upper, "KEY") == 0) return KW_KEY;
    if (strcmp(upper, "INTEGER") == 0) return KW_INTEGER;
    if (strcmp(upper, "TEXT") == 0) return KW_TEXT;
    if (strcmp(upper, "BLOB") == 0) return KW_BLOB;
    if (strcmp(upper, "NULL") == 0) return KW_NULL;
    if (strcmp(upper, "AND") == 0) return KW_AND;
    if (strcmp(upper, "OR") == 0) return KW_OR;
    if (strcmp(upper, "NOT") == 0) return KW_NOT;
    if (strcmp(upper, "ASC") == 0) return KW_ASC;
    if (strcmp(upper, "DESC") == 0) return KW_DESC;
    if (strcmp(upper, "COUNT") == 0) return KW_COUNT;
    if (strcmp(upper, "SUM") == 0) return KW_SUM;
    if (strcmp(upper, "AVG") == 0) return KW_AVG;
    if (strcmp(upper, "MIN") == 0) return KW_MIN;
    if (strcmp(upper, "MAX") == 0) return KW_MAX;

    return 0;  /* Not a keyword */
}

/*
 * Get next token
 */
int lexer_next(struct sql_lexer *lex, struct sql_token *token) {
    /* Skip whitespace and comments */
    while (1) {
        skip_whitespace(lex);

        if (peek(lex) == '-' && peek_next(lex) == '-') {
            skip_comment(lex);
        } else {
            break;
        }
    }

    /* Save token position */
    token->line = lex->line;
    token->column = lex->column;
    token->int_value = 0;
    token->keyword_id = 0;
    token->symbol_id = 0;
    token->text[0] = '\0';

    /* Check for EOF */
    if (peek(lex) == '\0') {
        token->type = TOKEN_EOF;
        strcpy(token->text, "<EOF>");
        return 0;
    }

    /* Identifier or keyword */
    if (isalpha((unsigned char)peek(lex)) || peek(lex) == '_') {
        return read_identifier_or_keyword(lex, token);
    }

    /* Number */
    if (isdigit((unsigned char)peek(lex)) || (peek(lex) == '-' && isdigit((unsigned char)peek_next(lex)))) {
        return read_number(lex, token);
    }

    /* String */
    if (peek(lex) == '\'') {
        return read_string(lex, token);
    }

    /* Symbol */
    return read_symbol(lex, token);
}

/*
 * Peek at next token without consuming it
 */
int lexer_peek(struct sql_lexer *lex, struct sql_token *token) {
    struct sql_lexer saved = *lex;
    int result = lexer_next(lex, token);
    *lex = saved;
    return result;
}

/*
 * Skip whitespace
 */
static void skip_whitespace(struct sql_lexer *lex) {
    while (peek(lex) == ' ' || peek(lex) == '\t' || peek(lex) == '\n' || peek(lex) == '\r') {
        advance(lex);
    }
}

/*
 * Skip comment (-- to end of line)
 */
static void skip_comment(struct sql_lexer *lex) {
    /* Skip -- */
    advance(lex);
    advance(lex);

    /* Skip to end of line */
    while (peek(lex) != '\n' && peek(lex) != '\0') {
        advance(lex);
    }
}

/*
 * Read identifier or keyword
 */
static int read_identifier_or_keyword(struct sql_lexer *lex, struct sql_token *token) {
    int i = 0;

    /* Read alphanumeric + underscore */
    while ((isalnum((unsigned char)peek(lex)) || peek(lex) == '_') && i < 255) {
        token->text[i++] = peek(lex);
        advance(lex);
    }
    token->text[i] = '\0';

    /* Check if keyword */
    token->keyword_id = lexer_keyword_id(token->text);
    if (token->keyword_id != 0) {
        token->type = TOKEN_KEYWORD;
    } else {
        token->type = TOKEN_IDENTIFIER;
    }

    return 0;
}

/*
 * Read number
 */
static int read_number(struct sql_lexer *lex, struct sql_token *token) {
    int i = 0;
    int negative = 0;
    int32_t value = 0;

    /* Handle negative */
    if (peek(lex) == '-') {
        negative = 1;
        token->text[i++] = peek(lex);
        advance(lex);
    }

    /* Read digits */
    while (isdigit((unsigned char)peek(lex)) && i < 255) {
        token->text[i++] = peek(lex);
        value = value * 10 + (peek(lex) - '0');
        advance(lex);
    }
    token->text[i] = '\0';

    token->type = TOKEN_INTEGER;
    token->int_value = negative ? -value : value;

    return 0;
}

/*
 * Read string literal (single quotes, '' for escape)
 */
static int read_string(struct sql_lexer *lex, struct sql_token *token) {
    int i = 0;

    /* Skip opening quote */
    advance(lex);

    /* Read until closing quote */
    while (peek(lex) != '\0' && i < 255) {
        if (peek(lex) == '\'') {
            /* Check for '' escape */
            if (peek_next(lex) == '\'') {
                token->text[i++] = '\'';
                advance(lex);
                advance(lex);
            } else {
                /* End of string */
                advance(lex);
                break;
            }
        } else {
            token->text[i++] = peek(lex);
            advance(lex);
        }
    }
    token->text[i] = '\0';

    token->type = TOKEN_STRING;
    return 0;
}

/*
 * Read symbol
 */
static int read_symbol(struct sql_lexer *lex, struct sql_token *token) {
    char ch = peek(lex);
    char next = peek_next(lex);

    token->type = TOKEN_SYMBOL;

    /* Check multi-character symbols */
    if (ch == '<' && next == '=') {
        token->symbol_id = SYM_LE;
        strcpy(token->text, "<=");
        advance(lex);
        advance(lex);
        return 0;
    }

    if (ch == '>' && next == '=') {
        token->symbol_id = SYM_GE;
        strcpy(token->text, ">=");
        advance(lex);
        advance(lex);
        return 0;
    }

    if (ch == '!' && next == '=') {
        token->symbol_id = SYM_NE;
        strcpy(token->text, "!=");
        advance(lex);
        advance(lex);
        return 0;
    }

    if (ch == '<' && next == '>') {
        token->symbol_id = SYM_NE;
        strcpy(token->text, "<>");
        advance(lex);
        advance(lex);
        return 0;
    }

    /* Single character symbol */
    token->symbol_id = (unsigned char)ch;
    token->text[0] = ch;
    token->text[1] = '\0';
    advance(lex);

    return 0;
}

/*
 * Advance to next character
 */
static void advance(struct sql_lexer *lex) {
    if (*lex->current == '\0') {
        return;
    }

    if (*lex->current == '\n') {
        lex->line++;
        lex->column = 1;
    } else {
        lex->column++;
    }

    lex->current++;
}

/*
 * Peek at current character
 */
static char peek(struct sql_lexer *lex) {
    return *lex->current;
}

/*
 * Peek at next character
 */
static char peek_next(struct sql_lexer *lex) {
    if (*lex->current == '\0') {
        return '\0';
    }
    return *(lex->current + 1);
}
