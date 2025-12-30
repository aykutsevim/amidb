/*
 * parser.c - SQL parser implementation
 *
 * Recursive descent parser for SQL statements.
 * Week 2: Implements CREATE TABLE parsing only.
 */

#include "sql/parser.h"
#include <string.h>
#include <stdio.h>

/* Forward declarations */
static void advance(struct sql_parser *parser);
static int match_keyword(struct sql_parser *parser, uint32_t keyword_id);
static int match_symbol(struct sql_parser *parser, uint32_t symbol_id);
static int expect_keyword(struct sql_parser *parser, uint32_t keyword_id);
static int expect_symbol(struct sql_parser *parser, uint32_t symbol_id);
static int expect_identifier(struct sql_parser *parser, char *out_name);
static void set_error(struct sql_parser *parser, const char *message);

static int parse_create_table(struct sql_parser *parser, struct sql_statement *stmt);
static int parse_drop_table(struct sql_parser *parser, struct sql_statement *stmt);
static int parse_column_def(struct sql_parser *parser, struct sql_column_def *col);
static int parse_data_type(struct sql_parser *parser, uint8_t *type);
static int parse_insert(struct sql_parser *parser, struct sql_statement *stmt);
static int parse_value(struct sql_parser *parser, struct sql_value *value);
static int parse_select(struct sql_parser *parser, struct sql_statement *stmt);
static int parse_where(struct sql_parser *parser, struct sql_where *where);

/*
 * Initialize parser
 */
void parser_init(struct sql_parser *parser, struct sql_lexer *lexer) {
    parser->lexer = lexer;
    parser->has_error = 0;
    parser->error_msg[0] = '\0';

    /* Load first two tokens */
    lexer_next(lexer, &parser->current);
    lexer_next(lexer, &parser->next);
}

/*
 * Parse SQL statement
 */
int parser_parse_statement(struct sql_parser *parser, struct sql_statement *stmt) {
    /* Clear statement */
    memset(stmt, 0, sizeof(struct sql_statement));

    /* Check for EOF */
    if (parser->current.type == TOKEN_EOF) {
        set_error(parser, "Unexpected end of input");
        return -1;
    }

    /* Determine statement type by first keyword */
    if (parser->current.type != TOKEN_KEYWORD) {
        set_error(parser, "Expected SQL keyword");
        return -1;
    }

    switch (parser->current.keyword_id) {
        case KW_CREATE:
            return parse_create_table(parser, stmt);

        case KW_DROP:
            return parse_drop_table(parser, stmt);

        case KW_INSERT:
            return parse_insert(parser, stmt);

        case KW_SELECT:
            return parse_select(parser, stmt);

        case KW_UPDATE:
            set_error(parser, "UPDATE not yet implemented");
            return -1;

        case KW_DELETE:
            set_error(parser, "DELETE not yet implemented");
            return -1;

        default:
            set_error(parser, "Unknown SQL statement");
            return -1;
    }
}

/*
 * Parse CREATE TABLE statement
 *
 * Grammar:
 *   CREATE TABLE table_name (
 *     column_name type [PRIMARY KEY],
 *     column_name type,
 *     ...
 *   )
 */
static int parse_create_table(struct sql_parser *parser, struct sql_statement *stmt) {
    struct sql_create_table *create = &stmt->stmt.create_table;
    int primary_key_count = 0;
    int i;

    stmt->type = STMT_CREATE_TABLE;

    /* CREATE */
    if (!expect_keyword(parser, KW_CREATE)) {
        return -1;
    }

    /* TABLE */
    if (!expect_keyword(parser, KW_TABLE)) {
        return -1;
    }

    /* table_name */
    if (!expect_identifier(parser, create->table_name)) {
        return -1;
    }

    /* ( */
    if (!expect_symbol(parser, SYM_LPAREN)) {
        return -1;
    }

    /* Parse column definitions */
    create->column_count = 0;

    while (1) {
        if (create->column_count >= 32) {
            set_error(parser, "Too many columns (max 32)");
            return -1;
        }

        /* Parse column definition */
        if (parse_column_def(parser, &create->columns[create->column_count]) != 0) {
            return -1;
        }

        /* Track PRIMARY KEY */
        if (create->columns[create->column_count].is_primary_key) {
            primary_key_count++;
        }

        create->column_count++;

        /* Check for comma (more columns) or closing paren */
        if (match_symbol(parser, SYM_COMMA)) {
            advance(parser);
            continue;
        } else if (match_symbol(parser, SYM_RPAREN)) {
            advance(parser);
            break;
        } else {
            set_error(parser, "Expected ',' or ')' in column list");
            return -1;
        }
    }

    /* Validate: max one PRIMARY KEY */
    if (primary_key_count > 1) {
        set_error(parser, "Table can have at most one PRIMARY KEY");
        return -1;
    }

    /* Ensure at least one column */
    if (create->column_count == 0) {
        set_error(parser, "Table must have at least one column");
        return -1;
    }

    /* Optional semicolon */
    if (match_symbol(parser, SYM_SEMICOLON)) {
        advance(parser);
    }

    return 0;
}

