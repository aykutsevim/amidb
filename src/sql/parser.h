/*
 * parser.h - SQL parser for AmiDB
 *
 * Parses tokenized SQL statements into Abstract Syntax Tree (AST) structures.
 * Uses stack-allocated structures (no malloc) for Amiga 2MB constraint.
 */

#ifndef AMIDB_SQL_PARSER_H
#define AMIDB_SQL_PARSER_H

#include "sql/lexer.h"
#include <stdint.h>

/* Statement types */
#define STMT_CREATE_TABLE   1
#define STMT_DROP_TABLE     2
#define STMT_INSERT         3
#define STMT_SELECT         4
#define STMT_UPDATE         5
#define STMT_DELETE         6
#define STMT_CREATE_INDEX   7
#define STMT_DROP_INDEX     8

/* Data types */
#define SQL_TYPE_INTEGER    1
#define SQL_TYPE_TEXT       2
#define SQL_TYPE_BLOB       3

/* Value types (for INSERT, WHERE clauses) */
#define SQL_VALUE_INTEGER   1
#define SQL_VALUE_TEXT      2
#define SQL_VALUE_BLOB      3
#define SQL_VALUE_NULL      4

/* Comparison operators (for WHERE clause) */
#define SQL_OP_EQ           1  /* = */
#define SQL_OP_NE           2  /* != or <> */
#define SQL_OP_LT           3  /* < */
#define SQL_OP_LE           4  /* <= */
#define SQL_OP_GT           5  /* > */
#define SQL_OP_GE           6  /* >= */

/* Aggregate functions */
#define SQL_AGG_NONE        0  /* No aggregate */
#define SQL_AGG_COUNT       1  /* COUNT(*) or COUNT(column) */
#define SQL_AGG_COUNT_STAR  2  /* COUNT(*) specifically */
#define SQL_AGG_SUM         3  /* SUM(column) */
#define SQL_AGG_AVG         4  /* AVG(column) */
#define SQL_AGG_MIN         5  /* MIN(column) */
#define SQL_AGG_MAX         6  /* MAX(column) */

/* Column definition (for CREATE TABLE) */
struct sql_column_def {
    char name[64];              /* Column name */
    uint8_t type;               /* SQL_TYPE_* */
    uint8_t is_primary_key;     /* 1 if PRIMARY KEY */
    uint8_t not_null;           /* 1 if NOT NULL */
};

/* CREATE TABLE statement */
struct sql_create_table {
    char table_name[64];
    uint8_t column_count;
    struct sql_column_def columns[32];  /* Max 32 columns */
};

/* DROP TABLE statement */
struct sql_drop_table {
    char table_name[64];
};

/* Value (for INSERT, WHERE) */
struct sql_value {
    uint8_t type;               /* SQL_VALUE_* */
    int32_t int_value;
    char text_value[256];
    uint8_t blob_value[256];
    uint16_t blob_length;
};

/* INSERT statement */
struct sql_insert {
    char table_name[64];
    uint8_t value_count;
    struct sql_value values[32];  /* Max 32 values */
};

/* WHERE clause condition (simple: column op value) */
struct sql_where {
    char column_name[64];
    uint8_t op;                 /* SQL_OP_* */
    struct sql_value value;
    uint8_t has_condition;      /* 1 if WHERE clause exists */
};

/* ORDER BY clause */
struct sql_order_by {
    char column_name[64];
    uint8_t ascending;          /* 1 for ASC, 0 for DESC */
    uint8_t has_order;          /* 1 if ORDER BY exists */
};

/* SELECT statement */
struct sql_select {
    char table_name[64];
    uint8_t select_all;         /* 1 for SELECT * */
    uint8_t column_count;       /* Number of specific columns (if not *) */
    char columns[32][64];       /* Column names */
    struct sql_where where;
    struct sql_order_by order_by;
    int32_t limit;              /* -1 if no LIMIT */
    uint8_t aggregate;          /* SQL_AGG_* - aggregate function type */
    char agg_column[64];        /* Column for aggregate (empty for COUNT(*)) */
};

/* UPDATE statement */
struct sql_update {
    char table_name[64];
    char column_name[64];       /* Column to update (simplified: one column) */
    struct sql_value value;     /* New value */
    struct sql_where where;
};

/* DELETE statement */
struct sql_delete {
    char table_name[64];
    struct sql_where where;
};

/* SQL statement (union of all statement types) */
struct sql_statement {
    uint8_t type;               /* STMT_* constant */
    union {
        struct sql_create_table create_table;
        struct sql_drop_table drop_table;
        struct sql_insert insert;
        struct sql_select select;
        struct sql_update update;
        struct sql_delete delete;
    } stmt;
};

/* Parser state */
struct sql_parser {
    struct sql_lexer *lexer;
    struct sql_token current;   /* Current token */
    struct sql_token next;      /* Lookahead token */
    char error_msg[256];        /* Error message */
    uint8_t has_error;          /* 1 if parse error occurred */
};

/* Parser API */

/*
 * Initialize parser with a lexer
 */
void parser_init(struct sql_parser *parser, struct sql_lexer *lexer);

/*
 * Parse a complete SQL statement
 * Returns 0 on success, -1 on error
 */
int parser_parse_statement(struct sql_parser *parser, struct sql_statement *stmt);

/*
 * Get parser error message
 */
const char *parser_get_error(struct sql_parser *parser);

#endif /* AMIDB_SQL_PARSER_H */
