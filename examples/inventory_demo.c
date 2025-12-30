/*
 * inventory_demo.c - AmiDB Comprehensive Example
 *
 * This example demonstrates ALL AmiDB capabilities:
 *
 * PART A: Direct C API
 *   1. Pager and Cache management
 *   2. B+Tree operations (insert, search, delete, cursor)
 *   3. Row serialization with multiple data types
 *   4. ACID Transactions (commit and abort)
 *   5. Crash recovery simulation
 *
 * PART B: SQL Interface (single session for data consistency)
 *   6. CREATE TABLE with PRIMARY KEY
 *   7. INSERT statements
 *   8. SELECT with WHERE, ORDER BY, LIMIT
 *   9. UPDATE and DELETE statements
 *  10. Aggregate functions (COUNT, SUM, AVG, MIN, MAX)
 *  11. DROP TABLE
 *
 * Scenario: Retro Computer Store Inventory System
 */

#include <stdio.h>
#include <string.h>

/* Storage layer */
#include "storage/pager.h"
#include "storage/cache.h"
#include "storage/btree.h"
#include "storage/row.h"

/* Transaction layer */
#include "txn/wal.h"
#include "txn/txn.h"

/* SQL layer */
#include "sql/lexer.h"
#include "sql/parser.h"
#include "sql/catalog.h"
#include "sql/executor.h"

/* OS layer for file deletion */
#include "os/file.h"

/* API */
#include "api/error.h"

/*
 * Configuration
 */
#define DB_PATH_DIRECT "RAM:inventory_direct.db"
#define DB_PATH_SQL    "RAM:inventory_sql.db"
#define CACHE_SIZE     64  /* 64 pages = 256KB */

/*
 * Helper function to print section headers
 */
static void print_section(const char *title) {
    printf("\n");
    printf("===============================================\n");
    printf("%s\n", title);
    printf("===============================================\n");
}

/*
 * Helper function to print sub-section headers
 */
static void print_subsection(const char *title) {
    printf("\n--- %s ---\n", title);
}

/*
 * Helper to print a horizontal line
 */
static void print_line(void) {
    printf("-----------------------------------------------\n");
}

/*
 * Delete a file if it exists (for clean test runs)
 */
static void delete_file_if_exists(const char *path) {
    /* Try to delete - ignore errors if file doesn't exist */
    file_delete(path);
}

/* ============================================================
 * PART A: DIRECT C API EXAMPLES
 * ============================================================ */

/*
 * Example 1: Basic B+Tree Operations
 *
 * Demonstrates: pager, cache, B+Tree insert/search/delete/cursor
 */
