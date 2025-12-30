/*
 * executor.c - SQL executor implementation
 *
 * Week 4: CREATE TABLE execution only
 * Week 5: INSERT execution added
 */

#include "sql/executor.h"
#include "storage/row.h"
#include "storage/btree.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* Forward declarations */
static void set_error(struct sql_executor *exec, const char *message);

/* Helper structure for ORDER BY - holds row data for sorting */
struct row_buffer {
    int32_t sort_key_int;        /* Integer sort key */
    char sort_key_text[256];     /* Text sort key */
    uint8_t sort_key_type;       /* AMIDB_TYPE_* */
    struct amidb_row row;        /* Deserialized row */
};

/* Comparison function for qsort (ascending integer) */
static int compare_rows_int_asc(const void *a, const void *b) {
    const struct row_buffer *ra = (const struct row_buffer *)a;
    const struct row_buffer *rb = (const struct row_buffer *)b;
    if (ra->sort_key_int < rb->sort_key_int) return -1;
    if (ra->sort_key_int > rb->sort_key_int) return 1;
    return 0;
}

/* Comparison function for qsort (descending integer) */
static int compare_rows_int_desc(const void *a, const void *b) {
    return compare_rows_int_asc(b, a);
}

/* Comparison function for qsort (ascending text) */
static int compare_rows_text_asc(const void *a, const void *b) {
    const struct row_buffer *ra = (const struct row_buffer *)a;
    const struct row_buffer *rb = (const struct row_buffer *)b;
    return strcmp(ra->sort_key_text, rb->sort_key_text);
}

/* Comparison function for qsort (descending text) */
static int compare_rows_text_desc(const void *a, const void *b) {
    return compare_rows_text_asc(b, a);
}

/*
 * Initialize executor
 */
int executor_init(struct sql_executor *exec, struct amidb_pager *pager,
                  struct page_cache *cache, struct catalog *catalog) {
    exec->pager = pager;
    exec->cache = cache;
    exec->catalog = catalog;
    exec->txn = NULL;  /* Transaction support added in Week 5+ */
    exec->has_error = 0;
    exec->error_msg[0] = '\0';

    return 0;
}

/*
 * Close executor
 */
void executor_close(struct sql_executor *exec) {
    /* Nothing to clean up for now */
    (void)exec;
}

/*
 * Execute SQL statement
 */
int executor_execute(struct sql_executor *exec, const struct sql_statement *stmt) {
    /* Clear previous error */
    exec->has_error = 0;
    exec->error_msg[0] = '\0';

    /* Dispatch to appropriate handler */
    switch (stmt->type) {
        case STMT_CREATE_TABLE:
            return executor_create_table(exec, &stmt->stmt.create_table);

        case STMT_DROP_TABLE:
            return executor_drop_table(exec, &stmt->stmt.drop_table);

        case STMT_INSERT:
            return executor_insert(exec, &stmt->stmt.insert);

        case STMT_SELECT:
            return executor_select(exec, &stmt->stmt.select);

        case STMT_UPDATE:
            return executor_update(exec, &stmt->stmt.update);

        case STMT_DELETE:
            return executor_delete(exec, &stmt->stmt.delete);

        default:
            set_error(exec, "Unknown statement type");
            return -1;
    }
}

/*
 * Get last error message
 */
const char *executor_get_error(struct sql_executor *exec) {
    return exec->error_msg;
}

/*
 * Execute CREATE TABLE
 */
int executor_create_table(struct sql_executor *exec, const struct sql_create_table *create_stmt) {
    int rc;
    int i;
    int pk_count = 0;

    /* Validate table name */
    if (create_stmt->table_name[0] == '\0') {
        set_error(exec, "Table name cannot be empty");
        return -1;
    }

    /* Validate column count */
    if (create_stmt->column_count == 0) {
        set_error(exec, "Table must have at least one column");
        return -1;
    }

    if (create_stmt->column_count > 32) {
        set_error(exec, "Table cannot have more than 32 columns");
        return -1;
    }

    /* Validate columns */
    for (i = 0; i < create_stmt->column_count; i++) {
        /* Check column name */
        if (create_stmt->columns[i].name[0] == '\0') {
            set_error(exec, "Column name cannot be empty");
            return -1;
        }

        /* Check data type */
        if (create_stmt->columns[i].type != SQL_TYPE_INTEGER &&
            create_stmt->columns[i].type != SQL_TYPE_TEXT &&
            create_stmt->columns[i].type != SQL_TYPE_BLOB) {
            set_error(exec, "Invalid column data type");
            return -1;
        }

        /* Count PRIMARY KEYs */
        if (create_stmt->columns[i].is_primary_key) {
            pk_count++;
        }
    }

    /* Validate PRIMARY KEY constraint */
    if (pk_count > 1) {
        set_error(exec, "Table can have at most one PRIMARY KEY");
        return -1;
    }

    /* If PRIMARY KEY exists, it must be INTEGER type */
    if (pk_count == 1) {
        for (i = 0; i < create_stmt->column_count; i++) {
            if (create_stmt->columns[i].is_primary_key) {
                if (create_stmt->columns[i].type != SQL_TYPE_INTEGER) {
                    set_error(exec, "PRIMARY KEY must be INTEGER type");
                    return -1;
                }
                break;
            }
        }
    }

    /* Create table in catalog */
    rc = catalog_create_table(exec->catalog, create_stmt);
    if (rc != 0) {
        snprintf(exec->error_msg, sizeof(exec->error_msg),
                 "Table '%s' already exists", create_stmt->table_name);
        exec->has_error = 1;
        return -1;
    }

    return 0;
}

/*
 * Execute DROP TABLE
 */
int executor_drop_table(struct sql_executor *exec, const struct sql_drop_table *drop_stmt) {
    int rc;

    /* Validate table name */
    if (drop_stmt->table_name[0] == '\0') {
        set_error(exec, "Table name cannot be empty");
        return -1;
    }

    /* Drop table from catalog */
    rc = catalog_drop_table(exec->catalog, drop_stmt->table_name);
    if (rc != 0) {
        snprintf(exec->error_msg, sizeof(exec->error_msg),
                 "Table '%s' does not exist", drop_stmt->table_name);
        exec->has_error = 1;
        return -1;
    }

    return 0;
}

/*
 * Execute INSERT (Week 5)
 */