/*
 * Parse DROP TABLE statement
 *
 * Grammar:
 *   DROP TABLE table_name
 */
static int parse_drop_table(struct sql_parser *parser, struct sql_statement *stmt) {
    struct sql_drop_table *drop = &stmt->stmt.drop_table;

    stmt->type = STMT_DROP_TABLE;

    /* DROP */
    if (!expect_keyword(parser, KW_DROP)) {
        return -1;
    }

    /* TABLE */
    if (!expect_keyword(parser, KW_TABLE)) {
        return -1;
    }

    /* table_name */
    if (!expect_identifier(parser, drop->table_name)) {
        return -1;
    }

    /* Optional semicolon */
    if (match_symbol(parser, SYM_SEMICOLON)) {
        advance(parser);
    }

    return 0;
}

/*
 * Parse column definition
 *
 * Grammar: column_name type [PRIMARY KEY]
 */
static int parse_column_def(struct sql_parser *parser, struct sql_column_def *col) {
    memset(col, 0, sizeof(struct sql_column_def));

    /* column_name */
    if (!expect_identifier(parser, col->name)) {
        return -1;
    }

    /* type */
    if (parse_data_type(parser, &col->type) != 0) {
        return -1;
    }

    /* Check for PRIMARY KEY */
    if (match_keyword(parser, KW_PRIMARY)) {
        advance(parser);
        if (!expect_keyword(parser, KW_KEY)) {
            return -1;
        }
        col->is_primary_key = 1;
    }

    return 0;
}

/*
 * Parse data type
 *
 * Grammar: INTEGER | TEXT | BLOB
 */
static int parse_data_type(struct sql_parser *parser, uint8_t *type) {
    if (parser->current.type != TOKEN_KEYWORD) {
        set_error(parser, "Expected data type (INTEGER, TEXT, or BLOB)");
        return -1;
    }

    switch (parser->current.keyword_id) {
        case KW_INTEGER:
            *type = SQL_TYPE_INTEGER;
            advance(parser);
            return 0;

        case KW_TEXT:
            *type = SQL_TYPE_TEXT;
            advance(parser);
            return 0;

        case KW_BLOB:
            *type = SQL_TYPE_BLOB;
            advance(parser);
            return 0;

        default:
            set_error(parser, "Expected data type (INTEGER, TEXT, or BLOB)");
            return -1;
    }
}

/*
 * Get parser error message
 */
const char *parser_get_error(struct sql_parser *parser) {
    return parser->error_msg;
}

/*
 * Parse INSERT statement
 *
 * Grammar:
 *   INSERT INTO table_name VALUES (value1, value2, ...)
 */
static int parse_insert(struct sql_parser *parser, struct sql_statement *stmt) {
    struct sql_insert *insert = &stmt->stmt.insert;

    stmt->type = STMT_INSERT;

    /* INSERT */
    if (!expect_keyword(parser, KW_INSERT)) {
        return -1;
    }

    /* INTO */
    if (!expect_keyword(parser, KW_INTO)) {
        return -1;
    }

    /* table_name */
    if (!expect_identifier(parser, insert->table_name)) {
        return -1;
    }

    /* VALUES */
    if (!expect_keyword(parser, KW_VALUES)) {
        return -1;
    }

    /* ( */
    if (!expect_symbol(parser, SYM_LPAREN)) {
        return -1;
    }

    /* Parse value list */
    insert->value_count = 0;

    while (1) {
        if (insert->value_count >= 32) {
            set_error(parser, "Too many values (max 32)");
            return -1;
        }

        /* Parse value */
        if (parse_value(parser, &insert->values[insert->value_count]) != 0) {
            return -1;
        }

        insert->value_count++;

        /* Check for comma (more values) or closing paren */
        if (match_symbol(parser, SYM_COMMA)) {
            advance(parser);
            continue;
        } else if (match_symbol(parser, SYM_RPAREN)) {
            advance(parser);
            break;
        } else {
            set_error(parser, "Expected ',' or ')' in value list");
            return -1;
        }
    }

    /* Ensure at least one value */
    if (insert->value_count == 0) {
        set_error(parser, "INSERT must have at least one value");
        return -1;
    }

    /* Optional semicolon */
    if (match_symbol(parser, SYM_SEMICOLON)) {
        advance(parser);
    }

    return 0;
}