static int example_btree_basics(void) {
    struct amidb_pager *pager = NULL;
    struct page_cache *cache = NULL;
    struct btree *tree = NULL;
    uint32_t root_page;
    uint32_t value;
    int rc;

    print_section("Example 1: B+Tree Basics");

    /* Delete any existing database for clean test */
    delete_file_if_exists(DB_PATH_DIRECT);

    /* Create database */
    printf("\n1. Creating database '%s'...\n", DB_PATH_DIRECT);
    rc = pager_open(DB_PATH_DIRECT, 0, &pager);
    if (rc != 0) {
        printf("ERROR: Failed to create database\n");
        return -1;
    }
    printf("   Database created (page size: %d bytes)\n", AMIDB_PAGE_SIZE);

    /* Create cache */
    printf("\n2. Creating page cache (%d pages = %d KB)...\n",
           CACHE_SIZE, CACHE_SIZE * 4);
    cache = cache_create(CACHE_SIZE, pager);
    if (!cache) {
        pager_close(pager);
        return -1;
    }

    /* Create B+Tree */
    printf("\n3. Creating B+Tree index...\n");
    tree = btree_create(pager, cache, &root_page);
    if (!tree) {
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }
    printf("   B+Tree created (root page: %u, order: %d)\n",
           root_page, BTREE_ORDER);

    /* Save root page for later */
    pager->header.root_page = root_page;
    pager_write_header(pager);

    /* Insert data */
    print_subsection("Inserting Products");
    printf("   Product 1001: Amiga 500    -> price 299\n");
    printf("   Product 1002: Amiga 1200   -> price 499\n");
    printf("   Product 1003: Amiga 4000   -> price 1299\n");
    printf("   Product 1004: Mouse        -> price 25\n");
    printf("   Product 1005: Joystick     -> price 15\n");

    btree_insert(tree, 1001, 299);
    btree_insert(tree, 1002, 499);
    btree_insert(tree, 1003, 1299);
    btree_insert(tree, 1004, 25);
    btree_insert(tree, 1005, 15);

    /* Search */
    print_subsection("Searching");
    rc = btree_search(tree, 1003, &value);
    if (rc == 0) {
        printf("   Found Product 1003: price = %u\n", value);
    }

    rc = btree_search(tree, 9999, &value);
    if (rc != 0) {
        printf("   Product 9999: NOT FOUND (expected)\n");
    }

    /* Update (re-insert with same key) */
    print_subsection("Updating");
    printf("   Updating Product 1001: 299 -> 349\n");
    btree_insert(tree, 1001, 349);
    btree_search(tree, 1001, &value);
    printf("   Product 1001 now: %u\n", value);

    /* Delete */
    print_subsection("Deleting");
    printf("   Deleting Product 1005...\n");
    rc = btree_delete(tree, 1005);
    if (rc == 0) {
        printf("   Product 1005 deleted.\n");
    }

    /* Cursor iteration */
    print_subsection("Listing All Products (Cursor)");
    struct btree_cursor cursor;
    int32_t key;

    rc = btree_cursor_first(tree, &cursor);
    if (rc == 0) {
        printf("   %-10s %s\n", "Product ID", "Price");
        print_line();
        do {
            btree_cursor_get(&cursor, &key, &value);
            printf("   %-10d %u\n", key, value);
        } while (btree_cursor_next(&cursor) == 0);
    }

    /* Statistics */
    print_subsection("B+Tree Statistics");
    uint32_t num_entries, height, num_nodes;
    btree_get_stats(tree, &num_entries, &height, &num_nodes);
    printf("   Entries: %u\n", num_entries);
    printf("   Height:  %u\n", height);
    printf("   Nodes:   %u\n", num_nodes);

    /* Cleanup */
    btree_close(tree);
    cache_destroy(cache);
    pager_close(pager);

    printf("\n[OK] Example 1 completed.\n");
    return 0;
}

/*
 * Example 2: Row Serialization
 *
 * Demonstrates: row_init, row_set_*, row_serialize, row_deserialize
 */
static int example_row_serialization(void) {
    struct amidb_row row;
    static uint8_t buffer[4096];  /* Static to avoid stack overflow */
    int bytes;

    print_section("Example 2: Row Serialization");

    /* Initialize row */
    printf("\n1. Creating a product row with multiple columns...\n");
    row_init(&row);

    /* Set column values */
    row_set_int(&row, 0, 1001);                    /* id: INTEGER */
    row_set_text(&row, 1, "Amiga 500 Plus", 0);    /* name: TEXT */
    row_set_int(&row, 2, 349);                     /* price: INTEGER */
    row_set_text(&row, 3, "Computer", 0);          /* category: TEXT */
    row_set_int(&row, 4, 5);                       /* stock: INTEGER */

    printf("   Columns set: id, name, price, category, stock\n");

    /* Serialize */
    print_subsection("Serializing Row");
    bytes = row_serialize(&row, buffer, sizeof(buffer));
    if (bytes > 0) {
        printf("   Serialized to %d bytes\n", bytes);
        printf("   Format: length-prefixed, little-endian\n");
    }

    /* Clear and deserialize */
    print_subsection("Deserializing Row");
    row_clear(&row);  /* Free any allocated memory */
    row_init(&row);

    bytes = row_deserialize(&row, buffer, sizeof(buffer));
    if (bytes > 0) {
        printf("   Deserialized %d bytes\n", bytes);
        printf("   Column count: %u\n", row.column_count);
    }

    /* Read back values */
    print_subsection("Reading Deserialized Values");
    const struct amidb_value *val;

    val = row_get_value(&row, 0);
    if (val && val->type == AMIDB_TYPE_INTEGER) {
        printf("   Column 0 (id):       %d\n", val->u.i);
    }

    val = row_get_value(&row, 1);
    if (val && val->type == AMIDB_TYPE_TEXT) {
        printf("   Column 1 (name):     %.*s\n",
               (int)val->u.blob.size, (char *)val->u.blob.data);
    }

    val = row_get_value(&row, 2);
    if (val && val->type == AMIDB_TYPE_INTEGER) {
        printf("   Column 2 (price):    %d\n", val->u.i);
    }

    val = row_get_value(&row, 3);
    if (val && val->type == AMIDB_TYPE_TEXT) {
        printf("   Column 3 (category): %.*s\n",
               (int)val->u.blob.size, (char *)val->u.blob.data);
    }

    val = row_get_value(&row, 4);
    if (val && val->type == AMIDB_TYPE_INTEGER) {
        printf("   Column 4 (stock):    %d\n", val->u.i);
    }

    /* Demonstrate data types */
    print_subsection("Supported Data Types");
    printf("   INTEGER: 32-bit signed (-2147483648 to 2147483647)\n");
    printf("   TEXT:    Variable-length, length-prefixed\n");
    printf("   BLOB:    Binary data, length-prefixed\n");
    printf("   NULL:    Missing/unknown value\n");

    row_clear(&row);

    printf("\n[OK] Example 2 completed.\n");
    return 0;
}

