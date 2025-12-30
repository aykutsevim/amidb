/*
 * test_sql_parser.c - Parser tests
 */

#include "sql/parser.h"
#include "sql/lexer.h"
#include "test_harness.h"
#include <stdio.h>
#include <string.h>

/*
 * Test: CREATE TABLE with explicit PRIMARY KEY
 */
int test_parser_create_explicit_pk(void) {
    struct sql_lexer lex;
    struct sql_parser parser;
    struct sql_statement stmt;
    const char *sql = "CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT)";

    printf("Testing CREATE TABLE with explicit PRIMARY KEY...\n");

    lexer_init(&lex, sql);
    parser_init(&parser, &lex);

    if (parser_parse_statement(&parser, &stmt) != 0) {
        printf("  ERROR: Parse failed: %s\n", parser_get_error(&parser));
        return -1;
    }

    /* Verify statement type */
    if (stmt.type != STMT_CREATE_TABLE) {
        printf("  ERROR: Expected CREATE TABLE statement\n");
        return -1;
    }

    /* Verify table name */
    if (strcmp(stmt.stmt.create_table.table_name, "users") != 0) {
        printf("  ERROR: Expected table name 'users', got '%s'\n",
               stmt.stmt.create_table.table_name);
        return -1;
    }

    /* Verify column count */
    if (stmt.stmt.create_table.column_count != 2) {
        printf("  ERROR: Expected 2 columns, got %d\n",
               stmt.stmt.create_table.column_count);
        return -1;
    }

    /* Verify first column (id INTEGER PRIMARY KEY) */
    if (strcmp(stmt.stmt.create_table.columns[0].name, "id") != 0) {
        printf("  ERROR: Expected column name 'id', got '%s'\n",
               stmt.stmt.create_table.columns[0].name);
        return -1;
    }

    if (stmt.stmt.create_table.columns[0].type != SQL_TYPE_INTEGER) {
        printf("  ERROR: Expected column type INTEGER\n");
        return -1;
    }

    if (!stmt.stmt.create_table.columns[0].is_primary_key) {
        printf("  ERROR: Expected PRIMARY KEY on column 'id'\n");
        return -1;
    }

    /* Verify second column (name TEXT) */
    if (strcmp(stmt.stmt.create_table.columns[1].name, "name") != 0) {
        printf("  ERROR: Expected column name 'name', got '%s'\n",
               stmt.stmt.create_table.columns[1].name);
        return -1;
    }

    if (stmt.stmt.create_table.columns[1].type != SQL_TYPE_TEXT) {
        printf("  ERROR: Expected column type TEXT\n");
        return -1;
    }

    if (stmt.stmt.create_table.columns[1].is_primary_key) {
        printf("  ERROR: Did not expect PRIMARY KEY on column 'name'\n");
        return -1;
    }

    printf("  Parsed: table='%s', columns=%d, pk='%s'\n",
           stmt.stmt.create_table.table_name,
           stmt.stmt.create_table.column_count,
           stmt.stmt.create_table.columns[0].name);

    return 0;
}

/*
 * Test: CREATE TABLE with implicit rowid (no PRIMARY KEY)
 */
int test_parser_create_implicit_rowid(void) {
    struct sql_lexer lex;
    struct sql_parser parser;
    struct sql_statement stmt;
    const char *sql = "CREATE TABLE posts (title TEXT, body TEXT)";
    int i;
    int has_pk = 0;

    printf("Testing CREATE TABLE with implicit rowid...\n");

    lexer_init(&lex, sql);
    parser_init(&parser, &lex);

    if (parser_parse_statement(&parser, &stmt) != 0) {
        printf("  ERROR: Parse failed: %s\n", parser_get_error(&parser));
        return -1;
    }

    /* Verify statement type */
    if (stmt.type != STMT_CREATE_TABLE) {
        printf("  ERROR: Expected CREATE TABLE statement\n");
        return -1;
    }

    /* Verify table name */
    if (strcmp(stmt.stmt.create_table.table_name, "posts") != 0) {
        printf("  ERROR: Expected table name 'posts', got '%s'\n",
               stmt.stmt.create_table.table_name);
        return -1;
    }

    /* Verify column count */
    if (stmt.stmt.create_table.column_count != 2) {
        printf("  ERROR: Expected 2 columns, got %d\n",
               stmt.stmt.create_table.column_count);
        return -1;
    }

    /* Verify no PRIMARY KEY */
    for (i = 0; i < stmt.stmt.create_table.column_count; i++) {
        if (stmt.stmt.create_table.columns[i].is_primary_key) {
            has_pk = 1;
            break;
        }
    }

    if (has_pk) {
        printf("  ERROR: Did not expect any PRIMARY KEY (implicit rowid)\n");
        return -1;
    }

    /* Verify columns */
    if (strcmp(stmt.stmt.create_table.columns[0].name, "title") != 0 ||
        stmt.stmt.create_table.columns[0].type != SQL_TYPE_TEXT) {
        printf("  ERROR: Expected column 'title TEXT'\n");
        return -1;
    }

    if (strcmp(stmt.stmt.create_table.columns[1].name, "body") != 0 ||
        stmt.stmt.create_table.columns[1].type != SQL_TYPE_TEXT) {
        printf("  ERROR: Expected column 'body TEXT'\n");
        return -1;
    }

    printf("  Parsed: table='%s', columns=%d, implicit rowid\n",
           stmt.stmt.create_table.table_name,
           stmt.stmt.create_table.column_count);

    return 0;
}

/*
 * Test: CREATE TABLE with multiple columns and types
 */
