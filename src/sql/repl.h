/*
 * repl.h - Interactive SQL REPL (Read-Eval-Print Loop)
 *
 * Provides an interactive command-line interface for AmiDB SQL.
 */

#ifndef AMIDB_REPL_H
#define AMIDB_REPL_H

#include "sql/executor.h"

/*
 * REPL state
 */
struct sql_repl {
    struct sql_executor *executor;
    char input_buffer[1024];
    int quit_requested;
};

/*
 * Initialize REPL
 */
int repl_init(struct sql_repl *repl, struct sql_executor *executor);

/*
 * Run REPL main loop
 * Returns 0 on normal exit, -1 on error
 */
int repl_run(struct sql_repl *repl);

/*
 * Execute a single command
 * Returns 0 on success, -1 on error, 1 if quit requested
 */
int repl_execute_command(struct sql_repl *repl, const char *command);

/*
 * Print REPL banner
 */
void repl_print_banner(void);

/*
 * Print command prompt
 */
void repl_print_prompt(void);

#endif /* AMIDB_REPL_H */
