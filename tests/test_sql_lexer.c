/*
 * test_sql_lexer.c - Lexer tests
 */

#include "sql/lexer.h"
#include "test_harness.h"
#include <stdio.h>
#include <string.h>

/*
 * Test: Keywords
 */
int test_lexer_keywords(void) {
    struct sql_lexer lex;
    struct sql_token token;

    printf("Testing keyword recognition...\n");

    lexer_init(&lex, "SELECT INSERT UPDATE DELETE CREATE DROP TABLE");

    /* SELECT */
    lexer_next(&lex, &token);
    if (token.type != TOKEN_KEYWORD || token.keyword_id != KW_SELECT) {
        printf("  ERROR: Expected SELECT keyword\n");
        return -1;
    }

    /* INSERT */
    lexer_next(&lex, &token);
    if (token.type != TOKEN_KEYWORD || token.keyword_id != KW_INSERT) {
        printf("  ERROR: Expected INSERT keyword\n");
        return -1;
    }

    /* UPDATE */
    lexer_next(&lex, &token);
    if (token.type != TOKEN_KEYWORD || token.keyword_id != KW_UPDATE) {
        printf("  ERROR: Expected UPDATE keyword\n");
        return -1;
    }

    /* DELETE */
    lexer_next(&lex, &token);
    if (token.type != TOKEN_KEYWORD || token.keyword_id != KW_DELETE) {
        printf("  ERROR: Expected DELETE keyword\n");
        return -1;
    }

    /* CREATE */
    lexer_next(&lex, &token);
    if (token.type != TOKEN_KEYWORD || token.keyword_id != KW_CREATE) {
        printf("  ERROR: Expected CREATE keyword\n");
        return -1;
    }

    /* DROP */
    lexer_next(&lex, &token);
    if (token.type != TOKEN_KEYWORD || token.keyword_id != KW_DROP) {
        printf("  ERROR: Expected DROP keyword\n");
        return -1;
    }

    /* TABLE */
    lexer_next(&lex, &token);
    if (token.type != TOKEN_KEYWORD || token.keyword_id != KW_TABLE) {
        printf("  ERROR: Expected TABLE keyword\n");
        return -1;
    }

    /* EOF */
    lexer_next(&lex, &token);
    if (token.type != TOKEN_EOF) {
        printf("  ERROR: Expected EOF\n");
        return -1;
    }

    /* Test case insensitivity */
    lexer_init(&lex, "select SeLeCt SELECT");

    lexer_next(&lex, &token);
    if (token.keyword_id != KW_SELECT) return -1;

    lexer_next(&lex, &token);
    if (token.keyword_id != KW_SELECT) return -1;

    lexer_next(&lex, &token);
    if (token.keyword_id != KW_SELECT) return -1;

    return 0;
}

/*
 * Test: Identifiers
 */
int test_lexer_identifiers(void) {
    struct sql_lexer lex;
    struct sql_token token;

    printf("Testing identifier parsing...\n");

    lexer_init(&lex, "users table_name column1 _id my_table123");

    /* users */
    lexer_next(&lex, &token);
    if (token.type != TOKEN_IDENTIFIER || strcmp(token.text, "users") != 0) {
        printf("  ERROR: Expected identifier 'users'\n");
        return -1;
    }

    /* table_name */
    lexer_next(&lex, &token);
    if (token.type != TOKEN_IDENTIFIER || strcmp(token.text, "table_name") != 0) {
        printf("  ERROR: Expected identifier 'table_name'\n");
        return -1;
    }

    /* column1 */
    lexer_next(&lex, &token);
    if (token.type != TOKEN_IDENTIFIER || strcmp(token.text, "column1") != 0) {
        printf("  ERROR: Expected identifier 'column1'\n");
        return -1;
    }

    /* _id */
    lexer_next(&lex, &token);
    if (token.type != TOKEN_IDENTIFIER || strcmp(token.text, "_id") != 0) {
        printf("  ERROR: Expected identifier '_id'\n");
        return -1;
    }

    /* my_table123 */
    lexer_next(&lex, &token);
    if (token.type != TOKEN_IDENTIFIER || strcmp(token.text, "my_table123") != 0) {
        printf("  ERROR: Expected identifier 'my_table123'\n");
        return -1;
    }

    return 0;
}

/*
 * Test: Integers
 */