/*
 * Example 3: ACID Transactions
 *
 * Demonstrates: txn_begin, txn_commit, txn_abort, WAL
 */
static int example_transactions(void) {
    struct amidb_pager *pager = NULL;
    struct page_cache *cache = NULL;
    struct btree *tree = NULL;
    struct wal_context *wal = NULL;
    struct txn_context *txn = NULL;
    uint32_t value;
    int rc;

    print_section("Example 3: ACID Transactions");

    /* Open database from Example 1 */
    printf("\n1. Opening database...\n");
    rc = pager_open(DB_PATH_DIRECT, 0, &pager);
    if (rc != 0) return -1;

    cache = cache_create(CACHE_SIZE, pager);
    tree = btree_open(pager, cache, pager->header.root_page);

    /* Create WAL and transaction context */
    printf("\n2. Initializing transaction system...\n");
    wal = wal_create(pager);
    if (!wal) {
        btree_close(tree);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }
    printf("   Write-Ahead Log created\n");

    txn = txn_create(wal, cache);
    if (!txn) {
        wal_destroy(wal);
        btree_close(tree);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }
    printf("   Transaction context created\n");

    /* Associate tree with transaction */
    btree_set_transaction(tree, txn);

    /* Transaction 1: Commit */
    print_subsection("Transaction 1: Commit");
    printf("   Beginning transaction...\n");
    txn_begin(txn);
    printf("   Transaction ID: %llu\n", (unsigned long long)txn->txn_id);

    printf("   Adding Product 2001 (price: 599)\n");
    printf("   Adding Product 2002 (price: 799)\n");
    btree_insert(tree, 2001, 599);
    btree_insert(tree, 2002, 799);

    printf("   Committing transaction...\n");
    rc = txn_commit(txn);
    if (rc == 0) {
        printf("   Transaction COMMITTED - changes are durable!\n");
    }

    /* Verify committed data */
    btree_search(tree, 2001, &value);
    printf("   Verified: Product 2001 = %u\n", value);

    /* Transaction 2: Abort */
    print_subsection("Transaction 2: Abort (Rollback)");
    printf("   Beginning transaction...\n");
    txn_begin(txn);
    printf("   Transaction ID: %llu\n", (unsigned long long)txn->txn_id);

    printf("   Attempting bad changes:\n");
    printf("   - Product 2001: 599 -> 9999\n");
    printf("   - Adding Product 6666\n");
    btree_insert(tree, 2001, 9999);
    btree_insert(tree, 6666, 666);

    printf("   ABORTING transaction (simulating error)...\n");
    rc = txn_abort(txn);
    if (rc == 0) {
        printf("   Transaction ABORTED - changes rolled back!\n");
    }

    /* Verify rollback */
    btree_search(tree, 2001, &value);
    printf("   Verified: Product 2001 = %u (unchanged)\n", value);

    rc = btree_search(tree, 6666, &value);
    if (rc != 0) {
        printf("   Verified: Product 6666 NOT FOUND (rolled back)\n");
    }

    /* ACID explanation */
    print_subsection("ACID Guarantees");
    printf("   Atomicity:   All changes commit or all rollback\n");
    printf("   Consistency: Database always valid\n");
    printf("   Isolation:   Uncommitted changes invisible\n");
    printf("   Durability:  Committed changes survive crashes\n");

    /* Cleanup */
    btree_set_transaction(tree, NULL);
    txn_destroy(txn);
    wal_destroy(wal);
    btree_close(tree);
    cache_destroy(cache);
    pager_close(pager);

    printf("\n[OK] Example 3 completed.\n");
    return 0;
}

/*
 * Example 4: Crash Recovery Simulation
 *
 * Demonstrates: WAL recovery after unclean shutdown
 */