/*
 * Parse value (for INSERT VALUES clause)
 *
 * Grammar: integer | 'string' | NULL
 */
static int parse_value(struct sql_parser *parser, struct sql_value *value) {
    memset(value, 0, sizeof(struct sql_value));

    /* Integer */
    if (parser->current.type == TOKEN_INTEGER) {
        value->type = SQL_VALUE_INTEGER;
        value->int_value = parser->current.int_value;
        advance(parser);
        return 0;
    }

    /* String */
    if (parser->current.type == TOKEN_STRING) {
        value->type = SQL_VALUE_TEXT;
        strncpy(value->text_value, parser->current.text, sizeof(value->text_value) - 1);
        value->text_value[sizeof(value->text_value) - 1] = '\0';
        advance(parser);
        return 0;
    }

    /* NULL */
    if (match_keyword(parser, KW_NULL)) {
        value->type = SQL_VALUE_NULL;
        advance(parser);
        return 0;
    }

    set_error(parser, "Expected value (integer, string, or NULL)");
    return -1;
}

/*
 * Parse SELECT statement
 *
 * Grammar:
 *   SELECT * FROM table_name [WHERE column op value]
 */
static int parse_select(struct sql_parser *parser, struct sql_statement *stmt) {
    struct sql_select *select = &stmt->stmt.select;

    stmt->type = STMT_SELECT;
    memset(select, 0, sizeof(struct sql_select));
    select->aggregate = SQL_AGG_NONE;
    select->agg_column[0] = '\0';

    /* SELECT */
    if (!expect_keyword(parser, KW_SELECT)) {
        return -1;
    }

    /* Check for COUNT aggregate function */
    if (match_keyword(parser, KW_COUNT)) {
        advance(parser);

        /* Expect ( */
        if (!match_symbol(parser, SYM_LPAREN)) {
            set_error(parser, "Expected '(' after COUNT");
            return -1;
        }
        advance(parser);

        /* Check for * or column name */
        if (match_symbol(parser, SYM_STAR)) {
            /* COUNT(*) */
            advance(parser);
            select->aggregate = SQL_AGG_COUNT_STAR;
            select->agg_column[0] = '\0';
        } else if (parser->current.type == TOKEN_IDENTIFIER) {
            /* COUNT(column) */
            strncpy(select->agg_column, parser->current.text, 63);
            select->agg_column[63] = '\0';
            advance(parser);
            select->aggregate = SQL_AGG_COUNT;
        } else {
            set_error(parser, "Expected '*' or column name in COUNT()");
            return -1;
        }

        /* Expect ) */
        if (!match_symbol(parser, SYM_RPAREN)) {
            set_error(parser, "Expected ')' after COUNT argument");
            return -1;
        }
        advance(parser);

        select->select_all = 0;
    }
    /* Check for SUM aggregate function */
    else if (match_keyword(parser, KW_SUM)) {
        advance(parser);

        /* Expect ( */
        if (!match_symbol(parser, SYM_LPAREN)) {
            set_error(parser, "Expected '(' after SUM");
            return -1;
        }
        advance(parser);

        /* SUM requires a column name */
        if (parser->current.type != TOKEN_IDENTIFIER) {
            set_error(parser, "Expected column name in SUM()");
            return -1;
        }
        strncpy(select->agg_column, parser->current.text, 63);
        select->agg_column[63] = '\0';
        advance(parser);
        select->aggregate = SQL_AGG_SUM;

        /* Expect ) */
        if (!match_symbol(parser, SYM_RPAREN)) {
            set_error(parser, "Expected ')' after SUM argument");
            return -1;
        }
        advance(parser);

        select->select_all = 0;
    }
    /* Check for AVG aggregate function */
    else if (match_keyword(parser, KW_AVG)) {
        advance(parser);

        /* Expect ( */
        if (!match_symbol(parser, SYM_LPAREN)) {
            set_error(parser, "Expected '(' after AVG");
            return -1;
        }
        advance(parser);

        /* AVG requires a column name */
        if (parser->current.type != TOKEN_IDENTIFIER) {
            set_error(parser, "Expected column name in AVG()");
            return -1;
        }
        strncpy(select->agg_column, parser->current.text, 63);
        select->agg_column[63] = '\0';
        advance(parser);
        select->aggregate = SQL_AGG_AVG;

        /* Expect ) */
        if (!match_symbol(parser, SYM_RPAREN)) {
            set_error(parser, "Expected ')' after AVG argument");
            return -1;
        }
        advance(parser);

        select->select_all = 0;
    }
    /* Check for MIN aggregate function */
    else if (match_keyword(parser, KW_MIN)) {
        advance(parser);

        /* Expect ( */
        if (!match_symbol(parser, SYM_LPAREN)) {
            set_error(parser, "Expected '(' after MIN");
            return -1;
        }
        advance(parser);

        /* MIN requires a column name */
        if (parser->current.type != TOKEN_IDENTIFIER) {
            set_error(parser, "Expected column name in MIN()");
            return -1;
        }
        strncpy(select->agg_column, parser->current.text, 63);
        select->agg_column[63] = '\0';
        advance(parser);
        select->aggregate = SQL_AGG_MIN;

        /* Expect ) */
        if (!match_symbol(parser, SYM_RPAREN)) {
            set_error(parser, "Expected ')' after MIN argument");
            return -1;
        }
        advance(parser);

        select->select_all = 0;
    }
    /* Check for MAX aggregate function */
    else if (match_keyword(parser, KW_MAX)) {
        advance(parser);

        /* Expect ( */
        if (!match_symbol(parser, SYM_LPAREN)) {
            set_error(parser, "Expected '(' after MAX");
            return -1;
        }
        advance(parser);

        /* MAX requires a column name */
        if (parser->current.type != TOKEN_IDENTIFIER) {
            set_error(parser, "Expected column name in MAX()");
            return -1;
        }
        strncpy(select->agg_column, parser->current.text, 63);
        select->agg_column[63] = '\0';
        advance(parser);
        select->aggregate = SQL_AGG_MAX;

        /* Expect ) */
        if (!match_symbol(parser, SYM_RPAREN)) {
            set_error(parser, "Expected ')' after MAX argument");
            return -1;
        }
        advance(parser);

        select->select_all = 0;
    }
    /* * (SELECT * supported) */
    else if (match_symbol(parser, SYM_STAR)) {
        advance(parser);
        select->select_all = 1;
    } else {
        set_error(parser, "Expected '*', COUNT(), SUM(), AVG(), MIN(), or MAX() after SELECT");
        return -1;
    }

    /* FROM */
    if (!expect_keyword(parser, KW_FROM)) {
        return -1;
    }

    /* table_name */
    if (!expect_identifier(parser, select->table_name)) {
        return -1;
    }

    /* Optional WHERE clause */
    if (match_keyword(parser, KW_WHERE)) {
        if (parse_where(parser, &select->where) != 0) {
            return -1;
        }
    } else {
        select->where.has_condition = 0;
    }

    /* Optional ORDER BY clause */
    if (match_keyword(parser, KW_ORDER)) {
        advance(parser);
        if (!expect_keyword(parser, KW_BY)) {
            return -1;
        }

        /* column_name */
        if (!expect_identifier(parser, select->order_by.column_name)) {
            return -1;
        }

        /* Optional ASC/DESC (default is ASC) */
        select->order_by.ascending = 1;  /* Default to ascending */
        if (match_keyword(parser, KW_ASC)) {
            advance(parser);
            select->order_by.ascending = 1;
        } else if (match_keyword(parser, KW_DESC)) {
            advance(parser);
            select->order_by.ascending = 0;
        }

        select->order_by.has_order = 1;
    } else {
        select->order_by.has_order = 0;
    }

    /* Optional LIMIT clause */
    if (match_keyword(parser, KW_LIMIT)) {
        advance(parser);

        /* Expect integer */
        if (parser->current.type != TOKEN_INTEGER) {
            set_error(parser, "LIMIT requires an integer value");
            return -1;
        }

        select->limit = parser->current.int_value;
        advance(parser);

        if (select->limit < 0) {
            set_error(parser, "LIMIT must be non-negative");
            return -1;
        }
    } else {
        select->limit = -1;
    }

    /* Optional semicolon */
    if (match_symbol(parser, SYM_SEMICOLON)) {
        advance(parser);
    }

    return 0;
}