int test_parser_create_multiple_columns(void) {
    struct sql_lexer lex;
    struct sql_parser parser;
    struct sql_statement stmt;
    const char *sql = "CREATE TABLE products ("
                      "  id INTEGER PRIMARY KEY,"
                      "  name TEXT,"
                      "  description TEXT,"
                      "  price INTEGER,"
                      "  image BLOB"
                      ")";

    printf("Testing CREATE TABLE with multiple columns...\n");

    lexer_init(&lex, sql);
    parser_init(&parser, &lex);

    if (parser_parse_statement(&parser, &stmt) != 0) {
        printf("  ERROR: Parse failed: %s\n", parser_get_error(&parser));
        return -1;
    }

    /* Verify column count */
    if (stmt.stmt.create_table.column_count != 5) {
        printf("  ERROR: Expected 5 columns, got %d\n",
               stmt.stmt.create_table.column_count);
        return -1;
    }

    /* Verify each column */
    if (strcmp(stmt.stmt.create_table.columns[0].name, "id") != 0 ||
        stmt.stmt.create_table.columns[0].type != SQL_TYPE_INTEGER ||
        !stmt.stmt.create_table.columns[0].is_primary_key) {
        printf("  ERROR: Column 0 mismatch\n");
        return -1;
    }

    if (strcmp(stmt.stmt.create_table.columns[1].name, "name") != 0 ||
        stmt.stmt.create_table.columns[1].type != SQL_TYPE_TEXT) {
        printf("  ERROR: Column 1 mismatch\n");
        return -1;
    }

    if (strcmp(stmt.stmt.create_table.columns[2].name, "description") != 0 ||
        stmt.stmt.create_table.columns[2].type != SQL_TYPE_TEXT) {
        printf("  ERROR: Column 2 mismatch\n");
        return -1;
    }

    if (strcmp(stmt.stmt.create_table.columns[3].name, "price") != 0 ||
        stmt.stmt.create_table.columns[3].type != SQL_TYPE_INTEGER) {
        printf("  ERROR: Column 3 mismatch\n");
        return -1;
    }

    if (strcmp(stmt.stmt.create_table.columns[4].name, "image") != 0 ||
        stmt.stmt.create_table.columns[4].type != SQL_TYPE_BLOB) {
        printf("  ERROR: Column 4 mismatch\n");
        return -1;
    }

    printf("  Parsed: table='%s', columns=%d, all types present\n",
           stmt.stmt.create_table.table_name,
           stmt.stmt.create_table.column_count);

    return 0;
}

/*
 * Test: CREATE TABLE error - multiple PRIMARY KEYs
 */
int test_parser_create_multiple_pk_error(void) {
    struct sql_lexer lex;
    struct sql_parser parser;
    struct sql_statement stmt;
    const char *sql = "CREATE TABLE invalid (id INTEGER PRIMARY KEY, pk2 INTEGER PRIMARY KEY)";

    printf("Testing CREATE TABLE error handling (multiple PRIMARY KEYs)...\n");

    lexer_init(&lex, sql);
    parser_init(&parser, &lex);

    /* Should fail */
    if (parser_parse_statement(&parser, &stmt) == 0) {
        printf("  ERROR: Expected parse error for multiple PRIMARY KEYs\n");
        return -1;
    }

    printf("  Correctly rejected: %s\n", parser_get_error(&parser));

    return 0;
}

/*
 * Test: CREATE TABLE error - no columns
 */
int test_parser_create_no_columns_error(void) {
    struct sql_lexer lex;
    struct sql_parser parser;
    struct sql_statement stmt;
    const char *sql = "CREATE TABLE empty ()";

    printf("Testing CREATE TABLE error handling (no columns)...\n");

    lexer_init(&lex, sql);
    parser_init(&parser, &lex);

    /* Should fail */
    if (parser_parse_statement(&parser, &stmt) == 0) {
        printf("  ERROR: Expected parse error for table with no columns\n");
        return -1;
    }

    printf("  Correctly rejected: %s\n", parser_get_error(&parser));

    return 0;
}

/*
 * Test: Parser with trailing semicolon
 */
int test_parser_trailing_semicolon(void) {
    struct sql_lexer lex;
    struct sql_parser parser;
    struct sql_statement stmt;
    const char *sql = "CREATE TABLE test (id INTEGER PRIMARY KEY);";

    printf("Testing CREATE TABLE with trailing semicolon...\n");

    lexer_init(&lex, sql);
    parser_init(&parser, &lex);

    if (parser_parse_statement(&parser, &stmt) != 0) {
        printf("  ERROR: Parse failed: %s\n", parser_get_error(&parser));
        return -1;
    }

    if (stmt.stmt.create_table.column_count != 1) {
        printf("  ERROR: Expected 1 column\n");
        return -1;
    }

    return 0;
}

/*
 * Test: Case insensitivity
 */
int test_parser_case_insensitive(void) {
    struct sql_lexer lex;
    struct sql_parser parser;
    struct sql_statement stmt;
    const char *sql = "create table MixedCase (ID integer primary key, Name text)";

    printf("Testing case insensitivity...\n");

    lexer_init(&lex, sql);
    parser_init(&parser, &lex);

    if (parser_parse_statement(&parser, &stmt) != 0) {
        printf("  ERROR: Parse failed: %s\n", parser_get_error(&parser));
        return -1;
    }

    /* Table names and column names preserve case */
    if (strcmp(stmt.stmt.create_table.table_name, "MixedCase") != 0) {
        printf("  ERROR: Table name case not preserved\n");
        return -1;
    }

    if (strcmp(stmt.stmt.create_table.columns[0].name, "ID") != 0) {
        printf("  ERROR: Column name case not preserved\n");
        return -1;
    }

    return 0;
}
