/*
 * executor.h - SQL statement executor
 *
 * Executes parsed SQL statements using the catalog and B+Tree infrastructure.
 * Provides ACID guarantees through the transaction manager.
 */

#ifndef AMIDB_SQL_EXECUTOR_H
#define AMIDB_SQL_EXECUTOR_H

#include "sql/parser.h"
#include "sql/catalog.h"
#include "storage/pager.h"
#include "storage/cache.h"
#include "storage/row.h"
#include "txn/txn.h"
#include <stdint.h>

/* Maximum rows to buffer for SELECT results (REPL only) */
#define MAX_RESULT_ROWS 100

/* Executor context */
struct sql_executor {
    struct amidb_pager *pager;
    struct page_cache *cache;
    struct catalog *catalog;
    struct txn_context *txn;        /* Transaction context */
    char error_msg[256];            /* Last error message */
    uint8_t has_error;              /* 1 if error occurred */

    /* SELECT result storage (for REPL display) */
    struct amidb_row result_rows[MAX_RESULT_ROWS];
    uint32_t result_count;          /* Number of rows in result set */
};

/* Executor API */

/*
 * Initialize executor
 * Returns 0 on success, -1 on error
 */
int executor_init(struct sql_executor *exec, struct amidb_pager *pager,
                  struct page_cache *cache, struct catalog *catalog);

/*
 * Close executor
 */
void executor_close(struct sql_executor *exec);

/*
 * Execute a SQL statement
 * Returns 0 on success, -1 on error
 */
int executor_execute(struct sql_executor *exec, const struct sql_statement *stmt);

/*
 * Get last error message
 */
const char *executor_get_error(struct sql_executor *exec);

/*
 * Execute CREATE TABLE statement
 * Week 4: Initial implementation
 */
int executor_create_table(struct sql_executor *exec, const struct sql_create_table *create_stmt);

/*
 * Execute DROP TABLE statement
 * Week 4+: To be implemented
 */
int executor_drop_table(struct sql_executor *exec, const struct sql_drop_table *drop_stmt);

/*
 * Execute INSERT statement
 * Week 5: To be implemented
 */
int executor_insert(struct sql_executor *exec, const struct sql_insert *insert_stmt);

/*
 * Execute SELECT statement
 * Week 6: To be implemented
 */
int executor_select(struct sql_executor *exec, const struct sql_select *select_stmt);

/*
 * Execute UPDATE statement
 * Week 8: To be implemented
 */
int executor_update(struct sql_executor *exec, const struct sql_update *update_stmt);

/*
 * Execute DELETE statement
 * Week 8: To be implemented
 */
int executor_delete(struct sql_executor *exec, const struct sql_delete *delete_stmt);

#endif /* AMIDB_SQL_EXECUTOR_H */
