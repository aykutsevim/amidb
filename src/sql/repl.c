/*
 * repl.c - Interactive SQL REPL implementation
 */

#include "sql/repl.h"
#include "sql/lexer.h"
#include "sql/parser.h"
#include "storage/row.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

/* Forward declarations */
static int handle_meta_command(struct sql_repl *repl, const char *command);
static void print_help(void);
static void print_tables(struct sql_executor *exec);
static void print_schema(struct sql_executor *exec, const char *table_name);
static void print_select_results(struct sql_executor *exec);
static void trim_string(char *str);

/*
 * Initialize REPL
 */
int repl_init(struct sql_repl *repl, struct sql_executor *executor) {
    repl->executor = executor;
    repl->quit_requested = 0;
    memset(repl->input_buffer, 0, sizeof(repl->input_buffer));
    return 0;
}

/*
 * Print REPL banner
 */
void repl_print_banner(void) {
    printf("\n");
    printf("================================================\n");
    printf("AmiDB SQL Shell v1.0\n");
    printf("================================================\n");
    printf("AmigaOS 3.1 - 68000 CPU - SQLite-like Database\n");
    printf("\n");
    printf("Type .help for help, .quit to exit\n");
    printf("================================================\n");
    printf("\n");
}

/*
 * Print command prompt
 */
void repl_print_prompt(void) {
    printf("amidb> ");
    fflush(stdout);
}

/*
 * Run REPL main loop
 */
int repl_run(struct sql_repl *repl) {
    repl_print_banner();

    while (!repl->quit_requested) {
        repl_print_prompt();

        /* Read input line */
        if (fgets(repl->input_buffer, sizeof(repl->input_buffer), stdin) == NULL) {
            /* EOF or error */
            printf("\n");
            break;
        }

        /* Remove trailing newline */
        trim_string(repl->input_buffer);

        /* Skip empty lines */
        if (repl->input_buffer[0] == '\0') {
            continue;
        }

        /* Execute command */
        repl_execute_command(repl, repl->input_buffer);
    }

    printf("Goodbye!\n");
    return 0;
}

/*
 * Simple wrapper for SQL parsing
 */
static int sql_parse(const char *sql, struct sql_statement *stmt) {
    struct sql_lexer lexer;
    struct sql_parser parser;
    int rc;

    lexer_init(&lexer, sql);
    parser_init(&parser, &lexer);
    rc = parser_parse_statement(&parser, stmt);

    return rc;
}

/*
 * Execute a single command
 */
int repl_execute_command(struct sql_repl *repl, const char *command) {
    struct sql_statement stmt;
    int rc;

    /* Check for meta-command (starts with .) */
    if (command[0] == '.') {
        return handle_meta_command(repl, command);
    }

    /* Parse SQL statement */
    rc = sql_parse(command, &stmt);
    if (rc != 0) {
        printf("Parse error: Invalid SQL syntax\n");
        return -1;
    }

    /* Execute statement */
    rc = executor_execute(repl->executor, &stmt);
    if (rc != 0) {
        printf("Error: %s\n", executor_get_error(repl->executor));
        return -1;
    }

    /* Handle statement results */
    switch (stmt.type) {
        case STMT_CREATE_TABLE:
            printf("Table created successfully.\n");
            break;

        case STMT_DROP_TABLE:
            printf("Table dropped successfully.\n");
            break;

        case STMT_INSERT:
            printf("Row inserted successfully.\n");
            break;

        case STMT_SELECT:
            print_select_results(repl->executor);
            break;

        case STMT_UPDATE:
            printf("Rows updated successfully.\n");
            break;

        case STMT_DELETE:
            printf("Rows deleted successfully.\n");
            break;

        default:
            printf("Command executed successfully.\n");
            break;
    }

    return 0;
}

/*
 * Handle meta-commands (.help, .quit, etc.)
 */
static int handle_meta_command(struct sql_repl *repl, const char *command) {
    char cmd_name[64];
    char arg[256];
    int n;

    /* Parse meta-command */
    n = sscanf(command, "%63s %255s", cmd_name, arg);
    if (n < 1) {
        return -1;
    }

    /* .help */
    if (strcmp(cmd_name, ".help") == 0) {
        print_help();
        return 0;
    }

    /* .quit or .exit */
    if (strcmp(cmd_name, ".quit") == 0 || strcmp(cmd_name, ".exit") == 0) {
        repl->quit_requested = 1;
        return 1;
    }

    /* .tables */
    if (strcmp(cmd_name, ".tables") == 0) {
        print_tables(repl->executor);
        return 0;
    }

    /* .schema [table_name] */
    if (strcmp(cmd_name, ".schema") == 0) {
        if (n >= 2) {
            print_schema(repl->executor, arg);
        } else {
            printf("Usage: .schema <table_name>\n");
        }
        return 0;
    }

    printf("Unknown meta-command: %s\n", cmd_name);
    printf("Type .help for help\n");
    return -1;
}