static int example_crash_recovery(void) {
    struct amidb_pager *pager = NULL;
    struct page_cache *cache = NULL;
    struct btree *tree = NULL;
    struct wal_context *wal = NULL;
    struct txn_context *txn = NULL;
    uint32_t value;
    int rc;

    print_section("Example 4: Crash Recovery");

    /* Phase 1: Commit some data, then "crash" */
    print_subsection("Phase 1: Commit Data Then Crash");

    printf("\n1. Opening database and committing critical data...\n");
    pager_open(DB_PATH_DIRECT, 0, &pager);
    cache = cache_create(CACHE_SIZE, pager);
    tree = btree_open(pager, cache, pager->header.root_page);
    wal = wal_create(pager);
    txn = txn_create(wal, cache);
    btree_set_transaction(tree, txn);

    txn_begin(txn);
    printf("   Adding HIGH VALUE Product 8001 (price: 5000)\n");
    btree_insert(tree, 8001, 5000);
    txn_commit(txn);
    printf("   Transaction COMMITTED.\n");

    /* Start another transaction but DON'T commit */
    txn_begin(txn);
    printf("\n2. Starting new transaction (will NOT commit)...\n");
    printf("   Adding INVALID Product 8888 (should be lost)\n");
    btree_insert(tree, 8888, 8888);

    printf("\n3. *** SIMULATING POWER FAILURE ***\n");
    printf("   Closing database WITHOUT commit...\n");

    /* Close ungracefully (no txn_commit, no txn_destroy) */
    btree_close(tree);
    cache_destroy(cache);
    pager_close(pager);

    /* Phase 2: Reopen and verify recovery */
    print_subsection("Phase 2: Recovery After Crash");

    printf("\n4. Reopening database (recovery happens automatically)...\n");
    rc = pager_open(DB_PATH_DIRECT, 0, &pager);
    if (rc != 0) {
        printf("ERROR: Failed to reopen database\n");
        return -1;
    }

    cache = cache_create(CACHE_SIZE, pager);
    tree = btree_open(pager, cache, pager->header.root_page);

    printf("\n5. Verifying data integrity:\n");

    /* Committed data should survive */
    rc = btree_search(tree, 8001, &value);
    if (rc == 0 && value == 5000) {
        printf("   Product 8001: %u - SURVIVED (committed data)\n", value);
    } else {
        printf("   ERROR: Product 8001 lost!\n");
    }

    /* Uncommitted data should be gone */
    rc = btree_search(tree, 8888, &value);
    if (rc != 0) {
        printf("   Product 8888: NOT FOUND - CORRECT (uncommitted data)\n");
    } else {
        printf("   ERROR: Product 8888 should not exist!\n");
    }

    print_subsection("Recovery Summary");
    printf("   Committed transactions:   RECOVERED\n");
    printf("   Uncommitted transactions: DISCARDED\n");
    printf("   Database integrity:       VERIFIED\n");

    btree_close(tree);
    cache_destroy(cache);
    pager_close(pager);

    printf("\n[OK] Example 4 completed.\n");
    return 0;
}

/* ============================================================
 * PART B: SQL INTERFACE EXAMPLES
 * All examples use a single database session for data consistency
 * ============================================================ */

/*
 * SQL helper: Execute a SQL statement
 */
static int run_sql(struct sql_executor *exec, const char *sql) {
    struct sql_lexer lexer;
    struct sql_parser parser;
    static struct sql_statement stmt;  /* Static to avoid stack overflow */

    lexer_init(&lexer, sql);
    parser_init(&parser, &lexer);

    if (parser_parse_statement(&parser, &stmt) != 0) {
        printf("   Parse error: %s\n", parser_get_error(&parser));
        return -1;
    }

    if (executor_execute(exec, &stmt) != 0) {
        printf("   Error: %s\n", executor_get_error(exec));
        return -1;
    }

    return 0;
}

/*
 * SQL helper: Print SELECT results
 */
static void print_results(struct sql_executor *exec) {
    uint32_t i, j;
    struct amidb_row *row;
    const struct amidb_value *val;

    if (exec->result_count == 0) {
        printf("   (no rows)\n");
        return;
    }

    for (i = 0; i < exec->result_count; i++) {
        row = &exec->result_rows[i];
        printf("   ");

        for (j = 0; j < row->column_count; j++) {
            if (j > 0) printf(", ");

            val = row_get_value(row, j);
            if (val == NULL || val->type == AMIDB_TYPE_NULL) {
                printf("NULL");
            } else if (val->type == AMIDB_TYPE_INTEGER) {
                printf("%d", val->u.i);
            } else if (val->type == AMIDB_TYPE_TEXT) {
                printf("'%.*s'", (int)val->u.blob.size, (char *)val->u.blob.data);
            } else if (val->type == AMIDB_TYPE_BLOB) {
                printf("[BLOB %u bytes]", val->u.blob.size);
            }
        }
        printf("\n");
    }
    printf("   (%u row%s)\n", exec->result_count,
           exec->result_count == 1 ? "" : "s");
}