int executor_insert(struct sql_executor *exec, const struct sql_insert *insert_stmt) {
    static struct table_schema schema;  /* Move off stack (4KB limit) */
    struct amidb_row row;
    struct btree *table_tree;
    static uint8_t row_buffer[4096];  /* Move off stack */
    int32_t primary_key;
    uint32_t row_page;
    int rc;
    int row_size;
    uint32_t i;
    uint8_t *page_data;

    /* Retrieve table schema */
    rc = catalog_get_table(exec->catalog, insert_stmt->table_name, &schema);
    if (rc != 0) {
        snprintf(exec->error_msg, sizeof(exec->error_msg),
                 "Table '%s' does not exist", insert_stmt->table_name);
        exec->has_error = 1;
        return -1;
    }

    /* Validate value count matches column count */
    if (insert_stmt->value_count != schema.column_count) {
        snprintf(exec->error_msg, sizeof(exec->error_msg),
                 "Column count mismatch: expected %u, got %u",
                 schema.column_count, insert_stmt->value_count);
        exec->has_error = 1;
        return -1;
    }

    /* Build row from values */
    row_init(&row);

    for (i = 0; i < insert_stmt->value_count; i++) {
        const struct sql_value *val = &insert_stmt->values[i];
        const struct sql_column_def *col = &schema.columns[i];

        /* Set value based on type */
        switch (val->type) {
            case SQL_VALUE_INTEGER:
                if (col->type != SQL_TYPE_INTEGER) {
                    snprintf(exec->error_msg, sizeof(exec->error_msg),
                             "Type mismatch for column '%s': expected INTEGER", col->name);
                    exec->has_error = 1;
                    row_clear(&row);
                    return -1;
                }
                row_set_int(&row, i, val->int_value);
                break;

            case SQL_VALUE_TEXT:
                if (col->type != SQL_TYPE_TEXT) {
                    snprintf(exec->error_msg, sizeof(exec->error_msg),
                             "Type mismatch for column '%s': expected TEXT", col->name);
                    exec->has_error = 1;
                    row_clear(&row);
                    return -1;
                }
                row_set_text(&row, i, val->text_value, 0);
                break;

            case SQL_VALUE_NULL:
                row_set_null(&row, i);
                break;

            default:
                snprintf(exec->error_msg, sizeof(exec->error_msg),
                         "Unsupported value type for column '%s'", col->name);
                exec->has_error = 1;
                row_clear(&row);
                return -1;
        }
    }

    /* Determine PRIMARY KEY value */
    if (schema.primary_key_index >= 0) {
        /* Explicit PRIMARY KEY */
        const struct amidb_value *pk_val = row_get_value(&row, schema.primary_key_index);
        if (pk_val == NULL || pk_val->type != AMIDB_TYPE_INTEGER) {
            set_error(exec, "PRIMARY KEY must be INTEGER");
            row_clear(&row);
            return -1;
        }
        primary_key = pk_val->u.i;
    } else {
        /* Implicit rowid - use auto-increment */
        primary_key = (int32_t)schema.next_rowid;
    }

    /* Serialize row */
    row_size = row_serialize(&row, row_buffer, sizeof(row_buffer));
    if (row_size < 0) {
        set_error(exec, "Failed to serialize row");
        row_clear(&row);
        return -1;
    }

    /* Allocate page for row data */
    rc = pager_allocate_page(exec->pager, &row_page);
    if (rc != 0) {
        set_error(exec, "Failed to allocate page for row");
        row_clear(&row);
        return -1;
    }

    /* Write serialized row to page */
    rc = cache_get_page(exec->cache, row_page, &page_data);
    if (rc != 0) {
        set_error(exec, "Failed to get page for row");
        row_clear(&row);
        return -1;
    }

    /* Write row data after the 12-byte page header */
    memcpy(page_data + 12, row_buffer, row_size);
    cache_mark_dirty(exec->cache, row_page);
    cache_unpin(exec->cache, row_page);

    /* Open table B+Tree */
    table_tree = btree_open(exec->pager, exec->cache, schema.btree_root);
    if (table_tree == NULL) {
        set_error(exec, "Failed to open table B+Tree");
        row_clear(&row);
        return -1;
    }

    /* Check for duplicate PRIMARY KEY (INSERT should fail on duplicates) */
    {
        uint32_t existing_page;
        rc = btree_search(table_tree, primary_key, &existing_page);
        if (rc == 0) {
            /* Key already exists - reject duplicate PRIMARY KEY */
            snprintf(exec->error_msg, sizeof(exec->error_msg),
                     "Failed to insert row (duplicate PRIMARY KEY: %d)", primary_key);
            exec->has_error = 1;
            btree_close(table_tree);
            row_clear(&row);
            return -1;
        }
    }

    /* Insert into B+Tree: primary_key → row_page */
    rc = btree_insert(table_tree, primary_key, row_page);
    if (rc != 0) {
        set_error(exec, "Failed to insert row");
        btree_close(table_tree);
        row_clear(&row);
        return -1;
    }

    /* CRITICAL: Update schema.btree_root from tree's root_page
     * If btree_insert caused a split, the root may have changed!
     */
    schema.btree_root = table_tree->root_page;

    btree_close(table_tree);

    /* If implicit rowid, update schema.next_rowid */
    if (schema.primary_key_index < 0) {
        schema.next_rowid++;
        schema.row_count++;
        rc = catalog_update_table(exec->catalog, &schema);
        if (rc != 0) {
            set_error(exec, "Failed to update table metadata");
            row_clear(&row);
            return -1;
        }
    } else {
        /* Explicit PRIMARY KEY - just update row count */
        schema.row_count++;
        catalog_update_table(exec->catalog, &schema);
    }

    row_clear(&row);
    return 0;
}

/*
 * Execute SELECT (Week 6)
 */
/*
 * Execute SELECT (Week 6-7: with ORDER BY and LIMIT)
 */