int test_lexer_integers(void) {
    struct sql_lexer lex;
    struct sql_token token;

    printf("Testing integer parsing...\n");

    lexer_init(&lex, "0 123 -456 2147483647");

    /* 0 */
    lexer_next(&lex, &token);
    if (token.type != TOKEN_INTEGER || token.int_value != 0) {
        printf("  ERROR: Expected integer 0, got %d\n", token.int_value);
        return -1;
    }

    /* 123 */
    lexer_next(&lex, &token);
    if (token.type != TOKEN_INTEGER || token.int_value != 123) {
        printf("  ERROR: Expected integer 123, got %d\n", token.int_value);
        return -1;
    }

    /* -456 */
    lexer_next(&lex, &token);
    if (token.type != TOKEN_INTEGER || token.int_value != -456) {
        printf("  ERROR: Expected integer -456, got %d\n", token.int_value);
        return -1;
    }

    /* 2147483647 (max int32) */
    lexer_next(&lex, &token);
    if (token.type != TOKEN_INTEGER || token.int_value != 2147483647) {
        printf("  ERROR: Expected integer 2147483647, got %d\n", token.int_value);
        return -1;
    }

    return 0;
}

/*
 * Test: Strings
 */
int test_lexer_strings(void) {
    struct sql_lexer lex;
    struct sql_token token;

    printf("Testing string parsing...\n");

    lexer_init(&lex, "'hello' 'world' 'it''s' ''");

    /* 'hello' */
    lexer_next(&lex, &token);
    if (token.type != TOKEN_STRING || strcmp(token.text, "hello") != 0) {
        printf("  ERROR: Expected string 'hello', got '%s'\n", token.text);
        return -1;
    }

    /* 'world' */
    lexer_next(&lex, &token);
    if (token.type != TOKEN_STRING || strcmp(token.text, "world") != 0) {
        printf("  ERROR: Expected string 'world', got '%s'\n", token.text);
        return -1;
    }

    /* 'it''s' (escaped quote) */
    lexer_next(&lex, &token);
    if (token.type != TOKEN_STRING || strcmp(token.text, "it's") != 0) {
        printf("  ERROR: Expected string 'it's', got '%s'\n", token.text);
        return -1;
    }

    /* '' (empty string) */
    lexer_next(&lex, &token);
    if (token.type != TOKEN_STRING || strcmp(token.text, "") != 0) {
        printf("  ERROR: Expected empty string, got '%s'\n", token.text);
        return -1;
    }

    return 0;
}

/*
 * Test: Symbols
 */
int test_lexer_symbols(void) {
    struct sql_lexer lex;
    struct sql_token token;

    printf("Testing symbol parsing...\n");

    lexer_init(&lex, "( ) , ; = < > <= >= != <> *");

    /* ( */
    lexer_next(&lex, &token);
    if (token.type != TOKEN_SYMBOL || token.symbol_id != SYM_LPAREN) {
        printf("  ERROR: Expected (\n");
        return -1;
    }

    /* ) */
    lexer_next(&lex, &token);
    if (token.type != TOKEN_SYMBOL || token.symbol_id != SYM_RPAREN) {
        printf("  ERROR: Expected )\n");
        return -1;
    }

    /* , */
    lexer_next(&lex, &token);
    if (token.type != TOKEN_SYMBOL || token.symbol_id != SYM_COMMA) {
        printf("  ERROR: Expected ,\n");
        return -1;
    }

    /* ; */
    lexer_next(&lex, &token);
    if (token.type != TOKEN_SYMBOL || token.symbol_id != SYM_SEMICOLON) {
        printf("  ERROR: Expected ;\n");
        return -1;
    }

    /* = */
    lexer_next(&lex, &token);
    if (token.type != TOKEN_SYMBOL || token.symbol_id != SYM_EQUAL) {
        printf("  ERROR: Expected =\n");
        return -1;
    }

    /* < */
    lexer_next(&lex, &token);
    if (token.type != TOKEN_SYMBOL || token.symbol_id != SYM_LT) {
        printf("  ERROR: Expected <\n");
        return -1;
    }

    /* > */
    lexer_next(&lex, &token);
    if (token.type != TOKEN_SYMBOL || token.symbol_id != SYM_GT) {
        printf("  ERROR: Expected >\n");
        return -1;
    }

    /* <= */
    lexer_next(&lex, &token);
    if (token.type != TOKEN_SYMBOL || token.symbol_id != SYM_LE) {
        printf("  ERROR: Expected <=\n");
        return -1;
    }

    /* >= */
    lexer_next(&lex, &token);
    if (token.type != TOKEN_SYMBOL || token.symbol_id != SYM_GE) {
        printf("  ERROR: Expected >=\n");
        return -1;
    }

    /* != */
    lexer_next(&lex, &token);
    if (token.type != TOKEN_SYMBOL || token.symbol_id != SYM_NE) {
        printf("  ERROR: Expected !=\n");
        return -1;
    }

    /* <> */
    lexer_next(&lex, &token);
    if (token.type != TOKEN_SYMBOL || token.symbol_id != SYM_NE) {
        printf("  ERROR: Expected <>\n");
        return -1;
    }

    /* * */
    lexer_next(&lex, &token);
    if (token.type != TOKEN_SYMBOL || token.symbol_id != SYM_STAR) {
        printf("  ERROR: Expected *\n");
        return -1;
    }

    return 0;
}