/*
 * Print help text
 */
static void print_help(void) {
    printf("\n");
    printf("AmiDB SQL Shell - Help\n");
    printf("======================\n");
    printf("\n");
    printf("Meta-commands:\n");
    printf("  .help              Show this help\n");
    printf("  .quit              Exit the shell\n");
    printf("  .tables            List all tables\n");
    printf("  .schema <table>    Show table schema\n");
    printf("\n");
    printf("SQL commands:\n");
    printf("  CREATE TABLE <name> (columns...)\n");
    printf("  INSERT INTO <table> VALUES (...)\n");
    printf("  SELECT * FROM <table> [WHERE ...] [ORDER BY ...] [LIMIT n]\n");
    printf("  UPDATE <table> SET ... WHERE ...\n");
    printf("  DELETE FROM <table> WHERE ...\n");
    printf("\n");
    printf("Example:\n");
    printf("  CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT);\n");
    printf("  INSERT INTO users VALUES (1, 'Alice');\n");
    printf("  SELECT * FROM users;\n");
    printf("\n");
}

/*
 * Print all tables
 */
static void print_tables(struct sql_executor *exec) {
    char table_names[32][64];
    int count;
    int i;

    count = catalog_list_tables(exec->catalog, table_names, 32);
    if (count == 0) {
        printf("No tables found.\n");
        return;
    }

    printf("\n");
    printf("Tables:\n");
    printf("-------\n");
    for (i = 0; i < count; i++) {
        printf("  %s\n", table_names[i]);
    }
    printf("\n");
}

/*
 * Print table schema
 */
static void print_schema(struct sql_executor *exec, const char *table_name) {
    struct table_schema schema;
    int rc;
    int i;
    const char *type_name;

    rc = catalog_get_table(exec->catalog, table_name, &schema);
    if (rc != 0) {
        printf("Error: Table '%s' not found.\n", table_name);
        return;
    }

    printf("\n");
    printf("Table: %s\n", schema.name);
    printf("=====================================\n");
    printf("Columns:\n");
    for (i = 0; i < schema.column_count; i++) {
        /* Get type name */
        switch (schema.columns[i].type) {
            case SQL_TYPE_INTEGER:
                type_name = "INTEGER";
                break;
            case SQL_TYPE_TEXT:
                type_name = "TEXT";
                break;
            case SQL_TYPE_BLOB:
                type_name = "BLOB";
                break;
            default:
                type_name = "UNKNOWN";
                break;
        }

        printf("  %s %s", schema.columns[i].name, type_name);
        if (schema.columns[i].is_primary_key) {
            printf(" PRIMARY KEY");
        }
        printf("\n");
    }

    if (schema.primary_key_index < 0) {
        printf("\nImplicit rowid: yes (next=%u)\n", schema.next_rowid);
    }
    printf("Row count: %u\n", schema.row_count);
    printf("\n");
}

/*
 * Print SELECT results
 */
static void print_select_results(struct sql_executor *exec) {
    uint32_t row_count = exec->result_count;
    uint32_t col_count;
    uint32_t i, j;
    struct amidb_row *row;
    const struct amidb_value *val;
    char buffer[256];

    if (row_count == 0) {
        printf("No rows returned.\n");
        return;
    }

    /* Get column count from first row */
    row = &exec->result_rows[0];
    col_count = row->column_count;

    printf("\n");

    /* Print rows */
    for (i = 0; i < row_count; i++) {
        row = &exec->result_rows[i];

        printf("Row %u: ", i + 1);
        for (j = 0; j < col_count; j++) {
            val = row_get_value(row, j);
            if (val == NULL) {
                printf("NULL");
            } else {
                switch (val->type) {
                    case AMIDB_TYPE_INTEGER:
                        printf("%d", val->u.i);
                        break;
                    case AMIDB_TYPE_TEXT:
                        if (val->u.blob.data && val->u.blob.size > 0) {
                            snprintf(buffer, sizeof(buffer), "%.*s", (int)val->u.blob.size, (char *)val->u.blob.data);
                            printf("'%s'", buffer);
                        } else {
                            printf("''");
                        }
                        break;
                    case AMIDB_TYPE_BLOB:
                        printf("[BLOB %u bytes]", val->u.blob.size);
                        break;
                    case AMIDB_TYPE_NULL:
                        printf("NULL");
                        break;
                    default:
                        printf("?");
                        break;
                }
            }

            if (j < col_count - 1) {
                printf(", ");
            }
        }
        printf("\n");
    }

    printf("\n%u row%s returned.\n\n", row_count, row_count == 1 ? "" : "s");
}

/*
 * Trim whitespace from string
 */
static void trim_string(char *str) {
    char *end;

    /* Trim leading space */
    while (isspace((unsigned char)*str)) {
        str++;
    }

    if (*str == '\0') {
        return;
    }

    /* Trim trailing space */
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) {
        end--;
    }

    /* Write new null terminator */
    *(end + 1) = '\0';
}