int executor_select(struct sql_executor *exec, const struct sql_select *select_stmt) {
    static struct table_schema schema;  /* Move off stack (4KB limit) */
    struct btree *table_tree;
    struct btree_cursor cursor;
    struct amidb_row row;
    struct row_buffer *row_buffers = NULL;
    uint8_t *page_data;
    uint32_t row_page;
    int rc;
    int match_count = 0;
    int i, j;
    int order_col_idx = -1;
    int need_sorting = 0;
    int row_buffer_count = 0;
    int row_buffer_capacity = 100;  /* Max 100 rows for ORDER BY */

    /* Initialize result storage */
    exec->result_count = 0;
    for (i = 0; i < MAX_RESULT_ROWS; i++) {
        row_init(&exec->result_rows[i]);
    }

    /* Retrieve table schema */
    rc = catalog_get_table(exec->catalog, select_stmt->table_name, &schema);
    if (rc != 0) {
        snprintf(exec->error_msg, sizeof(exec->error_msg),
                 "Table '%s' does not exist", select_stmt->table_name);
        exec->has_error = 1;
        return -1;
    }

    /* Open table B+Tree */
    table_tree = btree_open(exec->pager, exec->cache, schema.btree_root);
    if (table_tree == NULL) {
        set_error(exec, "Failed to open table B+Tree");
        return -1;
    }

    /* Handle COUNT aggregate function */
    if (select_stmt->aggregate == SQL_AGG_COUNT ||
        select_stmt->aggregate == SQL_AGG_COUNT_STAR) {
        int32_t count = 0;
        int agg_col_idx = -1;

        /* For COUNT(column), find the column index */
        if (select_stmt->aggregate == SQL_AGG_COUNT) {
            for (i = 0; i < schema.column_count; i++) {
                if (strcmp(select_stmt->agg_column, schema.columns[i].name) == 0) {
                    agg_col_idx = i;
                    break;
                }
            }
            if (agg_col_idx < 0) {
                snprintf(exec->error_msg, sizeof(exec->error_msg),
                         "Column '%s' not found", select_stmt->agg_column);
                exec->has_error = 1;
                btree_close(table_tree);
                return -1;
            }
        }

        /* Iterate through all rows and count */
        rc = btree_cursor_first(table_tree, &cursor);
        if (rc != 0) {
            /* Empty table - count is 0 */
            count = 0;
        } else {
            while (cursor.valid) {
                row_page = cursor.value;

                rc = cache_get_page(exec->cache, row_page, &page_data);
                if (rc != 0) {
                    btree_cursor_next(&cursor);
                    continue;
                }

                row_init(&row);
                rc = row_deserialize(&row, page_data + 12, 4096 - 12);
                cache_unpin(exec->cache, row_page);

                if (rc < 0) {
                    row_clear(&row);
                    btree_cursor_next(&cursor);
                    continue;
                }

                /* Apply WHERE filter if present */
                int passes_filter = 1;
                if (select_stmt->where.has_condition) {
                    passes_filter = 0;
                    int col_idx = -1;
                    for (i = 0; i < schema.column_count; i++) {
                        if (strcmp(select_stmt->where.column_name, schema.columns[i].name) == 0) {
                            col_idx = i;
                            break;
                        }
                    }

                    if (col_idx >= 0 && col_idx < row.column_count) {
                        const struct amidb_value *col_val = row_get_value(&row, col_idx);

                        if (col_val->type == AMIDB_TYPE_INTEGER &&
                            select_stmt->where.value.type == SQL_VALUE_INTEGER) {
                            int32_t row_val = col_val->u.i;
                            int32_t where_val = select_stmt->where.value.int_value;

                            switch (select_stmt->where.op) {
                                case SQL_OP_EQ: passes_filter = (row_val == where_val); break;
                                case SQL_OP_NE: passes_filter = (row_val != where_val); break;
                                case SQL_OP_LT: passes_filter = (row_val < where_val); break;
                                case SQL_OP_LE: passes_filter = (row_val <= where_val); break;
                                case SQL_OP_GT: passes_filter = (row_val > where_val); break;
                                case SQL_OP_GE: passes_filter = (row_val >= where_val); break;
                            }
                        } else if (col_val->type == AMIDB_TYPE_TEXT &&
                                  select_stmt->where.value.type == SQL_VALUE_TEXT) {
                            char row_str[256];
                            snprintf(row_str, sizeof(row_str), "%.*s",
                                    (int)col_val->u.blob.size, (char*)col_val->u.blob.data);
                            int cmp = strcmp(row_str, select_stmt->where.value.text_value);

                            switch (select_stmt->where.op) {
                                case SQL_OP_EQ: passes_filter = (cmp == 0); break;
                                case SQL_OP_NE: passes_filter = (cmp != 0); break;
                                case SQL_OP_LT: passes_filter = (cmp < 0); break;
                                case SQL_OP_LE: passes_filter = (cmp <= 0); break;
                                case SQL_OP_GT: passes_filter = (cmp > 0); break;
                                case SQL_OP_GE: passes_filter = (cmp >= 0); break;
                            }
                        }
                    }
                }

                if (passes_filter) {
                    /* For COUNT(*) - count all rows */
                    /* For COUNT(column) - count non-NULL values */
                    if (select_stmt->aggregate == SQL_AGG_COUNT_STAR) {
                        count++;
                    } else {
                        /* COUNT(column) - check if column is not NULL */
                        if (agg_col_idx < row.column_count) {
                            const struct amidb_value *val = row_get_value(&row, agg_col_idx);
                            if (val->type != AMIDB_TYPE_NULL) {
                                count++;
                            }
                        }
                    }
                }

                row_clear(&row);
                btree_cursor_next(&cursor);
            }
        }

        btree_close(table_tree);

        /* Create result row with count value */
        row_init(&exec->result_rows[0]);
        row_set_int(&exec->result_rows[0], 0, count);
        exec->result_rows[0].column_count = 1;
        exec->result_count = 1;

        return 0;
    }

    /* Handle SUM aggregate function */
    if (select_stmt->aggregate == SQL_AGG_SUM) {
        int32_t sum = 0;
        int agg_col_idx = -1;

        /* Find the column index */
        for (i = 0; i < schema.column_count; i++) {
            if (strcmp(select_stmt->agg_column, schema.columns[i].name) == 0) {
                agg_col_idx = i;
                break;
            }
        }
        if (agg_col_idx < 0) {
            snprintf(exec->error_msg, sizeof(exec->error_msg),
                     "Column '%s' not found", select_stmt->agg_column);
            exec->has_error = 1;
            btree_close(table_tree);
            return -1;
        }

        /* Verify column is INTEGER type */
        if (schema.columns[agg_col_idx].type != SQL_TYPE_INTEGER) {
            snprintf(exec->error_msg, sizeof(exec->error_msg),
                     "SUM() requires INTEGER column, '%s' is not INTEGER",
                     select_stmt->agg_column);
            exec->has_error = 1;
            btree_close(table_tree);
            return -1;
        }

        /* Iterate through all rows and sum */
        rc = btree_cursor_first(table_tree, &cursor);
        if (rc != 0) {
            /* Empty table - sum is 0 */
            sum = 0;
        } else {
            while (cursor.valid) {
                row_page = cursor.value;

                rc = cache_get_page(exec->cache, row_page, &page_data);
                if (rc != 0) {
                    btree_cursor_next(&cursor);
                    continue;
                }

                row_init(&row);
                rc = row_deserialize(&row, page_data + 12, 4096 - 12);
                cache_unpin(exec->cache, row_page);

                if (rc < 0) {
                    row_clear(&row);
                    btree_cursor_next(&cursor);
                    continue;
                }

                /* Apply WHERE filter if present */
                int passes_filter = 1;
                if (select_stmt->where.has_condition) {
                    passes_filter = 0;
                    int col_idx = -1;
                    for (i = 0; i < schema.column_count; i++) {
                        if (strcmp(select_stmt->where.column_name, schema.columns[i].name) == 0) {
                            col_idx = i;
                            break;
                        }
                    }

                    if (col_idx >= 0 && col_idx < row.column_count) {
                        const struct amidb_value *col_val = row_get_value(&row, col_idx);

                        if (col_val->type == AMIDB_TYPE_INTEGER &&
                            select_stmt->where.value.type == SQL_VALUE_INTEGER) {
                            int32_t row_val = col_val->u.i;
                            int32_t where_val = select_stmt->where.value.int_value;

                            switch (select_stmt->where.op) {
                                case SQL_OP_EQ: passes_filter = (row_val == where_val); break;
                                case SQL_OP_NE: passes_filter = (row_val != where_val); break;
                                case SQL_OP_LT: passes_filter = (row_val < where_val); break;
                                case SQL_OP_LE: passes_filter = (row_val <= where_val); break;
                                case SQL_OP_GT: passes_filter = (row_val > where_val); break;
                                case SQL_OP_GE: passes_filter = (row_val >= where_val); break;
                            }
                        } else if (col_val->type == AMIDB_TYPE_TEXT &&
                                  select_stmt->where.value.type == SQL_VALUE_TEXT) {
                            char row_str[256];
                            snprintf(row_str, sizeof(row_str), "%.*s",
                                    (int)col_val->u.blob.size, (char*)col_val->u.blob.data);
                            int cmp = strcmp(row_str, select_stmt->where.value.text_value);

                            switch (select_stmt->where.op) {
                                case SQL_OP_EQ: passes_filter = (cmp == 0); break;
                                case SQL_OP_NE: passes_filter = (cmp != 0); break;
                                case SQL_OP_LT: passes_filter = (cmp < 0); break;
                                case SQL_OP_LE: passes_filter = (cmp <= 0); break;
                                case SQL_OP_GT: passes_filter = (cmp > 0); break;
                                case SQL_OP_GE: passes_filter = (cmp >= 0); break;
                            }
                        }
                    }
                }

                if (passes_filter) {
                    /* Add value to sum (skip NULL values) */
                    if (agg_col_idx < row.column_count) {
                        const struct amidb_value *val = row_get_value(&row, agg_col_idx);
                        if (val->type == AMIDB_TYPE_INTEGER) {
                            sum += val->u.i;
                        }
                    }
                }

                row_clear(&row);
                btree_cursor_next(&cursor);
            }
        }

        btree_close(table_tree);

        /* Create result row with sum value */
        row_init(&exec->result_rows[0]);
        row_set_int(&exec->result_rows[0], 0, sum);
        exec->result_rows[0].column_count = 1;
        exec->result_count = 1;

        return 0;
    }

    /* Handle AVG aggregate function */
    if (select_stmt->aggregate == SQL_AGG_AVG) {
        int32_t sum = 0;
        int32_t count = 0;
        int agg_col_idx = -1;

        /* Find the column index */
        for (i = 0; i < schema.column_count; i++) {
            if (strcmp(select_stmt->agg_column, schema.columns[i].name) == 0) {
                agg_col_idx = i;
                break;
            }
        }
        if (agg_col_idx < 0) {
            snprintf(exec->error_msg, sizeof(exec->error_msg),
                     "Column '%s' not found", select_stmt->agg_column);
            exec->has_error = 1;
            btree_close(table_tree);
            return -1;
        }

        /* Verify column is INTEGER type */
        if (schema.columns[agg_col_idx].type != SQL_TYPE_INTEGER) {
            snprintf(exec->error_msg, sizeof(exec->error_msg),
                     "AVG() requires INTEGER column, '%s' is not INTEGER",
                     select_stmt->agg_column);
            exec->has_error = 1;
            btree_close(table_tree);
            return -1;
        }

        /* Iterate through all rows and calculate sum and count */
        rc = btree_cursor_first(table_tree, &cursor);
        if (rc != 0) {
            /* Empty table - avg is 0 */
            sum = 0;
            count = 0;
        } else {
            while (cursor.valid) {
                row_page = cursor.value;

                rc = cache_get_page(exec->cache, row_page, &page_data);
                if (rc != 0) {
                    btree_cursor_next(&cursor);
                    continue;
                }

                row_init(&row);
                rc = row_deserialize(&row, page_data + 12, 4096 - 12);
                cache_unpin(exec->cache, row_page);

                if (rc < 0) {
                    row_clear(&row);
                    btree_cursor_next(&cursor);
                    continue;
                }

                /* Apply WHERE filter if present */
                int passes_filter = 1;
                if (select_stmt->where.has_condition) {
                    passes_filter = 0;
                    int col_idx = -1;
                    for (i = 0; i < schema.column_count; i++) {
                        if (strcmp(select_stmt->where.column_name, schema.columns[i].name) == 0) {
                            col_idx = i;
                            break;
                        }
                    }

                    if (col_idx >= 0 && col_idx < row.column_count) {
                        const struct amidb_value *col_val = row_get_value(&row, col_idx);

                        if (col_val->type == AMIDB_TYPE_INTEGER &&
                            select_stmt->where.value.type == SQL_VALUE_INTEGER) {
                            int32_t row_val = col_val->u.i;
                            int32_t where_val = select_stmt->where.value.int_value;

                            switch (select_stmt->where.op) {
                                case SQL_OP_EQ: passes_filter = (row_val == where_val); break;
                                case SQL_OP_NE: passes_filter = (row_val != where_val); break;
                                case SQL_OP_LT: passes_filter = (row_val < where_val); break;
                                case SQL_OP_LE: passes_filter = (row_val <= where_val); break;
                                case SQL_OP_GT: passes_filter = (row_val > where_val); break;
                                case SQL_OP_GE: passes_filter = (row_val >= where_val); break;
                            }
                        } else if (col_val->type == AMIDB_TYPE_TEXT &&
                                  select_stmt->where.value.type == SQL_VALUE_TEXT) {
                            char row_str[256];
                            snprintf(row_str, sizeof(row_str), "%.*s",
                                    (int)col_val->u.blob.size, (char*)col_val->u.blob.data);
                            int cmp = strcmp(row_str, select_stmt->where.value.text_value);

                            switch (select_stmt->where.op) {
                                case SQL_OP_EQ: passes_filter = (cmp == 0); break;
                                case SQL_OP_NE: passes_filter = (cmp != 0); break;
                                case SQL_OP_LT: passes_filter = (cmp < 0); break;
                                case SQL_OP_LE: passes_filter = (cmp <= 0); break;
                                case SQL_OP_GT: passes_filter = (cmp > 0); break;
                                case SQL_OP_GE: passes_filter = (cmp >= 0); break;
                            }
                        }
                    }
                }

                if (passes_filter) {
                    /* Add value to sum and increment count (skip NULL values) */
                    if (agg_col_idx < row.column_count) {
                        const struct amidb_value *val = row_get_value(&row, agg_col_idx);
                        if (val->type == AMIDB_TYPE_INTEGER) {
                            sum += val->u.i;
                            count++;
                        }
                    }
                }

                row_clear(&row);
                btree_cursor_next(&cursor);
            }
        }

        btree_close(table_tree);

        /* Create result row with avg value (integer division) */
        row_init(&exec->result_rows[0]);
        if (count > 0) {
            row_set_int(&exec->result_rows[0], 0, sum / count);
        } else {
            row_set_int(&exec->result_rows[0], 0, 0);
        }
        exec->result_rows[0].column_count = 1;
        exec->result_count = 1;

        return 0;
    }

    /* Handle MIN aggregate function */
    if (select_stmt->aggregate == SQL_AGG_MIN) {
        int32_t min_val = 0;
        int found_any = 0;
        int agg_col_idx = -1;

        /* Find the column index */
        for (i = 0; i < schema.column_count; i++) {
            if (strcmp(select_stmt->agg_column, schema.columns[i].name) == 0) {
                agg_col_idx = i;
                break;
            }
        }
        if (agg_col_idx < 0) {
            snprintf(exec->error_msg, sizeof(exec->error_msg),
                     "Column '%s' not found", select_stmt->agg_column);
            exec->has_error = 1;
            btree_close(table_tree);
            return -1;
        }

        /* Verify column is INTEGER type */
        if (schema.columns[agg_col_idx].type != SQL_TYPE_INTEGER) {
            snprintf(exec->error_msg, sizeof(exec->error_msg),
                     "MIN() requires INTEGER column, '%s' is not INTEGER",
                     select_stmt->agg_column);
            exec->has_error = 1;
            btree_close(table_tree);
            return -1;
        }

        /* Iterate through all rows and find minimum */
        rc = btree_cursor_first(table_tree, &cursor);
        if (rc != 0) {
            /* Empty table - min is 0 */
            min_val = 0;
            found_any = 0;
        } else {
            while (cursor.valid) {
                row_page = cursor.value;

                rc = cache_get_page(exec->cache, row_page, &page_data);
                if (rc != 0) {
                    btree_cursor_next(&cursor);
                    continue;
                }

                row_init(&row);
                rc = row_deserialize(&row, page_data + 12, 4096 - 12);
                cache_unpin(exec->cache, row_page);

                if (rc < 0) {
                    row_clear(&row);
                    btree_cursor_next(&cursor);
                    continue;
                }

                /* Apply WHERE filter if present */
                int passes_filter = 1;
                if (select_stmt->where.has_condition) {
                    passes_filter = 0;
                    int col_idx = -1;
                    for (i = 0; i < schema.column_count; i++) {
                        if (strcmp(select_stmt->where.column_name, schema.columns[i].name) == 0) {
                            col_idx = i;
                            break;
                        }
                    }

                    if (col_idx >= 0 && col_idx < row.column_count) {
                        const struct amidb_value *col_val = row_get_value(&row, col_idx);

                        if (col_val->type == AMIDB_TYPE_INTEGER &&
                            select_stmt->where.value.type == SQL_VALUE_INTEGER) {
                            int32_t row_val = col_val->u.i;
                            int32_t where_val = select_stmt->where.value.int_value;

                            switch (select_stmt->where.op) {
                                case SQL_OP_EQ: passes_filter = (row_val == where_val); break;
                                case SQL_OP_NE: passes_filter = (row_val != where_val); break;
                                case SQL_OP_LT: passes_filter = (row_val < where_val); break;
                                case SQL_OP_LE: passes_filter = (row_val <= where_val); break;
                                case SQL_OP_GT: passes_filter = (row_val > where_val); break;
                                case SQL_OP_GE: passes_filter = (row_val >= where_val); break;
                            }
                        } else if (col_val->type == AMIDB_TYPE_TEXT &&
                                  select_stmt->where.value.type == SQL_VALUE_TEXT) {
                            char row_str[256];
                            snprintf(row_str, sizeof(row_str), "%.*s",
                                    (int)col_val->u.blob.size, (char*)col_val->u.blob.data);
                            int cmp = strcmp(row_str, select_stmt->where.value.text_value);

                            switch (select_stmt->where.op) {
                                case SQL_OP_EQ: passes_filter = (cmp == 0); break;
                                case SQL_OP_NE: passes_filter = (cmp != 0); break;
                                case SQL_OP_LT: passes_filter = (cmp < 0); break;
                                case SQL_OP_LE: passes_filter = (cmp <= 0); break;
                                case SQL_OP_GT: passes_filter = (cmp > 0); break;
                                case SQL_OP_GE: passes_filter = (cmp >= 0); break;
                            }
                        }
                    }
                }

                if (passes_filter) {
                    /* Check value and update minimum (skip NULL values) */
                    if (agg_col_idx < row.column_count) {
                        const struct amidb_value *val = row_get_value(&row, agg_col_idx);
                        if (val->type == AMIDB_TYPE_INTEGER) {
                            if (!found_any || val->u.i < min_val) {
                                min_val = val->u.i;
                                found_any = 1;
                            }
                        }
                    }
                }

                row_clear(&row);
                btree_cursor_next(&cursor);
            }
        }

        btree_close(table_tree);

        /* Create result row with min value */
        row_init(&exec->result_rows[0]);
        if (found_any) {
            row_set_int(&exec->result_rows[0], 0, min_val);
        } else {
            row_set_int(&exec->result_rows[0], 0, 0);
        }
        exec->result_rows[0].column_count = 1;
        exec->result_count = 1;

        return 0;
    }

    /* Handle MAX aggregate function */
    if (select_stmt->aggregate == SQL_AGG_MAX) {
        int32_t max_val = 0;
        int found_any = 0;
        int agg_col_idx = -1;

        /* Find the column index */
        for (i = 0; i < schema.column_count; i++) {
            if (strcmp(select_stmt->agg_column, schema.columns[i].name) == 0) {
                agg_col_idx = i;
                break;
            }
        }
        if (agg_col_idx < 0) {
            snprintf(exec->error_msg, sizeof(exec->error_msg),
                     "Column '%s' not found", select_stmt->agg_column);
            exec->has_error = 1;
            btree_close(table_tree);
            return -1;
        }

        /* Verify column is INTEGER type */
        if (schema.columns[agg_col_idx].type != SQL_TYPE_INTEGER) {
            snprintf(exec->error_msg, sizeof(exec->error_msg),
                     "MAX() requires INTEGER column, '%s' is not INTEGER",
                     select_stmt->agg_column);
            exec->has_error = 1;
            btree_close(table_tree);
            return -1;
        }

        /* Iterate through all rows and find maximum */
        rc = btree_cursor_first(table_tree, &cursor);
        if (rc != 0) {
            /* Empty table - max is 0 */
            max_val = 0;
            found_any = 0;
        } else {
            while (cursor.valid) {
                row_page = cursor.value;

                rc = cache_get_page(exec->cache, row_page, &page_data);
                if (rc != 0) {
                    btree_cursor_next(&cursor);
                    continue;
                }

                row_init(&row);
                rc = row_deserialize(&row, page_data + 12, 4096 - 12);
                cache_unpin(exec->cache, row_page);

                if (rc < 0) {
                    row_clear(&row);
                    btree_cursor_next(&cursor);
                    continue;
                }

                /* Apply WHERE filter if present */
                int passes_filter = 1;
                if (select_stmt->where.has_condition) {
                    passes_filter = 0;
                    int col_idx = -1;
                    for (i = 0; i < schema.column_count; i++) {
                        if (strcmp(select_stmt->where.column_name, schema.columns[i].name) == 0) {
                            col_idx = i;
                            break;
                        }
                    }

                    if (col_idx >= 0 && col_idx < row.column_count) {
                        const struct amidb_value *col_val = row_get_value(&row, col_idx);

                        if (col_val->type == AMIDB_TYPE_INTEGER &&
                            select_stmt->where.value.type == SQL_VALUE_INTEGER) {
                            int32_t row_val = col_val->u.i;
                            int32_t where_val = select_stmt->where.value.int_value;

                            switch (select_stmt->where.op) {
                                case SQL_OP_EQ: passes_filter = (row_val == where_val); break;
                                case SQL_OP_NE: passes_filter = (row_val != where_val); break;
                                case SQL_OP_LT: passes_filter = (row_val < where_val); break;
                                case SQL_OP_LE: passes_filter = (row_val <= where_val); break;
                                case SQL_OP_GT: passes_filter = (row_val > where_val); break;
                                case SQL_OP_GE: passes_filter = (row_val >= where_val); break;
                            }
                        } else if (col_val->type == AMIDB_TYPE_TEXT &&
                                  select_stmt->where.value.type == SQL_VALUE_TEXT) {
                            char row_str[256];
                            snprintf(row_str, sizeof(row_str), "%.*s",
                                    (int)col_val->u.blob.size, (char*)col_val->u.blob.data);
                            int cmp = strcmp(row_str, select_stmt->where.value.text_value);

                            switch (select_stmt->where.op) {
                                case SQL_OP_EQ: passes_filter = (cmp == 0); break;
                                case SQL_OP_NE: passes_filter = (cmp != 0); break;
                                case SQL_OP_LT: passes_filter = (cmp < 0); break;
                                case SQL_OP_LE: passes_filter = (cmp <= 0); break;
                                case SQL_OP_GT: passes_filter = (cmp > 0); break;
                                case SQL_OP_GE: passes_filter = (cmp >= 0); break;
                            }
                        }
                    }
                }

                if (passes_filter) {
                    /* Check value and update maximum (skip NULL values) */
                    if (agg_col_idx < row.column_count) {
                        const struct amidb_value *val = row_get_value(&row, agg_col_idx);
                        if (val->type == AMIDB_TYPE_INTEGER) {
                            if (!found_any || val->u.i > max_val) {
                                max_val = val->u.i;
                                found_any = 1;
                            }
                        }
                    }
                }

                row_clear(&row);
                btree_cursor_next(&cursor);
            }
        }

        btree_close(table_tree);

        /* Create result row with max value */
        row_init(&exec->result_rows[0]);
        if (found_any) {
            row_set_int(&exec->result_rows[0], 0, max_val);
        } else {
            row_set_int(&exec->result_rows[0], 0, 0);
        }
        exec->result_rows[0].column_count = 1;
        exec->result_count = 1;

        return 0;
    }

    /* Determine if we need in-memory sorting */
    if (select_stmt->order_by.has_order) {
        /* Find ORDER BY column index */
        for (i = 0; i < schema.column_count; i++) {
            if (strcmp(select_stmt->order_by.column_name, schema.columns[i].name) == 0) {
                order_col_idx = i;
                break;
            }
        }

        if (order_col_idx < 0) {
            snprintf(exec->error_msg, sizeof(exec->error_msg),
                     "ORDER BY column '%s' not found", select_stmt->order_by.column_name);
            exec->has_error = 1;
            btree_close(table_tree);
            return -1;
        }

        /* Check if we need sorting (not if ordering by PK ascending) */
        int is_pk_order = (schema.primary_key_index >= 0 && order_col_idx == schema.primary_key_index);
        need_sorting = !(is_pk_order && select_stmt->order_by.ascending);

        if (need_sorting) {
            /* Allocate row buffer for sorting */
            row_buffers = (struct row_buffer *)malloc(row_buffer_capacity * sizeof(struct row_buffer));
            if (row_buffers == NULL) {
                set_error(exec, "Out of memory for ORDER BY");
                btree_close(table_tree);
                return -1;
            }
            memset(row_buffers, 0, row_buffer_capacity * sizeof(struct row_buffer));
        }
    }

    /* WHERE clause fast path: Direct PRIMARY KEY lookup */
    if (select_stmt->where.has_condition && !need_sorting) {
        int is_pk_where = 0;
        if (schema.primary_key_index >= 0) {
            if (strcmp(select_stmt->where.column_name,
                      schema.columns[schema.primary_key_index].name) == 0) {
                is_pk_where = 1;
            }
        }

        if (is_pk_where && select_stmt->where.op == SQL_OP_EQ) {
            /* Fast path: Direct B+Tree search */
            if (select_stmt->where.value.type != SQL_VALUE_INTEGER) {
                set_error(exec, "WHERE on PRIMARY KEY requires INTEGER value");
                btree_close(table_tree);
                if (row_buffers) free(row_buffers);
                return -1;
            }

            rc = btree_search(table_tree, select_stmt->where.value.int_value, &row_page);
            if (rc == 0) {
                rc = cache_get_page(exec->cache, row_page, &page_data);
                if (rc == 0) {
                    row_init(&exec->result_rows[0]);
                    row_deserialize(&exec->result_rows[0], page_data + 12, 4096 - 12);
                    exec->result_count = 1;
                    cache_unpin(exec->cache, row_page);
                }
            }

            btree_close(table_tree);
            return 0;
        }
    }

    /* Collect rows (with WHERE filtering if present) */
    rc = btree_cursor_first(table_tree, &cursor);
    if (rc != 0) {
        /* Empty table */
        btree_close(table_tree);
        if (row_buffers) free(row_buffers);
        exec->result_count = 0;
        return 0;
    }

    while (cursor.valid) {
        row_page = cursor.value;

        rc = cache_get_page(exec->cache, row_page, &page_data);
        if (rc != 0) {
            cache_unpin(exec->cache, row_page);
            btree_cursor_next(&cursor);
            continue;
        }

        row_init(&row);
        rc = row_deserialize(&row, page_data + 12, 4096 - 12);
        cache_unpin(exec->cache, row_page);

        if (rc < 0) {
            row_clear(&row);
            btree_cursor_next(&cursor);
            continue;
        }

        /* Apply WHERE filter */
        int passes_filter = 1;
        if (select_stmt->where.has_condition) {
            passes_filter = 0;
            int col_idx = -1;
            for (i = 0; i < schema.column_count; i++) {
                if (strcmp(select_stmt->where.column_name, schema.columns[i].name) == 0) {
                    col_idx = i;
                    break;
                }
            }

            if (col_idx >= 0 && col_idx < row.column_count) {
                const struct amidb_value *col_val = row_get_value(&row, col_idx);

                if (col_val->type == AMIDB_TYPE_INTEGER &&
                    select_stmt->where.value.type == SQL_VALUE_INTEGER) {
                    int32_t row_val = col_val->u.i;
                    int32_t where_val = select_stmt->where.value.int_value;

                    switch (select_stmt->where.op) {
                        case SQL_OP_EQ: passes_filter = (row_val == where_val); break;
                        case SQL_OP_NE: passes_filter = (row_val != where_val); break;
                        case SQL_OP_LT: passes_filter = (row_val < where_val); break;
                        case SQL_OP_LE: passes_filter = (row_val <= where_val); break;
                        case SQL_OP_GT: passes_filter = (row_val > where_val); break;
                        case SQL_OP_GE: passes_filter = (row_val >= where_val); break;
                    }
                } else if (col_val->type == AMIDB_TYPE_TEXT &&
                          select_stmt->where.value.type == SQL_VALUE_TEXT) {
                    char row_str[256];
                    snprintf(row_str, sizeof(row_str), "%.*s",
                            (int)col_val->u.blob.size, (char*)col_val->u.blob.data);
                    int cmp = strcmp(row_str, select_stmt->where.value.text_value);

                    switch (select_stmt->where.op) {
                        case SQL_OP_EQ: passes_filter = (cmp == 0); break;
                        case SQL_OP_NE: passes_filter = (cmp != 0); break;
                        case SQL_OP_LT: passes_filter = (cmp < 0); break;
                        case SQL_OP_LE: passes_filter = (cmp <= 0); break;
                        case SQL_OP_GT: passes_filter = (cmp > 0); break;
                        case SQL_OP_GE: passes_filter = (cmp >= 0); break;
                    }
                }
            }
        }

        if (!passes_filter) {
            row_clear(&row);
            btree_cursor_next(&cursor);
            continue;
        }

        /* If we need sorting, buffer the row */
        if (need_sorting) {
            if (row_buffer_count >= row_buffer_capacity) {
                set_error(exec, "Too many rows for ORDER BY (max 100)");
                for (j = 0; j < row_buffer_count; j++) {
                    row_clear(&row_buffers[j].row);
                }
                free(row_buffers);
                row_clear(&row);
                btree_close(table_tree);
                return -1;
            }

            /* Extract sort key */
            const struct amidb_value *sort_val = row_get_value(&row, order_col_idx);
            row_buffers[row_buffer_count].sort_key_type = sort_val->type;

            if (sort_val->type == AMIDB_TYPE_INTEGER) {
                row_buffers[row_buffer_count].sort_key_int = sort_val->u.i;
            } else if (sort_val->type == AMIDB_TYPE_TEXT) {
                snprintf(row_buffers[row_buffer_count].sort_key_text,
                        sizeof(row_buffers[row_buffer_count].sort_key_text),
                        "%.*s", (int)sort_val->u.blob.size, (char*)sort_val->u.blob.data);
            }

            /* Deep copy the row */
            row_buffers[row_buffer_count].row = row;
            row_buffer_count++;
            /* Don't row_clear here - we're keeping the row */
        } else {
            /* No sorting - store row in result set with LIMIT */
            if (exec->result_count < MAX_RESULT_ROWS) {
                /* Deep copy the row to result set */
                exec->result_rows[exec->result_count] = row;
                exec->result_count++;
                match_count++;
                /* Don't row_clear here - we're keeping the row */
            } else {
                row_clear(&row);
            }

            /* Check LIMIT */
            if (select_stmt->limit > 0 && match_count >= select_stmt->limit) {
                break;
            }
        }

        btree_cursor_next(&cursor);
    }

    btree_close(table_tree);

    /* If we buffered rows, sort and output them */
    if (need_sorting && row_buffer_count > 0) {
        /* Sort the rows */
        if (row_buffers[0].sort_key_type == AMIDB_TYPE_INTEGER) {
            if (select_stmt->order_by.ascending) {
                qsort(row_buffers, row_buffer_count, sizeof(struct row_buffer), compare_rows_int_asc);
            } else {
                qsort(row_buffers, row_buffer_count, sizeof(struct row_buffer), compare_rows_int_desc);
            }
        } else if (row_buffers[0].sort_key_type == AMIDB_TYPE_TEXT) {
            if (select_stmt->order_by.ascending) {
                qsort(row_buffers, row_buffer_count, sizeof(struct row_buffer), compare_rows_text_asc);
            } else {
                qsort(row_buffers, row_buffer_count, sizeof(struct row_buffer), compare_rows_text_desc);
            }
        }

        /* Copy sorted rows to result set with LIMIT */
        int output_limit = row_buffer_count;
        if (select_stmt->limit > 0 && select_stmt->limit < row_buffer_count) {
            output_limit = select_stmt->limit;
        }
        if (output_limit > MAX_RESULT_ROWS) {
            output_limit = MAX_RESULT_ROWS;
        }

        for (j = 0; j < output_limit; j++) {
            exec->result_rows[j] = row_buffers[j].row;
            /* Don't row_clear - we're transferring ownership */
        }
        exec->result_count = output_limit;

        /* Clean up remaining row buffers that weren't transferred */
        for (j = output_limit; j < row_buffer_count; j++) {
            row_clear(&row_buffers[j].row);
        }
        free(row_buffers);
    }

    return 0;
}

