/*
 * shell_main.c - AmiDB SQL Shell main entry point
 *
 * Interactive SQL shell for AmiDB.
 */

#include "sql/repl.h"
#include "sql/executor.h"
#include "sql/catalog.h"
#include "storage/pager.h"
#include "storage/cache.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

/* Default database file */
#define DEFAULT_DB_FILE "RAM:amidb.db"

/* Forward declarations */
static int execute_sql_file(struct sql_repl *repl, const char *filename);
static void trim_string(char *str);

/*
 * Main entry point
 */
int main(int argc, char **argv) {
    struct amidb_pager *pager = NULL;
    struct page_cache *cache = NULL;
    struct catalog catalog;
    struct sql_executor executor;
    struct sql_repl repl;
    const char *db_file;
    const char *script_file = NULL;
    int rc;

    /* Parse command line arguments */
    if (argc >= 2) {
        db_file = argv[1];
        if (argc >= 3) {
            script_file = argv[2];
        }
    } else {
        db_file = DEFAULT_DB_FILE;
    }

    /* Print usage if requested */
    if (argc >= 2 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        printf("Usage: %s [database_file] [script_file]\n", argv[0]);
        printf("\n");
        printf("Arguments:\n");
        printf("  database_file  Database file path (default: RAM:amidb.db)\n");
        printf("  script_file    Optional SQL script to execute on startup\n");
        printf("\n");
        printf("Examples:\n");
        printf("  %s                        # Use default database\n", argv[0]);
        printf("  %s mydb.db                # Use custom database\n", argv[0]);
        printf("  %s mydb.db showcase.sql   # Execute script on startup\n", argv[0]);
        printf("\n");
        return 0;
    }

    /* Open/create database */
    rc = pager_open(db_file, 0, &pager);
    if (rc != 0) {
        printf("Error: Failed to open database '%s'\n", db_file);
        return 1;
    }

    /* Create page cache (128 pages = 512KB) */
    cache = cache_create(128, pager);
    if (cache == NULL) {
        printf("Error: Failed to create page cache\n");
        pager_close(pager);
        return 1;
    }

    /* Initialize catalog */
    rc = catalog_init(&catalog, pager, cache);
    if (rc != 0) {
        printf("Error: Failed to initialize catalog\n");
        cache_destroy(cache);
        pager_close(pager);
        return 1;
    }

    /* Initialize executor */
    rc = executor_init(&executor, pager, cache, &catalog);
    if (rc != 0) {
        printf("Error: Failed to initialize SQL executor\n");
        catalog_close(&catalog);
        cache_destroy(cache);
        pager_close(pager);
        return 1;
    }

    /* Initialize REPL */
    rc = repl_init(&repl, &executor);
    if (rc != 0) {
        printf("Error: Failed to initialize REPL\n");
        executor_close(&executor);
        catalog_close(&catalog);
        cache_destroy(cache);
        pager_close(pager);
        return 1;
    }

    /* Execute script file if provided */
    if (script_file != NULL) {
        printf("Executing script: %s\n\n", script_file);
        rc = execute_sql_file(&repl, script_file);
        if (rc != 0) {
            printf("\nWarning: Script execution had errors\n");
        } else {
            printf("\nScript executed successfully!\n");
        }

        /* Flush all dirty pages to disk after script execution */
        cache_flush(cache);
        printf("\n");
    }

    /* Run REPL main loop */
    repl_run(&repl);

    /* Cleanup */
    executor_close(&executor);
    catalog_close(&catalog);
    cache_destroy(cache);
    pager_close(pager);

    return 0;
}

/*
 * Execute SQL commands from a file
 */
static int execute_sql_file(struct sql_repl *repl, const char *filename) {
    FILE *file;
    char line[1024];
    char command[4096];
    int command_len = 0;
    int line_num = 0;
    int error_count = 0;

    file = fopen(filename, "r");
    if (file == NULL) {
        printf("Error: Cannot open file '%s'\n", filename);
        return -1;
    }

    command[0] = '\0';

    while (fgets(line, sizeof(line), file) != NULL) {
        line_num++;

        /* Strip inline comments (-- or #) */
        char *comment_pos = strstr(line, "--");
        if (comment_pos != NULL) {
            *comment_pos = '\0';
        }
        comment_pos = strchr(line, '#');
        if (comment_pos != NULL) {
            *comment_pos = '\0';
        }

        trim_string(line);

        /* Skip empty lines */
        if (line[0] == '\0') {
            continue;
        }

        /* Accumulate command */
        if (command_len + strlen(line) + 2 < sizeof(command)) {
            if (command_len > 0) {
                strcat(command, " ");
                command_len++;
            }
            strcat(command, line);
            command_len += strlen(line);
        } else {
            printf("Error: Command too long at line %d\n", line_num);
            error_count++;
            command[0] = '\0';
            command_len = 0;
            continue;
        }

        /* Check if command is complete (ends with semicolon) */
        if (command_len > 0 && command[command_len - 1] == ';') {
            /* Remove trailing semicolon */
            command[command_len - 1] = '\0';

            /* Execute command */
            printf(">> %s\n", command);
            if (repl_execute_command(repl, command) != 0) {
                printf("   [Error at line %d]\n", line_num);
                error_count++;
            }

            /* Flush cache after each statement for memory-constrained systems.
             * This ensures B+Tree splits and schema updates are persisted
             * before the next statement executes.
             */
            cache_flush(repl->executor->cache);

            /* Reset for next command */
            command[0] = '\0';
            command_len = 0;
        }
    }

    /* Check for incomplete command */
    if (command_len > 0) {
        printf("Warning: Incomplete command at end of file: %s\n", command);
        error_count++;
    }

    fclose(file);
    return (error_count > 0) ? -1 : 0;
}

/*
 * Trim whitespace from string
 */
static void trim_string(char *str) {
    char *end;

    /* Trim leading space */
    while (isspace((unsigned char)*str)) {
        memmove(str, str + 1, strlen(str));
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
