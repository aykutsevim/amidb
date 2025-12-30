/*
 * lexer.h - SQL lexer (tokenizer) for AmiDB
 *
 * Tokenizes SQL statements into keywords, identifiers, integers, strings, and symbols.
 * Supports case-insensitive keywords, single-quote strings, and -- comments.
 */

#ifndef AMIDB_SQL_LEXER_H
#define AMIDB_SQL_LEXER_H

#include <stdint.h>

/* Token types */
#define TOKEN_EOF           0
#define TOKEN_KEYWORD       1
#define TOKEN_IDENTIFIER    2
#define TOKEN_INTEGER       3
#define TOKEN_STRING        4
#define TOKEN_SYMBOL        5
#define TOKEN_ERROR         99

/* Keyword constants (for fast comparison) */
#define KW_SELECT       1
#define KW_INSERT       2
#define KW_UPDATE       3
#define KW_DELETE       4
#define KW_CREATE       5
#define KW_DROP         6
#define KW_TABLE        7
#define KW_INDEX        8
#define KW_FROM         9
#define KW_WHERE        10
#define KW_INTO         11
#define KW_VALUES       12
#define KW_SET          13
#define KW_ORDER        14
#define KW_BY           15
#define KW_LIMIT        16
#define KW_PRIMARY      17
#define KW_KEY          18
#define KW_INTEGER      19
#define KW_TEXT         20
#define KW_BLOB         21
#define KW_NULL         22
#define KW_AND          23
#define KW_OR           24
#define KW_NOT          25
#define KW_ASC          26
#define KW_DESC         27
#define KW_COUNT        28
#define KW_SUM          29
#define KW_AVG          30
#define KW_MIN          31
#define KW_MAX          32

/* Symbol constants */
#define SYM_LPAREN      '('
#define SYM_RPAREN      ')'
#define SYM_COMMA       ','
#define SYM_SEMICOLON   ';'
#define SYM_EQUAL       '='
#define SYM_LT          '<'
#define SYM_GT          '>'
#define SYM_STAR        '*'

/* Multi-character symbols (use high values to avoid collision with single chars) */
#define SYM_LE          256  /* <= */
#define SYM_GE          257  /* >= */
#define SYM_NE          258  /* != or <> */

/* Token structure */
struct sql_token {
    uint8_t type;           /* TOKEN_* constant */
    char text[256];         /* Original text of token */
    int32_t int_value;      /* Parsed integer value (if type == TOKEN_INTEGER) */
    uint32_t keyword_id;    /* KW_* constant (if type == TOKEN_KEYWORD) */
    uint32_t symbol_id;     /* SYM_* constant (if type == TOKEN_SYMBOL) */
    uint32_t line;          /* Line number (1-based) */
    uint32_t column;        /* Column number (1-based) */
};

/* Lexer state */
struct sql_lexer {
    const char *input;      /* Original input string */
    const char *current;    /* Current position in input */
    uint32_t line;          /* Current line number */
    uint32_t column;        /* Current column number */
};

/* Initialize lexer with SQL input string */
void lexer_init(struct sql_lexer *lex, const char *input);

/* Get next token from input */
int lexer_next(struct sql_lexer *lex, struct sql_token *token);

/* Peek at next token without consuming it */
int lexer_peek(struct sql_lexer *lex, struct sql_token *token);

/* Check if text is a keyword (returns KW_* constant or 0) */
uint32_t lexer_keyword_id(const char *text);

#endif /* AMIDB_SQL_LEXER_H */