/*
 * Execute UPDATE
 */
int executor_update(struct sql_executor *exec, const struct sql_update *update_stmt) {
    static struct table_schema schema;  /* Move off stack (4KB limit) */
    struct btree *table_tree;
    struct btree_cursor cursor;
    struct amidb_row row;
    static uint8_t row_buffer[4096];  /* Move off stack */
    uint8_t *page_data;
    uint32_t row_page;
    int update_count = 0;
    int update_col_idx = -1;
    int rc;
    int i;
    int row_size;

    /* Retrieve table schema */
    rc = catalog_get_table(exec->catalog, update_stmt->table_name, &schema);
    if (rc != 0) {
        snprintf(exec->error_msg, sizeof(exec->error_msg),
                 "Table '%s' does not exist", update_stmt->table_name);
        exec->has_error = 1;
        return -1;
    }

    /* Find column to update */
    for (i = 0; i < schema.column_count; i++) {
        if (strcmp(update_stmt->column_name, schema.columns[i].name) == 0) {
            update_col_idx = i;
            break;
        }
    }

    if (update_col_idx < 0) {
        snprintf(exec->error_msg, sizeof(exec->error_msg),
                 "Column '%s' not found in table '%s'",
                 update_stmt->column_name, update_stmt->table_name);
        exec->has_error = 1;
        return -1;
    }

    /* Validate value type matches column type */
    if (update_stmt->value.type == SQL_VALUE_INTEGER &&
        schema.columns[update_col_idx].type != SQL_TYPE_INTEGER) {
        set_error(exec, "Type mismatch: expected INTEGER");
        return -1;
    }
    if (update_stmt->value.type == SQL_VALUE_TEXT &&
        schema.columns[update_col_idx].type != SQL_TYPE_TEXT) {
        set_error(exec, "Type mismatch: expected TEXT");
        return -1;
    }

    /* Check if updating PRIMARY KEY (not allowed for simplicity) */
    if (update_col_idx == schema.primary_key_index) {
        set_error(exec, "Cannot update PRIMARY KEY column");
        return -1;
    }

    /* Open table B+Tree */
    table_tree = btree_open(exec->pager, exec->cache, schema.btree_root);
    if (table_tree == NULL) {
        set_error(exec, "Failed to open table B+Tree");
        return -1;
    }

    /* WHERE clause fast path: Direct PRIMARY KEY lookup */
    if (update_stmt->where.has_condition) {
        int is_pk_where = 0;
        if (schema.primary_key_index >= 0) {
            if (strcmp(update_stmt->where.column_name,
                      schema.columns[schema.primary_key_index].name) == 0) {
                is_pk_where = 1;
            }
        }

        if (is_pk_where && update_stmt->where.op == SQL_OP_EQ) {
            /* Fast path: Direct B+Tree search and update */
            if (update_stmt->where.value.type != SQL_VALUE_INTEGER) {
                set_error(exec, "WHERE on PRIMARY KEY requires INTEGER value");
                btree_close(table_tree);
                return -1;
            }

            rc = btree_search(table_tree, update_stmt->where.value.int_value, &row_page);
            if (rc == 0) {
                rc = cache_get_page(exec->cache, row_page, &page_data);
                if (rc == 0) {
                    row_init(&row);
                    row_deserialize(&row, page_data + 12, 4096 - 12);

                    /* Update the column value */
                    if (update_stmt->value.type == SQL_VALUE_INTEGER) {
                        row_set_int(&row, update_col_idx, update_stmt->value.int_value);
                    } else if (update_stmt->value.type == SQL_VALUE_TEXT) {
                        row_set_text(&row, update_col_idx, update_stmt->value.text_value, 0);
                    }

                    /* Serialize and write back */
                    row_size = row_serialize(&row, row_buffer, sizeof(row_buffer));
                    if (row_size > 0) {
                        memcpy(page_data + 12, row_buffer, row_size);
                        cache_mark_dirty(exec->cache, row_page);
                        update_count = 1;
                    }

                    row_clear(&row);
                    cache_unpin(exec->cache, row_page);
                }
            }

            btree_close(table_tree);
            return 0;
        }
    }

    /* General case: Iterate through all rows */
    rc = btree_cursor_first(table_tree, &cursor);
    if (rc != 0) {
        /* Empty table */
        btree_close(table_tree);
        return 0;
    }

    while (cursor.valid) {
        row_page = cursor.value;

        rc = cache_get_page(exec->cache, row_page, &page_data);
        if (rc != 0) {
            cache_unpin(exec->cache, row_page);
            btree_cursor_next(&cursor);
            continue;
        }

        row_init(&row);
        rc = row_deserialize(&row, page_data + 12, 4096 - 12);

        if (rc < 0) {
            row_clear(&row);
            cache_unpin(exec->cache, row_page);
            btree_cursor_next(&cursor);
            continue;
        }

        /* Apply WHERE filter if present */
        int should_update = 1;
        if (update_stmt->where.has_condition) {
            should_update = 0;
            int col_idx = -1;
            for (i = 0; i < schema.column_count; i++) {
                if (strcmp(update_stmt->where.column_name, schema.columns[i].name) == 0) {
                    col_idx = i;
                    break;
                }
            }

            if (col_idx >= 0 && col_idx < row.column_count) {
                const struct amidb_value *col_val = row_get_value(&row, col_idx);

                if (col_val->type == AMIDB_TYPE_INTEGER &&
                    update_stmt->where.value.type == SQL_VALUE_INTEGER) {
                    int32_t row_val = col_val->u.i;
                    int32_t where_val = update_stmt->where.value.int_value;

                    switch (update_stmt->where.op) {
                        case SQL_OP_EQ: should_update = (row_val == where_val); break;
                        case SQL_OP_NE: should_update = (row_val != where_val); break;
                        case SQL_OP_LT: should_update = (row_val < where_val); break;
                        case SQL_OP_LE: should_update = (row_val <= where_val); break;
                        case SQL_OP_GT: should_update = (row_val > where_val); break;
                        case SQL_OP_GE: should_update = (row_val >= where_val); break;
                    }
                } else if (col_val->type == AMIDB_TYPE_TEXT &&
                          update_stmt->where.value.type == SQL_VALUE_TEXT) {
                    char row_str[256];
                    snprintf(row_str, sizeof(row_str), "%.*s",
                            (int)col_val->u.blob.size, (char*)col_val->u.blob.data);
                    int cmp = strcmp(row_str, update_stmt->where.value.text_value);

                    switch (update_stmt->where.op) {
                        case SQL_OP_EQ: should_update = (cmp == 0); break;
                        case SQL_OP_NE: should_update = (cmp != 0); break;
                        case SQL_OP_LT: should_update = (cmp < 0); break;
                        case SQL_OP_LE: should_update = (cmp <= 0); break;
                        case SQL_OP_GT: should_update = (cmp > 0); break;
                        case SQL_OP_GE: should_update = (cmp >= 0); break;
                    }
                }
            }
        }

        if (should_update) {
            /* Update the column value */
            if (update_stmt->value.type == SQL_VALUE_INTEGER) {
                row_set_int(&row, update_col_idx, update_stmt->value.int_value);
            } else if (update_stmt->value.type == SQL_VALUE_TEXT) {
                row_set_text(&row, update_col_idx, update_stmt->value.text_value, 0);
            }

            /* Serialize and write back */
            row_size = row_serialize(&row, row_buffer, sizeof(row_buffer));
            if (row_size > 0) {
                memcpy(page_data + 12, row_buffer, row_size);
                cache_mark_dirty(exec->cache, row_page);
                update_count++;
            }
        }

        row_clear(&row);
        cache_unpin(exec->cache, row_page);
        btree_cursor_next(&cursor);
    }

    btree_close(table_tree);
    return 0;
}