/*
 * Parse WHERE clause
 *
 * Grammar: WHERE column_name op value
 * Where op is: = | != | < | <= | > | >=
 */
static int parse_where(struct sql_parser *parser, struct sql_where *where) {
    memset(where, 0, sizeof(struct sql_where));

    /* WHERE */
    if (!expect_keyword(parser, KW_WHERE)) {
        return -1;
    }

    /* column_name */
    if (!expect_identifier(parser, where->column_name)) {
        return -1;
    }

    /* Comparison operator */
    if (parser->current.type != TOKEN_SYMBOL) {
        set_error(parser, "Expected comparison operator (=, !=, <, <=, >, >=)");
        return -1;
    }

    switch (parser->current.symbol_id) {
        case SYM_EQUAL:
            where->op = SQL_OP_EQ;
            break;
        case SYM_NE:
            where->op = SQL_OP_NE;
            break;
        case SYM_LT:
            where->op = SQL_OP_LT;
            break;
        case SYM_LE:
            where->op = SQL_OP_LE;
            break;
        case SYM_GT:
            where->op = SQL_OP_GT;
            break;
        case SYM_GE:
            where->op = SQL_OP_GE;
            break;
        default:
            set_error(parser, "Invalid comparison operator");
            return -1;
    }
    advance(parser);

    /* value */
    if (parse_value(parser, &where->value) != 0) {
        return -1;
    }

    where->has_condition = 1;
    return 0;
}