/*
 * Test: Whitespace handling
 */
int test_lexer_whitespace(void) {
    struct sql_lexer lex;
    struct sql_token token;

    printf("Testing whitespace handling...\n");

    lexer_init(&lex, "  SELECT   \t\n  FROM   \r\n  users  ");

    /* SELECT */
    lexer_next(&lex, &token);
    if (token.keyword_id != KW_SELECT) {
        printf("  ERROR: Expected SELECT\n");
        return -1;
    }

    /* FROM */
    lexer_next(&lex, &token);
    if (token.keyword_id != KW_FROM) {
        printf("  ERROR: Expected FROM\n");
        return -1;
    }

    /* users */
    lexer_next(&lex, &token);
    if (token.type != TOKEN_IDENTIFIER || strcmp(token.text, "users") != 0) {
        printf("  ERROR: Expected identifier 'users'\n");
        return -1;
    }

    /* EOF */
    lexer_next(&lex, &token);
    if (token.type != TOKEN_EOF) {
        printf("  ERROR: Expected EOF\n");
        return -1;
    }

    return 0;
}

/*
 * Test: Comment handling
 */
int test_lexer_comments(void) {
    struct sql_lexer lex;
    struct sql_token token;

    printf("Testing comment handling...\n");

    lexer_init(&lex, "SELECT -- this is a comment\nFROM -- another comment\nusers");

    /* SELECT */
    lexer_next(&lex, &token);
    if (token.keyword_id != KW_SELECT) {
        printf("  ERROR: Expected SELECT\n");
        return -1;
    }

    /* FROM */
    lexer_next(&lex, &token);
    if (token.keyword_id != KW_FROM) {
        printf("  ERROR: Expected FROM\n");
        return -1;
    }

    /* users */
    lexer_next(&lex, &token);
    if (token.type != TOKEN_IDENTIFIER || strcmp(token.text, "users") != 0) {
        printf("  ERROR: Expected identifier 'users'\n");
        return -1;
    }

    return 0;
}

/*
 * Test: Multi-token SQL statement
 */
int test_lexer_multitoken(void) {
    struct sql_lexer lex;
    struct sql_token token;

    printf("Testing multi-token SQL statement...\n");

    lexer_init(&lex, "SELECT * FROM users WHERE id = 123;");

    /* SELECT */
    lexer_next(&lex, &token);
    if (token.keyword_id != KW_SELECT) return -1;

    /* * */
    lexer_next(&lex, &token);
    if (token.symbol_id != SYM_STAR) return -1;

    /* FROM */
    lexer_next(&lex, &token);
    if (token.keyword_id != KW_FROM) return -1;

    /* users */
    lexer_next(&lex, &token);
    if (token.type != TOKEN_IDENTIFIER) return -1;

    /* WHERE */
    lexer_next(&lex, &token);
    if (token.keyword_id != KW_WHERE) return -1;

    /* id */
    lexer_next(&lex, &token);
    if (token.type != TOKEN_IDENTIFIER) return -1;

    /* = */
    lexer_next(&lex, &token);
    if (token.symbol_id != SYM_EQUAL) return -1;

    /* 123 */
    lexer_next(&lex, &token);
    if (token.type != TOKEN_INTEGER || token.int_value != 123) return -1;

    /* ; */
    lexer_next(&lex, &token);
    if (token.symbol_id != SYM_SEMICOLON) return -1;

    /* EOF */
    lexer_next(&lex, &token);
    if (token.type != TOKEN_EOF) return -1;

    return 0;
}