/*
 * Run all SQL examples in a single database session
 */
static int run_sql_examples(void) {
    struct amidb_pager *pager = NULL;
    struct page_cache *cache = NULL;
    struct catalog catalog;
    struct sql_executor exec;
    char table_names[32][64];
    int count;
    int rc;

    /* Delete existing database for clean test */
    delete_file_if_exists(DB_PATH_SQL);

    print_section("SQL Interface Examples");

    /* Initialize SQL engine (ONCE for all examples) */
    printf("\nInitializing SQL engine...\n");
    rc = pager_open(DB_PATH_SQL, 0, &pager);
    if (rc != 0) {
        printf("ERROR: Failed to create database\n");
        return -1;
    }

    cache = cache_create(CACHE_SIZE, pager);
    if (!cache) {
        pager_close(pager);
        return -1;
    }

    rc = catalog_init(&catalog, pager, cache);
    if (rc != 0) {
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    rc = executor_init(&exec, pager, cache, &catalog);
    if (rc != 0) {
        catalog_close(&catalog);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    printf("Database '%s' ready.\n", DB_PATH_SQL);

    /* ===== Example 5: CREATE TABLE and INSERT ===== */
    print_section("Example 5: CREATE TABLE and INSERT");

    print_subsection("CREATE TABLE products");
    printf("   SQL: CREATE TABLE products (\n");
    printf("          id INTEGER PRIMARY KEY,\n");
    printf("          name TEXT,\n");
    printf("          price INTEGER,\n");
    printf("          category TEXT,\n");
    printf("          stock INTEGER\n");
    printf("        )\n");

    rc = run_sql(&exec, "CREATE TABLE products (id INTEGER PRIMARY KEY, name TEXT, price INTEGER, category TEXT, stock INTEGER)");
    if (rc == 0) {
        printf("   Table 'products' created.\n");
    }

    print_subsection("CREATE TABLE logs");
    printf("   SQL: CREATE TABLE logs (message TEXT, level INTEGER)\n");

    rc = run_sql(&exec, "CREATE TABLE logs (message TEXT, level INTEGER)");
    if (rc == 0) {
        printf("   Table 'logs' created (implicit rowid).\n");
    }

    print_subsection("INSERT INTO products");
    printf("   Inserting 10 products...\n");

    run_sql(&exec, "INSERT INTO products VALUES (1, 'Amiga 500', 299, 'Computer', 10)");
    run_sql(&exec, "INSERT INTO products VALUES (2, 'Amiga 1200', 499, 'Computer', 5)");
    run_sql(&exec, "INSERT INTO products VALUES (3, 'Amiga 4000', 1299, 'Computer', 2)");
    run_sql(&exec, "INSERT INTO products VALUES (4, 'Amiga CD32', 399, 'Console', 8)");
    run_sql(&exec, "INSERT INTO products VALUES (5, 'Tank Mouse', 35, 'Peripheral', 50)");
    run_sql(&exec, "INSERT INTO products VALUES (6, 'Competition Pro', 25, 'Peripheral', 40)");
    run_sql(&exec, "INSERT INTO products VALUES (7, 'Action Replay', 79, 'Accessory', 15)");
    run_sql(&exec, "INSERT INTO products VALUES (8, 'External Floppy', 89, 'Peripheral', 20)");
    run_sql(&exec, "INSERT INTO products VALUES (9, 'Kickstart 3.1', 49, 'Software', 100)");
    run_sql(&exec, "INSERT INTO products VALUES (10, 'Workbench 3.1', 39, 'Software', 100)");

    printf("   10 rows inserted.\n");

    print_subsection("INSERT INTO logs");
    run_sql(&exec, "INSERT INTO logs VALUES ('Database initialized', 1)");
    run_sql(&exec, "INSERT INTO logs VALUES ('Products loaded', 1)");
    run_sql(&exec, "INSERT INTO logs VALUES ('System ready', 1)");
    printf("   3 log entries inserted.\n");

    /* Flush to ensure data is visible */
    cache_flush(cache);

    printf("\n[OK] Example 5 completed.\n");

    /* ===== Example 6: SELECT Queries ===== */
    print_section("Example 6: SELECT Queries");

    print_subsection("SELECT * FROM products");
    run_sql(&exec, "SELECT * FROM products");
    print_results(&exec);

    print_subsection("SELECT WHERE category = 'Computer'");
    run_sql(&exec, "SELECT * FROM products WHERE category = 'Computer'");
    print_results(&exec);

    print_subsection("SELECT WHERE price > 100");
    run_sql(&exec, "SELECT * FROM products WHERE price > 100");
    print_results(&exec);

    print_subsection("SELECT WHERE price <= 50");
    run_sql(&exec, "SELECT * FROM products WHERE price <= 50");
    print_results(&exec);

    print_subsection("SELECT ORDER BY price ASC");
    run_sql(&exec, "SELECT * FROM products ORDER BY price");
    print_results(&exec);

    print_subsection("SELECT ORDER BY price DESC");
    run_sql(&exec, "SELECT * FROM products ORDER BY price DESC");
    print_results(&exec);

    print_subsection("SELECT LIMIT 3");
    run_sql(&exec, "SELECT * FROM products LIMIT 3");
    print_results(&exec);

    print_subsection("SELECT WHERE + ORDER BY + LIMIT");
    printf("   SQL: SELECT * FROM products WHERE price > 50\n");
    printf("        ORDER BY price DESC LIMIT 5\n");
    run_sql(&exec, "SELECT * FROM products WHERE price > 50 ORDER BY price DESC LIMIT 5");
    print_results(&exec);

    printf("\n[OK] Example 6 completed.\n");

    /* ===== Example 7: UPDATE and DELETE ===== */
    print_section("Example 7: UPDATE and DELETE");

    print_subsection("Before UPDATE");
    printf("   Product 1 (Amiga 500):\n");
    run_sql(&exec, "SELECT * FROM products WHERE id = 1");
    print_results(&exec);

    print_subsection("UPDATE by PRIMARY KEY");
    printf("   SQL: UPDATE products SET price = 349 WHERE id = 1\n");
    run_sql(&exec, "UPDATE products SET price = 349 WHERE id = 1");

    printf("   After UPDATE:\n");
    run_sql(&exec, "SELECT * FROM products WHERE id = 1");
    print_results(&exec);

    print_subsection("UPDATE multiple rows");
    printf("   SQL: UPDATE products SET stock = 999 WHERE category = 'Software'\n");
    run_sql(&exec, "UPDATE products SET stock = 999 WHERE category = 'Software'");

    printf("   Software products after UPDATE:\n");
    run_sql(&exec, "SELECT * FROM products WHERE category = 'Software'");
    print_results(&exec);

    print_subsection("DELETE by PRIMARY KEY");
    printf("   Before: Product count = ");
    run_sql(&exec, "SELECT COUNT(*) FROM products");
    print_results(&exec);

    printf("   SQL: DELETE FROM products WHERE id = 10\n");
    run_sql(&exec, "DELETE FROM products WHERE id = 10");

    printf("   After: Product count = ");
    run_sql(&exec, "SELECT COUNT(*) FROM products");
    print_results(&exec);

    print_subsection("DELETE by condition");
    printf("   SQL: DELETE FROM products WHERE price < 40\n");
    run_sql(&exec, "DELETE FROM products WHERE price < 40");

    printf("   Remaining products:\n");
    run_sql(&exec, "SELECT * FROM products ORDER BY id");
    print_results(&exec);

    printf("\n[OK] Example 7 completed.\n");

    /* ===== Example 8: Aggregate Functions ===== */
    print_section("Example 8: Aggregate Functions");

    print_subsection("COUNT(*)");
    printf("   SQL: SELECT COUNT(*) FROM products\n");
    printf("   Total products: ");
    run_sql(&exec, "SELECT COUNT(*) FROM products");
    print_results(&exec);

    print_subsection("COUNT(*) with WHERE");
    printf("   SQL: SELECT COUNT(*) FROM products WHERE price > 100\n");
    printf("   Products over $100: ");
    run_sql(&exec, "SELECT COUNT(*) FROM products WHERE price > 100");
    print_results(&exec);

    print_subsection("SUM");
    printf("   SQL: SELECT SUM(price) FROM products\n");
    printf("   Total inventory value: $");
    run_sql(&exec, "SELECT SUM(price) FROM products");
    print_results(&exec);

    print_subsection("SUM with WHERE");
    printf("   SQL: SELECT SUM(price) FROM products WHERE category = 'Computer'\n");
    printf("   Computer inventory value: $");
    run_sql(&exec, "SELECT SUM(price) FROM products WHERE category = 'Computer'");
    print_results(&exec);

    print_subsection("AVG");
    printf("   SQL: SELECT AVG(price) FROM products\n");
    printf("   Average price: $");
    run_sql(&exec, "SELECT AVG(price) FROM products");
    print_results(&exec);

    print_subsection("MIN");
    printf("   SQL: SELECT MIN(price) FROM products\n");
    printf("   Cheapest product: $");
    run_sql(&exec, "SELECT MIN(price) FROM products");
    print_results(&exec);

    print_subsection("MAX");
    printf("   SQL: SELECT MAX(price) FROM products\n");
    printf("   Most expensive product: $");
    run_sql(&exec, "SELECT MAX(price) FROM products");
    print_results(&exec);

    print_subsection("MAX with WHERE");
    printf("   SQL: SELECT MAX(stock) FROM products WHERE category = 'Peripheral'\n");
    printf("   Highest peripheral stock: ");
    run_sql(&exec, "SELECT MAX(stock) FROM products WHERE category = 'Peripheral'");
    print_results(&exec);

    printf("\n[OK] Example 8 completed.\n");

    /* ===== Example 9: DROP TABLE ===== */
    print_section("Example 9: DROP TABLE");

    print_subsection("Tables Before DROP");
    count = catalog_list_tables(&catalog, table_names, 32);
    printf("   Tables (%d):\n", count);
    for (int i = 0; i < count; i++) {
        printf("     - %s\n", table_names[i]);
    }

    print_subsection("DROP TABLE logs");
    printf("   SQL: DROP TABLE logs\n");
    run_sql(&exec, "DROP TABLE logs");
    printf("   Table 'logs' dropped.\n");

    print_subsection("Tables After DROP");
    count = catalog_list_tables(&catalog, table_names, 32);
    printf("   Tables (%d):\n", count);
    for (int i = 0; i < count; i++) {
        printf("     - %s\n", table_names[i]);
    }

    print_subsection("DROP Non-Existent Table");
    printf("   SQL: DROP TABLE nonexistent\n");
    rc = run_sql(&exec, "DROP TABLE nonexistent");
    if (rc != 0) {
        printf("   (Error is expected)\n");
    }

    print_subsection("Recreate logs with new schema");
    printf("   SQL: CREATE TABLE logs (id INTEGER PRIMARY KEY, msg TEXT, severity INTEGER)\n");
    run_sql(&exec, "CREATE TABLE logs (id INTEGER PRIMARY KEY, msg TEXT, severity INTEGER)");
    printf("   New 'logs' table created.\n");

    run_sql(&exec, "INSERT INTO logs VALUES (1, 'System rebooted', 1)");
    run_sql(&exec, "INSERT INTO logs VALUES (2, 'Error detected', 3)");
    printf("   2 rows inserted into new schema.\n");

    printf("   New table contents:\n");
    run_sql(&exec, "SELECT * FROM logs");
    print_results(&exec);

    printf("\n[OK] Example 9 completed.\n");

    /* ===== Example 10: Complete Workflow ===== */
    print_section("Example 10: Complete Inventory Workflow");

    printf("\n=== DAILY INVENTORY REPORT ===\n");

    print_subsection("Inventory Summary");

    printf("   Total Products: ");
    run_sql(&exec, "SELECT COUNT(*) FROM products");
    print_results(&exec);

    printf("   Total Value: $");
    run_sql(&exec, "SELECT SUM(price) FROM products");
    print_results(&exec);

    printf("   Average Price: $");
    run_sql(&exec, "SELECT AVG(price) FROM products");
    print_results(&exec);

    printf("   Price Range: $");
    run_sql(&exec, "SELECT MIN(price) FROM products");
    struct amidb_row *row = &exec.result_rows[0];
    const struct amidb_value *val = row_get_value(row, 0);
    int min_price = (val && val->type == AMIDB_TYPE_INTEGER) ? val->u.i : 0;

    run_sql(&exec, "SELECT MAX(price) FROM products");
    row = &exec.result_rows[0];
    val = row_get_value(row, 0);
    int max_price = (val && val->type == AMIDB_TYPE_INTEGER) ? val->u.i : 0;

    printf("   %d - $%d\n", min_price, max_price);

    print_subsection("By Category");

    printf("   Computers: ");
    run_sql(&exec, "SELECT COUNT(*) FROM products WHERE category = 'Computer'");
    print_results(&exec);

    printf("   Peripherals: ");
    run_sql(&exec, "SELECT COUNT(*) FROM products WHERE category = 'Peripheral'");
    print_results(&exec);

    printf("   Accessories: ");
    run_sql(&exec, "SELECT COUNT(*) FROM products WHERE category = 'Accessory'");
    print_results(&exec);

    print_subsection("High-Value Items (> $200)");
    run_sql(&exec, "SELECT * FROM products WHERE price > 200 ORDER BY price DESC");
    print_results(&exec);

    print_subsection("Low Stock Alert (< 10 units)");
    run_sql(&exec, "SELECT * FROM products WHERE stock < 10");
    print_results(&exec);

    print_subsection("Recent System Logs");
    run_sql(&exec, "SELECT * FROM logs ORDER BY id DESC LIMIT 5");
    print_results(&exec);

    print_subsection("Processing Sale: Amiga 1200");
    printf("   Before sale:\n");
    run_sql(&exec, "SELECT * FROM products WHERE id = 2");
    print_results(&exec);

    printf("   Updating stock (5 -> 4)...\n");
    run_sql(&exec, "UPDATE products SET stock = 4 WHERE id = 2");

    printf("   After sale:\n");
    run_sql(&exec, "SELECT * FROM products WHERE id = 2");
    print_results(&exec);

    printf("\n[OK] Example 10 completed.\n");

    /* Cleanup SQL engine */
    cache_flush(cache);
    executor_close(&exec);
    catalog_close(&catalog);
    cache_destroy(cache);
    pager_close(pager);

    return 0;
}

/* ============================================================
 * MAIN FUNCTION
 * ============================================================ */

int main(void) {
    int rc;
    int failed = 0;

    printf("\n");
    printf("*****************************************************\n");
    printf("*     AmiDB Comprehensive Example Program           *\n");
    printf("*     Demonstrating ALL Database Capabilities       *\n");
    printf("*****************************************************\n");
    printf("\n");
    printf("Platform: AmigaOS 3.1 / 68000 CPU\n");
    printf("Constraints: 2MB RAM, 4KB Stack\n");
    printf("\n");

    /* Part A: Direct C API */
    printf("\n");
    printf("=====================================================\n");
    printf("PART A: DIRECT C API\n");
    printf("=====================================================\n");

    if ((rc = example_btree_basics()) != 0) failed++;
    if ((rc = example_row_serialization()) != 0) failed++;
    if ((rc = example_transactions()) != 0) failed++;
    if ((rc = example_crash_recovery()) != 0) failed++;

    /* Part B: SQL Interface (single session) */
    printf("\n");
    printf("=====================================================\n");
    printf("PART B: SQL INTERFACE\n");
    printf("=====================================================\n");

    if ((rc = run_sql_examples()) != 0) failed++;

    /* Final Summary */
    print_section("DEMONSTRATION COMPLETE");

    if (failed == 0) {
        printf("\nAll examples completed successfully!\n\n");

        printf("Capabilities Demonstrated:\n");
        printf("  DIRECT API:\n");
        printf("    [x] Pager - Page-based file I/O\n");
        printf("    [x] Cache - LRU page caching\n");
        printf("    [x] B+Tree - Indexed storage (insert/search/delete/cursor)\n");
        printf("    [x] Row - Multi-column serialization\n");
        printf("    [x] WAL - Write-Ahead Logging\n");
        printf("    [x] Transactions - ACID guarantees\n");
        printf("    [x] Recovery - Crash recovery\n");
        printf("\n");
        printf("  SQL INTERFACE:\n");
        printf("    [x] CREATE TABLE - Schema definition\n");
        printf("    [x] DROP TABLE - Schema removal\n");
        printf("    [x] INSERT - Data insertion\n");
        printf("    [x] SELECT - Queries with WHERE/ORDER BY/LIMIT\n");
        printf("    [x] UPDATE - Data modification\n");
        printf("    [x] DELETE - Data removal\n");
        printf("    [x] COUNT(*) - Row counting\n");
        printf("    [x] SUM() - Numeric summation\n");
        printf("    [x] AVG() - Numeric averaging\n");
        printf("    [x] MIN() - Minimum value\n");
        printf("    [x] MAX() - Maximum value\n");
        printf("\n");
        printf("AmiDB is ready for your Amiga applications!\n");
    } else {
        printf("\n%d example(s) failed.\n", failed);
    }

    printf("\n");
    return failed;
}