/*
 * Execute DELETE
 */
int executor_delete(struct sql_executor *exec, const struct sql_delete *delete_stmt) {
    static struct table_schema schema;  /* Move off stack (4KB limit) */
    struct btree *table_tree;
    struct btree_cursor cursor;
    struct amidb_row row;
    uint8_t *page_data;
    uint32_t row_page;
    int32_t *keys_to_delete = NULL;
    int delete_count = 0;
    int delete_capacity = 100;
    int rc;
    int i, j;

    /* Retrieve table schema */
    rc = catalog_get_table(exec->catalog, delete_stmt->table_name, &schema);
    if (rc != 0) {
        snprintf(exec->error_msg, sizeof(exec->error_msg),
                 "Table '%s' does not exist", delete_stmt->table_name);
        exec->has_error = 1;
        return -1;
    }

    /* Open table B+Tree */
    table_tree = btree_open(exec->pager, exec->cache, schema.btree_root);
    if (table_tree == NULL) {
        set_error(exec, "Failed to open table B+Tree");
        return -1;
    }

    /* Allocate buffer for keys to delete */
    keys_to_delete = (int32_t *)malloc(delete_capacity * sizeof(int32_t));
    if (keys_to_delete == NULL) {
        set_error(exec, "Out of memory for DELETE");
        btree_close(table_tree);
        return -1;
    }

    /* WHERE clause fast path: Direct PRIMARY KEY lookup */
    if (delete_stmt->where.has_condition) {
        int is_pk_where = 0;
        if (schema.primary_key_index >= 0) {
            if (strcmp(delete_stmt->where.column_name,
                      schema.columns[schema.primary_key_index].name) == 0) {
                is_pk_where = 1;
            }
        }

        if (is_pk_where && delete_stmt->where.op == SQL_OP_EQ) {
            /* Fast path: Direct B+Tree delete */
            if (delete_stmt->where.value.type != SQL_VALUE_INTEGER) {
                set_error(exec, "WHERE on PRIMARY KEY requires INTEGER value");
                btree_close(table_tree);
                free(keys_to_delete);
                return -1;
            }

            rc = btree_delete(table_tree, delete_stmt->where.value.int_value);
            if (rc == 0) {
                delete_count = 1;
                schema.row_count--;
            }

            btree_close(table_tree);
            free(keys_to_delete);

            /* Update catalog */
            if (delete_count > 0) {
                catalog_update_table(exec->catalog, &schema);
            }

            return 0;
        }
    }

    /* Collect keys to delete (can't delete during iteration) */
    rc = btree_cursor_first(table_tree, &cursor);
    if (rc != 0) {
        /* Empty table */
        btree_close(table_tree);
        free(keys_to_delete);
        return 0;
    }

    while (cursor.valid) {
        row_page = cursor.value;
        int32_t current_key = cursor.key;

        rc = cache_get_page(exec->cache, row_page, &page_data);
        if (rc != 0) {
            cache_unpin(exec->cache, row_page);
            btree_cursor_next(&cursor);
            continue;
        }

        row_init(&row);
        rc = row_deserialize(&row, page_data + 12, 4096 - 12);
        cache_unpin(exec->cache, row_page);

        if (rc < 0) {
            row_clear(&row);
            btree_cursor_next(&cursor);
            continue;
        }

        /* Apply WHERE filter if present */
        int should_delete = 1;
        if (delete_stmt->where.has_condition) {
            should_delete = 0;
            int col_idx = -1;
            for (i = 0; i < schema.column_count; i++) {
                if (strcmp(delete_stmt->where.column_name, schema.columns[i].name) == 0) {
                    col_idx = i;
                    break;
                }
            }

            if (col_idx >= 0 && col_idx < row.column_count) {
                const struct amidb_value *col_val = row_get_value(&row, col_idx);

                if (col_val->type == AMIDB_TYPE_INTEGER &&
                    delete_stmt->where.value.type == SQL_VALUE_INTEGER) {
                    int32_t row_val = col_val->u.i;
                    int32_t where_val = delete_stmt->where.value.int_value;

                    switch (delete_stmt->where.op) {
                        case SQL_OP_EQ: should_delete = (row_val == where_val); break;
                        case SQL_OP_NE: should_delete = (row_val != where_val); break;
                        case SQL_OP_LT: should_delete = (row_val < where_val); break;
                        case SQL_OP_LE: should_delete = (row_val <= where_val); break;
                        case SQL_OP_GT: should_delete = (row_val > where_val); break;
                        case SQL_OP_GE: should_delete = (row_val >= where_val); break;
                    }
                } else if (col_val->type == AMIDB_TYPE_TEXT &&
                          delete_stmt->where.value.type == SQL_VALUE_TEXT) {
                    char row_str[256];
                    snprintf(row_str, sizeof(row_str), "%.*s",
                            (int)col_val->u.blob.size, (char*)col_val->u.blob.data);
                    int cmp = strcmp(row_str, delete_stmt->where.value.text_value);

                    switch (delete_stmt->where.op) {
                        case SQL_OP_EQ: should_delete = (cmp == 0); break;
                        case SQL_OP_NE: should_delete = (cmp != 0); break;
                        case SQL_OP_LT: should_delete = (cmp < 0); break;
                        case SQL_OP_LE: should_delete = (cmp <= 0); break;
                        case SQL_OP_GT: should_delete = (cmp > 0); break;
                        case SQL_OP_GE: should_delete = (cmp >= 0); break;
                    }
                }
            }
        }

        if (should_delete) {
            if (delete_count >= delete_capacity) {
                set_error(exec, "Too many rows to delete (max 100)");
                for (j = 0; j < delete_count; j++) {
                    /* Clean up what we can */
                }
                row_clear(&row);
                free(keys_to_delete);
                btree_close(table_tree);
                return -1;
            }
            keys_to_delete[delete_count++] = current_key;
        }

        row_clear(&row);
        btree_cursor_next(&cursor);
    }

    /* Now delete all marked keys */
    for (i = 0; i < delete_count; i++) {
        btree_delete(table_tree, keys_to_delete[i]);
        schema.row_count--;
    }

    btree_close(table_tree);
    free(keys_to_delete);

    /* Update catalog */
    if (delete_count > 0) {
        catalog_update_table(exec->catalog, &schema);
    }

    return 0;
}

/* ========== Helper Functions ========== */

/*
 * Set executor error message
 */
static void set_error(struct sql_executor *exec, const char *message) {
    strncpy(exec->error_msg, message, sizeof(exec->error_msg) - 1);
    exec->error_msg[sizeof(exec->error_msg) - 1] = '\0';
    exec->has_error = 1;
}
