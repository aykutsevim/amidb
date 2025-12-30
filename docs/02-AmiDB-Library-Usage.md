# Using AmiDB as a Library

A comprehensive guide for integrating AmiDB into your AmigaOS applications, covering both the direct C API and the SQL interface.

## Table of Contents

1. [Introduction](#introduction)
2. [Architecture Overview](#architecture-overview)
3. [Direct API Access](#direct-api-access)
4. [SQL Interface](#sql-interface)
5. [Memory Management](#memory-management)
6. [Transaction Handling](#transaction-handling)
7. [Error Handling](#error-handling)
8. [Complete Examples](#complete-examples)
9. [Best Practices](#best-practices)

---

## Introduction

AmiDB can be used in two ways:

1. **Direct C API** - Low-level access to the storage engine (pager, cache, B+Tree, rows)
2. **SQL Interface** - High-level SQL parsing and execution

Choose based on your needs:

| Use Case | Recommended Interface |
|----------|----------------------|
| Simple key-value storage | Direct API |
| Relational data with queries | SQL Interface |
| Maximum performance | Direct API |
| Rapid development | SQL Interface |
| Custom indexing strategies | Direct API |
| Standard SQL operations | SQL Interface |

---

## Architecture Overview

```
+--------------------------------------------------+
|                  Your Application                 |
+--------------------------------------------------+
         |                          |
         v                          v
+------------------+      +--------------------+
|   SQL Interface  |      |    Direct C API    |
| (lexer, parser,  |      | (pager, cache,     |
|  executor, REPL) |      |  btree, row)       |
+------------------+      +--------------------+
         |                          |
         +------------+-------------+
                      |
                      v
+--------------------------------------------------+
|              Storage Engine Core                  |
|  +----------+  +---------+  +-----------------+  |
|  |  Pager   |--|  Cache  |--|    B+Tree       |  |
|  +----------+  +---------+  +-----------------+  |
|                     |                            |
|  +------------------+------------------------+   |
|  |              Transaction Manager          |   |
|  |          (WAL + ACID Guarantees)          |   |
|  +-------------------------------------------+   |
+--------------------------------------------------+
                      |
                      v
              [ Database File ]
```

### Core Components

| Component | Header | Purpose |
|-----------|--------|---------|
| Pager | `storage/pager.h` | Page-based file I/O, allocation bitmap |
| Cache | `storage/cache.h` | LRU page cache with pinning |
| B+Tree | `storage/btree.h` | Indexed key-value storage |
| Row | `storage/row.h` | Row serialization/deserialization |
| WAL | `txn/wal.h` | Write-ahead logging |
| Transaction | `txn/txn.h` | ACID transaction support |
| Catalog | `sql/catalog.h` | Table schema storage |
| Executor | `sql/executor.h` | SQL statement execution |

---

## Direct API Access

### Opening a Database

```c
#include "storage/pager.h"
#include "storage/cache.h"
#include "storage/btree.h"

int main(void) {
    struct amidb_pager *pager = NULL;
    struct page_cache *cache = NULL;
    struct btree *tree = NULL;
    uint32_t root_page;
    int rc;

    /* Open or create database file */
    rc = pager_open("RAM:mydata.db", 0, &pager);
    if (rc != 0) {
        printf("Failed to open database\n");
        return 1;
    }

    /* Create page cache (128 pages = 512KB) */
    cache = cache_create(128, pager);
    if (cache == NULL) {
        printf("Failed to create cache\n");
        pager_close(pager);
        return 1;
    }

    /* Create or open B+Tree */
    if (pager->header.root_page == 0) {
        /* New database - create tree */
        tree = btree_create(pager, cache, &root_page);
        pager->header.root_page = root_page;
    } else {
        /* Existing database - open tree */
        tree = btree_open(pager, cache, pager->header.root_page);
    }

    if (tree == NULL) {
        printf("Failed to create/open B+Tree\n");
        cache_destroy(cache);
        pager_close(pager);
        return 1;
    }

    /* Use the tree... */

    /* Cleanup */
    btree_close(tree);
    cache_destroy(cache);  /* Flushes dirty pages */
    pager_close(pager);

    return 0;
}
```

### B+Tree Operations

#### Inserting Data

```c
#include "storage/btree.h"

/* Insert key-value pairs */
int store_data(struct btree *tree) {
    int rc;

    /* Insert integer key with page number as value */
    rc = btree_insert(tree, 1, 100);  /* key=1, value=100 */
    if (rc != 0) {
        printf("Insert failed\n");
        return -1;
    }

    rc = btree_insert(tree, 2, 200);
    rc = btree_insert(tree, 3, 300);

    return 0;
}
```

#### Searching for Data

```c
int find_data(struct btree *tree, int32_t key) {
    uint32_t value;
    int rc;

    rc = btree_search(tree, key, &value);
    if (rc == 0) {
        printf("Found key %d with value %u\n", key, value);
        return value;
    } else {
        printf("Key %d not found\n", key);
        return -1;
    }
}
```

#### Deleting Data

```c
int remove_data(struct btree *tree, int32_t key) {
    int rc;

    rc = btree_delete(tree, key);
    if (rc == 0) {
        printf("Key %d deleted\n", key);
        return 0;
    } else {
        printf("Key %d not found for deletion\n", key);
        return -1;
    }
}
```

#### Iterating Over All Entries

```c
void iterate_all(struct btree *tree) {
    struct btree_cursor cursor;
    int32_t key;
    uint32_t value;
    int rc;

    /* Position cursor at first entry */
    rc = btree_cursor_first(tree, &cursor);
    if (rc != 0) {
        printf("Tree is empty\n");
        return;
    }

    /* Iterate through all entries */
    while (btree_cursor_valid(&cursor)) {
        rc = btree_cursor_get(&cursor, &key, &value);
        if (rc == 0) {
            printf("Key: %d, Value: %u\n", key, value);
        }

        /* Move to next entry */
        if (btree_cursor_next(&cursor) != 0) {
            break;  /* No more entries */
        }
    }
}
```

### Row Serialization

#### Creating and Serializing Rows

```c
#include "storage/row.h"

int store_row(struct btree *tree, struct page_cache *cache,
              struct amidb_pager *pager, int32_t id, const char *name, int32_t price) {
    struct amidb_row row;
    static uint8_t buffer[4096];  /* Static to avoid stack overflow */
    uint32_t page_num;
    uint8_t *page_data;
    int bytes_written;
    int rc;

    /* Initialize row */
    row_init(&row);

    /* Set column values */
    row_set_int(&row, 0, id);              /* Column 0: INTEGER */
    row_set_text(&row, 1, name, 0);        /* Column 1: TEXT (0 = use strlen) */
    row_set_int(&row, 2, price);           /* Column 2: INTEGER */

    /* Serialize row to buffer */
    bytes_written = row_serialize(&row, buffer, sizeof(buffer));
    if (bytes_written < 0) {
        printf("Serialization failed\n");
        row_clear(&row);
        return -1;
    }

    /* Allocate a page for the row data */
    rc = pager_allocate_page(pager, &page_num);
    if (rc != 0) {
        printf("Page allocation failed\n");
        row_clear(&row);
        return -1;
    }

    /* Get page from cache and write data */
    rc = cache_get_page(cache, page_num, &page_data);
    if (rc == 0) {
        memcpy(page_data, buffer, bytes_written);
        cache_mark_dirty(cache, page_num);
        cache_unpin(cache, page_num);
    }

    /* Insert into B+Tree: id -> page_num */
    rc = btree_insert(tree, id, page_num);

    /* Clean up row (frees TEXT/BLOB memory) */
    row_clear(&row);

    return rc;
}
```

#### Reading and Deserializing Rows

```c
int read_row(struct btree *tree, struct page_cache *cache,
             int32_t id) {
    uint32_t page_num;
    uint8_t *page_data;
    struct amidb_row row;
    const struct amidb_value *val;
    int rc;

    /* Find row's page number */
    rc = btree_search(tree, id, &page_num);
    if (rc != 0) {
        printf("Row with id %d not found\n", id);
        return -1;
    }

    /* Read page from cache */
    rc = cache_get_page(cache, page_num, &page_data);
    if (rc != 0) {
        printf("Failed to read page\n");
        return -1;
    }

    /* Deserialize row */
    row_init(&row);
    rc = row_deserialize(&row, page_data, 4096);
    cache_unpin(cache, page_num);

    if (rc < 0) {
        printf("Deserialization failed\n");
        return -1;
    }

    /* Access column values */
    val = row_get_value(&row, 0);
    if (val && val->type == AMIDB_TYPE_INTEGER) {
        printf("ID: %d\n", val->u.i);
    }

    val = row_get_value(&row, 1);
    if (val && val->type == AMIDB_TYPE_TEXT) {
        /* TEXT is length-prefixed, NOT null-terminated! */
        printf("Name: %.*s\n", (int)val->u.blob.size, (char *)val->u.blob.data);
    }

    val = row_get_value(&row, 2);
    if (val && val->type == AMIDB_TYPE_INTEGER) {
        printf("Price: %d\n", val->u.i);
    }

    row_clear(&row);
    return 0;
}
```

### Data Types

```c
/* Setting different column types */
void demonstrate_types(struct amidb_row *row) {
    uint8_t binary_data[] = {0x00, 0x01, 0x02, 0x03};

    /* INTEGER: 32-bit signed */
    row_set_int(row, 0, 42);
    row_set_int(row, 1, -100);

    /* TEXT: UTF-8 string (length-prefixed, NOT null-terminated) */
    row_set_text(row, 2, "Hello Amiga", 0);  /* 0 = auto-calculate length */
    row_set_text(row, 3, "Fixed", 5);        /* Explicit length */

    /* BLOB: Binary data */
    row_set_blob(row, 4, binary_data, sizeof(binary_data));

    /* NULL: Explicit null value */
    row_set_null(row, 5);
}
```

---

## SQL Interface

### Setting Up the SQL Executor

```c
#include "storage/pager.h"
#include "storage/cache.h"
#include "sql/catalog.h"
#include "sql/executor.h"
#include "sql/lexer.h"
#include "sql/parser.h"

int setup_sql_engine(void) {
    struct amidb_pager *pager = NULL;
    struct page_cache *cache = NULL;
    struct catalog catalog;
    struct sql_executor executor;
    int rc;

    /* Open database */
    rc = pager_open("RAM:sqltest.db", 0, &pager);
    if (rc != 0) return -1;

    /* Create cache */
    cache = cache_create(128, pager);
    if (cache == NULL) {
        pager_close(pager);
        return -1;
    }

    /* Initialize catalog (stores table schemas) */
    rc = catalog_init(&catalog, pager, cache);
    if (rc != 0) {
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    /* Initialize SQL executor */
    rc = executor_init(&executor, pager, cache, &catalog);
    if (rc != 0) {
        catalog_close(&catalog);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    /* Use the executor... */

    /* Cleanup (in reverse order) */
    executor_close(&executor);
    catalog_close(&catalog);
    cache_destroy(cache);
    pager_close(pager);

    return 0;
}
```

### Executing SQL Statements

```c
#include "sql/lexer.h"
#include "sql/parser.h"
#include "sql/executor.h"

/*
 * Execute a SQL statement
 * Returns 0 on success, -1 on error
 */
int execute_sql(struct sql_executor *exec, const char *sql) {
    struct sql_lexer lexer;
    struct sql_parser parser;
    struct sql_statement stmt;
    int rc;

    /* Tokenize SQL */
    lexer_init(&lexer, sql);

    /* Parse into AST */
    parser_init(&parser, &lexer);
    rc = parser_parse_statement(&parser, &stmt);
    if (rc != 0) {
        printf("Parse error: %s\n", parser_get_error(&parser));
        return -1;
    }

    /* Execute statement */
    rc = executor_execute(exec, &stmt);
    if (rc != 0) {
        printf("Execution error: %s\n", executor_get_error(exec));
        return -1;
    }

    return 0;
}
```

### Complete SQL Example

```c
#include "storage/pager.h"
#include "storage/cache.h"
#include "sql/catalog.h"
#include "sql/executor.h"
#include "sql/lexer.h"
#include "sql/parser.h"
#include "storage/row.h"
#include <stdio.h>
#include <string.h>

/* Helper function to execute SQL */
static int run_sql(struct sql_executor *exec, const char *sql) {
    struct sql_lexer lexer;
    struct sql_parser parser;
    struct sql_statement stmt;

    lexer_init(&lexer, sql);
    parser_init(&parser, &lexer);

    if (parser_parse_statement(&parser, &stmt) != 0) {
        printf("Parse error in: %s\n", sql);
        return -1;
    }

    if (executor_execute(exec, &stmt) != 0) {
        printf("Error: %s\n", executor_get_error(exec));
        return -1;
    }

    return 0;
}

/* Print SELECT results */
static void print_results(struct sql_executor *exec) {
    uint32_t i, j;
    struct amidb_row *row;
    const struct amidb_value *val;

    printf("Results: %u rows\n", exec->result_count);
    for (i = 0; i < exec->result_count; i++) {
        row = &exec->result_rows[i];
        printf("  Row %u: ", i + 1);

        for (j = 0; j < row->column_count; j++) {
            if (j > 0) printf(", ");

            val = row_get_value(row, j);
            if (val == NULL || val->type == AMIDB_TYPE_NULL) {
                printf("NULL");
            } else if (val->type == AMIDB_TYPE_INTEGER) {
                printf("%d", val->u.i);
            } else if (val->type == AMIDB_TYPE_TEXT) {
                printf("'%.*s'", (int)val->u.blob.size, (char *)val->u.blob.data);
            }
        }
        printf("\n");
    }
}

int main(void) {
    struct amidb_pager *pager = NULL;
    struct page_cache *cache = NULL;
    struct catalog catalog;
    struct sql_executor exec;
    int rc;

    /* Initialize database */
    rc = pager_open("RAM:demo.db", 0, &pager);
    if (rc != 0) { printf("Pager failed\n"); return 1; }

    cache = cache_create(128, pager);
    if (!cache) { pager_close(pager); return 1; }

    rc = catalog_init(&catalog, pager, cache);
    if (rc != 0) { cache_destroy(cache); pager_close(pager); return 1; }

    rc = executor_init(&exec, pager, cache, &catalog);
    if (rc != 0) { catalog_close(&catalog); cache_destroy(cache); pager_close(pager); return 1; }

    /* Create table */
    printf("Creating table...\n");
    run_sql(&exec, "CREATE TABLE products (id INTEGER PRIMARY KEY, name TEXT, price INTEGER)");

    /* Insert data */
    printf("Inserting data...\n");
    run_sql(&exec, "INSERT INTO products VALUES (1, 'Amiga 500', 299)");
    run_sql(&exec, "INSERT INTO products VALUES (2, 'Amiga 1200', 499)");
    run_sql(&exec, "INSERT INTO products VALUES (3, 'Amiga 4000', 1299)");

    /* Query data */
    printf("\nAll products:\n");
    run_sql(&exec, "SELECT * FROM products");
    print_results(&exec);

    printf("\nProducts over 400:\n");
    run_sql(&exec, "SELECT * FROM products WHERE price > 400");
    print_results(&exec);

    printf("\nProduct count:\n");
    run_sql(&exec, "SELECT COUNT(*) FROM products");
    print_results(&exec);

    printf("\nTotal value:\n");
    run_sql(&exec, "SELECT SUM(price) FROM products");
    print_results(&exec);

    /* Update data */
    printf("\nUpdating price...\n");
    run_sql(&exec, "UPDATE products SET price = 349 WHERE id = 1");

    printf("\nAfter update:\n");
    run_sql(&exec, "SELECT * FROM products WHERE id = 1");
    print_results(&exec);

    /* Delete data */
    printf("\nDeleting expensive items...\n");
    run_sql(&exec, "DELETE FROM products WHERE price > 1000");

    printf("\nRemaining products:\n");
    run_sql(&exec, "SELECT * FROM products");
    print_results(&exec);

    /* Cleanup */
    executor_close(&exec);
    catalog_close(&catalog);
    cache_destroy(cache);
    pager_close(pager);

    printf("\nDone!\n");
    return 0;
}
```

### Supported SQL Syntax

#### CREATE TABLE

```sql
-- With explicit PRIMARY KEY
CREATE TABLE users (
    id INTEGER PRIMARY KEY,
    name TEXT,
    age INTEGER
);

-- Without explicit PRIMARY KEY (implicit rowid)
CREATE TABLE logs (
    message TEXT,
    timestamp INTEGER
);
```

#### DROP TABLE

```sql
DROP TABLE users;
```

#### INSERT

```sql
-- All columns
INSERT INTO users VALUES (1, 'Alice', 30);

-- Text values use single quotes
INSERT INTO products VALUES (1, 'Amiga 500', 299);
```

#### SELECT

```sql
-- All columns
SELECT * FROM users;

-- With WHERE clause (=, !=, <, <=, >, >=)
SELECT * FROM users WHERE age > 25;
SELECT * FROM products WHERE name = 'Amiga 500';

-- With ORDER BY (ASC or DESC)
SELECT * FROM products ORDER BY price DESC;

-- With LIMIT
SELECT * FROM users LIMIT 10;

-- Aggregate functions
SELECT COUNT(*) FROM users;
SELECT COUNT(name) FROM users;
SELECT SUM(price) FROM products;
SELECT AVG(price) FROM products;
SELECT MIN(price) FROM products;
SELECT MAX(price) FROM products;

-- Aggregates with WHERE
SELECT SUM(price) FROM products WHERE price > 100;
```

#### UPDATE

```sql
-- Update specific row
UPDATE products SET price = 349 WHERE id = 1;

-- Update multiple rows
UPDATE products SET price = 0 WHERE price < 100;
```

#### DELETE

```sql
-- Delete specific row
DELETE FROM products WHERE id = 1;

-- Delete multiple rows
DELETE FROM products WHERE price < 100;
```

---

## Memory Management

### The 4KB Stack Problem

The Amiga 68000 has only 4KB of stack space. Large structures MUST use static allocation:

```c
/* WRONG - Will crash! */
void bad_function(void) {
    struct table_schema schema;     /* ~2300 bytes */
    uint8_t buffer[4096];           /* 4096 bytes */
    /* Total: 6400 bytes > 4KB stack! */
}

/* CORRECT - Static allocation */
void good_function(void) {
    static struct table_schema schema;
    static uint8_t buffer[4096];
    /* Stack usage: ~16 bytes (just pointers) */
}
```

### Large Structure Sizes

| Structure | Approximate Size |
|-----------|------------------|
| `struct table_schema` | ~2300 bytes |
| `struct sql_statement` | ~8000 bytes |
| `struct amidb_row` | ~1200 bytes |
| `struct btree_node` | ~1500 bytes |

### Memory Allocation Functions

```c
#include "os/mem.h"

/* Allocate memory */
void *mem_alloc(size_t size);

/* Free memory */
void mem_free(void *ptr);
```

### Row Memory Management

```c
/* Row values (TEXT/BLOB) are heap-allocated */
struct amidb_row row;
row_init(&row);

row_set_text(&row, 0, "Hello", 0);  /* Allocates memory for "Hello" */

/* IMPORTANT: Always clear rows to free memory! */
row_clear(&row);  /* Frees the TEXT data */
```

---

## Transaction Handling

### Basic Transaction Usage

```c
#include "txn/txn.h"
#include "txn/wal.h"

int use_transactions(struct amidb_pager *pager, struct page_cache *cache) {
    struct wal_context *wal;
    struct txn_context *txn;
    int rc;

    /* Create WAL (Write-Ahead Log) */
    wal = wal_create(pager);
    if (wal == NULL) {
        printf("Failed to create WAL\n");
        return -1;
    }

    /* Create transaction context */
    txn = txn_create(wal, cache);
    if (txn == NULL) {
        wal_destroy(wal);
        return -1;
    }

    /* Begin transaction */
    rc = txn_begin(txn);
    if (rc != 0) {
        printf("Begin failed\n");
        txn_destroy(txn);
        wal_destroy(wal);
        return -1;
    }

    /* Perform operations... */
    /* All dirty pages are tracked by the transaction */

    /* Commit (makes changes permanent) */
    rc = txn_commit(txn);
    if (rc != 0) {
        printf("Commit failed, rolling back\n");
        txn_abort(txn);
    }

    /* Cleanup */
    txn_destroy(txn);
    wal_destroy(wal);

    return rc;
}
```

### Transaction with B+Tree

```c
int transactional_inserts(struct btree *tree, struct txn_context *txn) {
    int rc;

    /* Associate transaction with B+Tree */
    btree_set_transaction(tree, txn);

    /* Begin transaction */
    rc = txn_begin(txn);
    if (rc != 0) return -1;

    /* Multiple inserts in one transaction */
    btree_insert(tree, 1, 100);
    btree_insert(tree, 2, 200);
    btree_insert(tree, 3, 300);

    /* If something goes wrong, abort */
    if (some_error_condition) {
        txn_abort(txn);  /* Rolls back all changes */
        btree_set_transaction(tree, NULL);
        return -1;
    }

    /* Commit all changes atomically */
    rc = txn_commit(txn);

    /* Clear transaction from tree */
    btree_set_transaction(tree, NULL);

    return rc;
}
```

### ACID Guarantees

- **Atomicity**: All operations in a transaction succeed or all are rolled back
- **Consistency**: Database remains in a valid state
- **Isolation**: Uncommitted changes are invisible to other operations
- **Durability**: Committed changes survive crashes (via WAL)

---

## Error Handling

### Error Codes

```c
#include "api/error.h"

/* Common error codes */
#define AMIDB_OK        0     /* Success */
#define AMIDB_ERROR    -1     /* Generic error */
#define AMIDB_NOTFOUND -2     /* Key/row not found */
#define AMIDB_BUSY     -3     /* Resource busy */
#define AMIDB_FULL     -4     /* No space available */
#define AMIDB_CORRUPT  -5     /* Data corruption detected */
#define AMIDB_NOMEM    -6     /* Out of memory */
```

### Checking Return Values

```c
int safe_operations(void) {
    struct amidb_pager *pager = NULL;
    int rc;

    rc = pager_open("RAM:test.db", 0, &pager);
    if (rc != 0) {
        printf("Failed to open database: error %d\n", rc);
        return rc;
    }

    /* Always check return values */
    rc = btree_insert(tree, key, value);
    if (rc != 0) {
        printf("Insert failed\n");
        goto cleanup;
    }

cleanup:
    if (pager) pager_close(pager);
    return rc;
}
```

### SQL Error Messages

```c
/* Get SQL execution errors */
const char *executor_get_error(struct sql_executor *exec);

/* Get SQL parse errors */
const char *parser_get_error(struct sql_parser *parser);

/* Example */
rc = executor_execute(&exec, &stmt);
if (rc != 0) {
    printf("SQL Error: %s\n", executor_get_error(&exec));
}
```

---

## Complete Examples

### Example 1: Simple Key-Value Store

```c
/*
 * Simple key-value store using direct API
 */
#include "storage/pager.h"
#include "storage/cache.h"
#include "storage/btree.h"
#include <stdio.h>

struct kv_store {
    struct amidb_pager *pager;
    struct page_cache *cache;
    struct btree *tree;
};

int kv_open(struct kv_store *kv, const char *path) {
    uint32_t root_page;
    int rc;

    rc = pager_open(path, 0, &kv->pager);
    if (rc != 0) return -1;

    kv->cache = cache_create(64, kv->pager);
    if (!kv->cache) {
        pager_close(kv->pager);
        return -1;
    }

    if (kv->pager->header.root_page == 0) {
        kv->tree = btree_create(kv->pager, kv->cache, &root_page);
        kv->pager->header.root_page = root_page;
        pager_write_header(kv->pager);
    } else {
        kv->tree = btree_open(kv->pager, kv->cache, kv->pager->header.root_page);
    }

    return (kv->tree != NULL) ? 0 : -1;
}

void kv_close(struct kv_store *kv) {
    if (kv->tree) btree_close(kv->tree);
    if (kv->cache) cache_destroy(kv->cache);
    if (kv->pager) pager_close(kv->pager);
}

int kv_put(struct kv_store *kv, int32_t key, uint32_t value) {
    return btree_insert(kv->tree, key, value);
}

int kv_get(struct kv_store *kv, int32_t key, uint32_t *value) {
    return btree_search(kv->tree, key, value);
}

int kv_delete(struct kv_store *kv, int32_t key) {
    return btree_delete(kv->tree, key);
}

int main(void) {
    struct kv_store kv;
    uint32_t value;

    if (kv_open(&kv, "RAM:kvstore.db") != 0) {
        printf("Failed to open store\n");
        return 1;
    }

    /* Store some values */
    kv_put(&kv, 100, 1000);
    kv_put(&kv, 200, 2000);
    kv_put(&kv, 300, 3000);

    /* Retrieve */
    if (kv_get(&kv, 200, &value) == 0) {
        printf("Key 200 = %u\n", value);
    }

    /* Delete */
    kv_delete(&kv, 200);

    kv_close(&kv);
    return 0;
}
```

### Example 2: Product Database with SQL

```c
/*
 * Product database using SQL interface
 */
#include "storage/pager.h"
#include "storage/cache.h"
#include "sql/catalog.h"
#include "sql/executor.h"
#include "sql/lexer.h"
#include "sql/parser.h"
#include "storage/row.h"
#include <stdio.h>

struct product_db {
    struct amidb_pager *pager;
    struct page_cache *cache;
    struct catalog catalog;
    struct sql_executor exec;
};

static int run_sql(struct sql_executor *exec, const char *sql) {
    struct sql_lexer lexer;
    struct sql_parser parser;
    struct sql_statement stmt;

    lexer_init(&lexer, sql);
    parser_init(&parser, &lexer);
    if (parser_parse_statement(&parser, &stmt) != 0) return -1;
    return executor_execute(exec, &stmt);
}

int db_open(struct product_db *db, const char *path) {
    int rc;

    rc = pager_open(path, 0, &db->pager);
    if (rc != 0) return -1;

    db->cache = cache_create(128, db->pager);
    if (!db->cache) { pager_close(db->pager); return -1; }

    rc = catalog_init(&db->catalog, db->pager, db->cache);
    if (rc != 0) { cache_destroy(db->cache); pager_close(db->pager); return -1; }

    rc = executor_init(&db->exec, db->pager, db->cache, &db->catalog);
    if (rc != 0) { catalog_close(&db->catalog); cache_destroy(db->cache); pager_close(db->pager); return -1; }

    return 0;
}

void db_close(struct product_db *db) {
    executor_close(&db->exec);
    catalog_close(&db->catalog);
    cache_destroy(db->cache);
    pager_close(db->pager);
}

int db_create_schema(struct product_db *db) {
    return run_sql(&db->exec, "CREATE TABLE products (id INTEGER PRIMARY KEY, name TEXT, price INTEGER, stock INTEGER)");
}

int db_add_product(struct product_db *db, int id, const char *name, int price, int stock) {
    char sql[512];
    snprintf(sql, sizeof(sql), "INSERT INTO products VALUES (%d, '%s', %d, %d)",
             id, name, price, stock);
    return run_sql(&db->exec, sql);
}

void db_list_products(struct product_db *db) {
    uint32_t i;
    struct amidb_row *row;
    const struct amidb_value *val;

    run_sql(&db->exec, "SELECT * FROM products ORDER BY price");

    printf("\n%-4s %-20s %8s %6s\n", "ID", "Name", "Price", "Stock");
    printf("------------------------------------------\n");

    for (i = 0; i < db->exec.result_count; i++) {
        row = &db->exec.result_rows[i];

        val = row_get_value(row, 0);
        printf("%-4d ", val->u.i);

        val = row_get_value(row, 1);
        printf("%-20.*s ", (int)val->u.blob.size, (char *)val->u.blob.data);

        val = row_get_value(row, 2);
        printf("%8d ", val->u.i);

        val = row_get_value(row, 3);
        printf("%6d\n", val->u.i);
    }
}

int main(void) {
    struct product_db db;

    if (db_open(&db, "RAM:products.db") != 0) {
        printf("Failed to open database\n");
        return 1;
    }

    /* Create schema (ignore error if already exists) */
    db_create_schema(&db);

    /* Add products */
    db_add_product(&db, 1, "Amiga 500", 299, 10);
    db_add_product(&db, 2, "Amiga 1200", 499, 5);
    db_add_product(&db, 3, "Amiga 4000", 1299, 2);
    db_add_product(&db, 4, "Mouse", 25, 50);
    db_add_product(&db, 5, "Joystick", 15, 30);

    /* List all products */
    printf("Product Inventory:\n");
    db_list_products(&db);

    /* Show stats */
    run_sql(&db.exec, "SELECT COUNT(*) FROM products");
    printf("\nTotal products: %d\n", db.exec.result_rows[0].values[0].u.i);

    run_sql(&db.exec, "SELECT SUM(price) FROM products");
    printf("Total value: %d\n", db.exec.result_rows[0].values[0].u.i);

    db_close(&db);
    return 0;
}
```

---

## Best Practices

### 1. Always Use Static Allocation for Large Structures

```c
/* Good */
static struct sql_statement stmt;
static uint8_t buffer[4096];

/* Bad - will overflow stack */
struct sql_statement stmt;
uint8_t buffer[4096];
```

### 2. Always Clear Rows After Use

```c
struct amidb_row row;
row_init(&row);
/* ... use row ... */
row_clear(&row);  /* ALWAYS call this! */
```

### 3. Check All Return Values

```c
rc = btree_insert(tree, key, value);
if (rc != 0) {
    /* Handle error */
}
```

### 4. Use Appropriate Cache Sizes

```c
/* Small database (< 1000 rows): 32-64 pages */
cache = cache_create(64, pager);

/* Medium database: 128 pages (512KB) */
cache = cache_create(128, pager);

/* Large database (memory permitting): 256 pages (1MB) */
cache = cache_create(256, pager);
```

### 5. Flush Cache After Bulk Operations

```c
/* After many inserts */
for (i = 0; i < 100; i++) {
    run_sql(&exec, insert_statements[i]);
}
cache_flush(cache);  /* Ensure all data is on disk */
```

### 6. Use Transactions for Related Operations

```c
txn_begin(txn);
btree_insert(tree, 1, 100);
btree_insert(tree, 2, 200);
btree_insert(tree, 3, 300);
txn_commit(txn);  /* All or nothing */
```

### 7. Handle TEXT as Length-Prefixed, Not Null-Terminated

```c
/* Good */
printf("%.*s", (int)val->u.blob.size, (char *)val->u.blob.data);

/* Bad - may read garbage */
printf("%s", (char *)val->u.blob.data);
```

---

## Next Steps

1. Review [AmiDB Shell Guide](03-AmiDB-Shell-Guide.md) for interactive usage
2. Study the source code in `src/` for implementation details
3. Run the test suite to see more usage examples: `./amidb_tests`
4. Experiment with the demo scripts: `./amidb_shell RAM:demo.db showcase.sql`

Happy coding for the Amiga!
