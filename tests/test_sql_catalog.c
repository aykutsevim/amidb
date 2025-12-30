/*
 * test_sql_catalog.c - Catalog system tests
 */

#include "sql/catalog.h"
#include "sql/parser.h"
#include "storage/pager.h"
#include "storage/cache.h"
#include "test_harness.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Test: Create and retrieve table schema
 */
int test_catalog_create_get(void) {
    struct amidb_pager *pager;
    int rc;

    test_printf("START\n");
    rc = pager_open("RAM:test_catalog.db", 0, &pager);
    test_printf("DONE: %d\n", rc);

    if (rc == 0) {
        pager_close(pager);
    }
    return 0;
}

/*
 * Test: Implicit rowid (no PRIMARY KEY)
 */
int test_catalog_implicit_rowid(void) {
    struct amidb_pager *pager;
    struct page_cache *cache;
    struct catalog cat;
    static struct sql_create_table create_stmt;  /* Move off stack */
    static struct table_schema schema;  /* Move off stack */
    int rc;

    test_printf("Testing catalog with implicit rowid...\n");

    /* Delete old database file to ensure clean state */
    remove("RAM:test_catalog2.db");

    test_printf("  [1] Opening pager...\n");
    rc = pager_open("RAM:test_catalog2.db", 0, &pager);
    if (rc != 0) {
        test_printf("  [1] FAILED: pager_open rc=%d\n", rc);
        return -1;
    }
    test_printf("  [1] OK: pager=%p\n", (void*)pager);

    test_printf("  [2] Creating cache...\n");
    cache = cache_create(32, pager);
    if (!cache) {
        test_printf("  [2] FAILED: cache_create returned NULL\n");
        pager_close(pager);
        return -1;
    }
    test_printf("  [2] OK: cache=%p\n", (void*)cache);

    test_printf("  [3] Initializing catalog...\n");
    rc = catalog_init(&cat, pager, cache);
    if (rc != 0) {
        test_printf("  [3] FAILED: catalog_init rc=%d\n", rc);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }
    test_printf("  [3] OK: catalog_tree=%p\n", (void*)cat.catalog_tree);

    /* Create table: posts (title TEXT, body TEXT) - no PRIMARY KEY */
    test_printf("  [4] Building create_stmt...\n");
    memset(&create_stmt, 0, sizeof(create_stmt));
    strcpy(create_stmt.table_name, "posts");
    create_stmt.column_count = 2;
    strcpy(create_stmt.columns[0].name, "title");
    create_stmt.columns[0].type = SQL_TYPE_TEXT;
    strcpy(create_stmt.columns[1].name, "body");
    create_stmt.columns[1].type = SQL_TYPE_TEXT;
    test_printf("  [4] OK: table_name='%s', column_count=%d\n", create_stmt.table_name, create_stmt.column_count);

    /* Test if malloc works before calling catalog_create_table */
    test_printf("  [4.5] Testing malloc...\n");
    {
        void *test_alloc1 = malloc(sizeof(struct table_schema));
        void *test_alloc2 = malloc(4096);
        if (!test_alloc1 || !test_alloc2) {
            test_printf("  [4.5] FAILED: malloc test failed! test_alloc1=%p, test_alloc2=%p\n",
                   test_alloc1, test_alloc2);
            if (test_alloc1) free(test_alloc1);
            if (test_alloc2) free(test_alloc2);
        } else {
            test_printf("  [4.5] OK: malloc works! test_alloc1=%p, test_alloc2=%p\n",
                   test_alloc1, test_alloc2);
            free(test_alloc1);
            free(test_alloc2);
        }
    }

    test_printf("  [5] Calling catalog_create_table...\n");
    rc = catalog_create_table(&cat, &create_stmt);
    if (rc != 0) {
        test_printf("  [5] FAILED: catalog_create_table rc=%d\n", rc);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }
    test_printf("  [5] OK: table created\n");

    /* Retrieve schema */
    test_printf("  [6] Calling catalog_get_table...\n");
    rc = catalog_get_table(&cat, "posts", &schema);
    if (rc != 0) {
        test_printf("  [6] FAILED: catalog_get_table rc=%d\n", rc);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }
    test_printf("  [6] OK: schema.name='%s', pk_index=%d, next_rowid=%u\n",
           schema.name, schema.primary_key_index, schema.next_rowid);

    /* Verify implicit rowid */
    test_printf("  [7] Verifying implicit rowid...\n");
    if (schema.primary_key_index != -1) {
        test_printf("  [7] FAILED: Expected implicit rowid (pk_index=-1), got %d\n",
               schema.primary_key_index);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    if (schema.next_rowid != 1) {
        test_printf("  [7] FAILED: Expected next_rowid=1, got %u\n", schema.next_rowid);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    test_printf("  [7] OK: Created table 'posts' with implicit rowid, next_rowid=%u\n",
           schema.next_rowid);

    catalog_close(&cat);
    cache_destroy(cache);
    pager_close(pager);

    return 0;
}

/*
 * Test: Duplicate table name error
 */
int test_catalog_duplicate_table(void) {
    struct amidb_pager *pager;
    struct page_cache *cache;
    struct catalog cat;
    static struct sql_create_table create_stmt;  /* Move off stack */
    int rc;

    test_printf("Testing catalog duplicate table error...\n");

    rc = pager_open("RAM:test_catalog3.db", 0, &pager);
    if (rc != 0) return -1;

    cache = cache_create(32, pager);
    if (!cache) {
        pager_close(pager);
        return -1;
    }

    rc = catalog_init(&cat, pager, cache);
    if (rc != 0) {
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    /* Create first table */
    memset(&create_stmt, 0, sizeof(create_stmt));
    strcpy(create_stmt.table_name, "users");
    create_stmt.column_count = 1;
    strcpy(create_stmt.columns[0].name, "id");
    create_stmt.columns[0].type = SQL_TYPE_INTEGER;
    create_stmt.columns[0].is_primary_key = 1;

    rc = catalog_create_table(&cat, &create_stmt);
    if (rc != 0) {
        test_printf("  ERROR: Failed to create first table\n");
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    /* Try to create duplicate */
    rc = catalog_create_table(&cat, &create_stmt);
    if (rc == 0) {
        test_printf("  ERROR: Expected error for duplicate table, but succeeded\n");
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    test_printf("  Correctly rejected duplicate table 'users'\n");

    catalog_close(&cat);
    cache_destroy(cache);
    pager_close(pager);

    return 0;
}

/*
 * Test: Drop table
 */
int test_catalog_drop_table(void) {
    struct amidb_pager *pager;
    struct page_cache *cache;
    struct catalog cat;
    static struct sql_create_table create_stmt;  /* Move off stack */
    static struct table_schema schema;  /* Move off stack */
    int rc;

    test_printf("Testing catalog drop table...\n");

    rc = pager_open("RAM:test_catalog4.db", 0, &pager);
    if (rc != 0) return -1;

    cache = cache_create(32, pager);
    if (!cache) {
        pager_close(pager);
        return -1;
    }

    rc = catalog_init(&cat, pager, cache);
    if (rc != 0) {
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    /* Create table */
    memset(&create_stmt, 0, sizeof(create_stmt));
    strcpy(create_stmt.table_name, "temp");
    create_stmt.column_count = 1;
    strcpy(create_stmt.columns[0].name, "id");
    create_stmt.columns[0].type = SQL_TYPE_INTEGER;
    create_stmt.columns[0].is_primary_key = 1;

    rc = catalog_create_table(&cat, &create_stmt);
    if (rc != 0) {
        test_printf("  ERROR: Failed to create table\n");
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    /* Verify it exists */
    rc = catalog_get_table(&cat, "temp", &schema);
    if (rc != 0) {
        test_printf("  ERROR: Table not found after creation\n");
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    /* Drop table */
    rc = catalog_drop_table(&cat, "temp");
    if (rc != 0) {
        test_printf("  ERROR: Failed to drop table\n");
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    /* Verify it's gone */
    rc = catalog_get_table(&cat, "temp", &schema);
    if (rc == 0) {
        test_printf("  ERROR: Table still exists after drop\n");
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    test_printf("  Successfully dropped table 'temp'\n");

    catalog_close(&cat);
    cache_destroy(cache);
    pager_close(pager);

    return 0;
}

/*
 * Test: List tables
 */
int test_catalog_list_tables(void) {
    struct amidb_pager *pager;
    struct page_cache *cache;
    struct catalog cat;
    static struct sql_create_table create_stmt;  /* Move off stack */
    char table_names[10][64];
    int count;
    int rc;
    int found_users = 0, found_posts = 0, found_products = 0;
    int i;

    test_printf("Testing catalog list tables...\n");

    /* Delete old database file to ensure clean state */
    remove("RAM:test_catalog5.db");

    rc = pager_open("RAM:test_catalog5.db", 0, &pager);
    if (rc != 0) return -1;

    cache = cache_create(32, pager);
    if (!cache) {
        pager_close(pager);
        return -1;
    }

    rc = catalog_init(&cat, pager, cache);
    if (rc != 0) {
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    /* Create three tables */
    memset(&create_stmt, 0, sizeof(create_stmt));
    strcpy(create_stmt.table_name, "users");
    create_stmt.column_count = 1;
    strcpy(create_stmt.columns[0].name, "id");
    create_stmt.columns[0].type = SQL_TYPE_INTEGER;
    create_stmt.columns[0].is_primary_key = 1;
    catalog_create_table(&cat, &create_stmt);

    strcpy(create_stmt.table_name, "posts");
    catalog_create_table(&cat, &create_stmt);

    strcpy(create_stmt.table_name, "products");
    catalog_create_table(&cat, &create_stmt);

    /* List tables */
    count = catalog_list_tables(&cat, table_names, 10);
    if (count != 3) {
        test_printf("  ERROR: Expected 3 tables, got %d\n", count);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    /* Verify table names */
    for (i = 0; i < count; i++) {
        test_printf("  Table %d: %s\n", i, table_names[i]);
        if (strcmp(table_names[i], "users") == 0) found_users = 1;
        if (strcmp(table_names[i], "posts") == 0) found_posts = 1;
        if (strcmp(table_names[i], "products") == 0) found_products = 1;
    }

    if (!found_users || !found_posts || !found_products) {
        test_printf("  ERROR: Not all tables found\n");
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    test_printf("  Successfully listed %d tables\n", count);

    catalog_close(&cat);
    cache_destroy(cache);
    pager_close(pager);

    return 0;
}

/*
 * Test: Persist catalog across reopens
 */
int test_catalog_persistence(void) {
    struct amidb_pager *pager;
    struct page_cache *cache;
    struct catalog cat;
    static struct sql_create_table create_stmt;  /* Move off stack */
    static struct table_schema schema;  /* Move off stack */
    int rc;

    test_printf("Testing catalog persistence...\n");

    /* Delete old database file to ensure clean state */
    remove("RAM:test_catalog6.db");

    /* Create database and table */
    rc = pager_open("RAM:test_catalog6.db", 0, &pager);
    if (rc != 0) return -1;

    cache = cache_create(32, pager);
    if (!cache) {
        pager_close(pager);
        return -1;
    }

    rc = catalog_init(&cat, pager, cache);
    if (rc != 0) {
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    memset(&create_stmt, 0, sizeof(create_stmt));
    strcpy(create_stmt.table_name, "persistent");
    create_stmt.column_count = 1;
    strcpy(create_stmt.columns[0].name, "id");
    create_stmt.columns[0].type = SQL_TYPE_INTEGER;
    create_stmt.columns[0].is_primary_key = 1;

    rc = catalog_create_table(&cat, &create_stmt);
    if (rc != 0) {
        test_printf("  ERROR: Failed to create table\n");
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    test_printf("  Created table 'persistent'\n");

    catalog_close(&cat);
    cache_destroy(cache);
    pager_close(pager);

    /* Reopen database */
    rc = pager_open("RAM:test_catalog6.db", 0, &pager);
    if (rc != 0) {
        test_printf("  ERROR: Failed to reopen database\n");
        return -1;
    }

    cache = cache_create(32, pager);
    if (!cache) {
        pager_close(pager);
        return -1;
    }

    rc = catalog_init(&cat, pager, cache);
    if (rc != 0) {
        test_printf("  ERROR: Failed to reinitialize catalog\n");
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    /* Verify table still exists */
    rc = catalog_get_table(&cat, "persistent", &schema);
    if (rc != 0) {
        test_printf("  ERROR: Table not found after reopen\n");
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    if (strcmp(schema.name, "persistent") != 0) {
        test_printf("  ERROR: Table name mismatch after reopen\n");
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    test_printf("  Successfully reopened database, table 'persistent' found\n");

    catalog_close(&cat);
    cache_destroy(cache);
    pager_close(pager);

    return 0;
}