/* ========== Helper Functions ========== */

/*
 * Advance to next token
 */
static void advance(struct sql_parser *parser) {
    parser->current = parser->next;
    lexer_next(parser->lexer, &parser->next);
}

/*
 * Check if current token matches keyword (without consuming)
 */
static int match_keyword(struct sql_parser *parser, uint32_t keyword_id) {
    return (parser->current.type == TOKEN_KEYWORD &&
            parser->current.keyword_id == keyword_id);
}

/*
 * Check if current token matches symbol (without consuming)
 */
static int match_symbol(struct sql_parser *parser, uint32_t symbol_id) {
    return (parser->current.type == TOKEN_SYMBOL &&
            parser->current.symbol_id == symbol_id);
}

/*
 * Expect keyword and consume it
 */
static int expect_keyword(struct sql_parser *parser, uint32_t keyword_id) {
    if (!match_keyword(parser, keyword_id)) {
        snprintf(parser->error_msg, sizeof(parser->error_msg),
                 "Expected keyword, got '%s'", parser->current.text);
        parser->has_error = 1;
        return 0;
    }
    advance(parser);
    return 1;
}

/*
 * Expect symbol and consume it
 */
static int expect_symbol(struct sql_parser *parser, uint32_t symbol_id) {
    if (!match_symbol(parser, symbol_id)) {
        snprintf(parser->error_msg, sizeof(parser->error_msg),
                 "Expected symbol, got '%s'", parser->current.text);
        parser->has_error = 1;
        return 0;
    }
    advance(parser);
    return 1;
}

/*
 * Expect identifier and consume it
 */
static int expect_identifier(struct sql_parser *parser, char *out_name) {
    if (parser->current.type != TOKEN_IDENTIFIER) {
        snprintf(parser->error_msg, sizeof(parser->error_msg),
                 "Expected identifier, got '%s'", parser->current.text);
        parser->has_error = 1;
        return 0;
    }

    strncpy(out_name, parser->current.text, 63);
    out_name[63] = '\0';
    advance(parser);
    return 1;
}

/*
 * Set parser error message
 */
static void set_error(struct sql_parser *parser, const char *message) {
    strncpy(parser->error_msg, message, sizeof(parser->error_msg) - 1);
    parser->error_msg[sizeof(parser->error_msg) - 1] = '\0';
    parser->has_error = 1;
}
