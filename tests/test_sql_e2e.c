/*
 * test_sql_e2e.c - End-to-end SQL tests
 *
 * Tests the complete SQL stack: Lexer → Parser → Executor → Catalog → B+Tree
 */

#include "sql/executor.h"
#include "sql/parser.h"
#include "sql/lexer.h"
#include "sql/catalog.h"
#include "storage/pager.h"
#include "storage/cache.h"
#include "test_harness.h"
#include <stdio.h>
#include <string.h>

/*
 * Test: CREATE TABLE end-to-end (explicit PRIMARY KEY)
 */
int test_e2e_create_table_explicit_pk(void) {
    struct amidb_pager *pager;
    struct page_cache *cache;
    struct catalog cat;
    struct sql_executor exec;
    struct sql_lexer lex;
    struct sql_parser parser;
    struct sql_statement stmt;
    static struct table_schema schema;  /* Move off stack */
    const char *sql = "CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT, email TEXT)";
    int rc;

    test_printf("Testing E2E: CREATE TABLE with explicit PRIMARY KEY...\n");

    /* Delete old database file to ensure clean state */
    remove("RAM:test_e2e1.db");

    /* Initialize database */
    rc = pager_open("RAM:test_e2e1.db", 0, &pager);
    if (rc != 0) {
        test_printf("  ERROR: Failed to open pager\n");
        return -1;
    }

    cache = cache_create(32, pager);
    if (!cache) {
        test_printf("  ERROR: Failed to create cache\n");
        pager_close(pager);
        return -1;
    }

    rc = catalog_init(&cat, pager, cache);
    if (rc != 0) {
        test_printf("  ERROR: Failed to initialize catalog\n");
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    rc = executor_init(&exec, pager, cache, &cat);
    if (rc != 0) {
        test_printf("  ERROR: Failed to initialize executor\n");
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    /* Parse SQL */
    test_printf("  SQL: %s\n", sql);
    lexer_init(&lex, sql);
    parser_init(&parser, &lex);

    rc = parser_parse_statement(&parser, &stmt);
    if (rc != 0) {
        test_printf("  ERROR: Parse failed: %s\n", parser_get_error(&parser));
        executor_close(&exec);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    /* Execute CREATE TABLE */
    rc = executor_execute(&exec, &stmt);
    if (rc != 0) {
        test_printf("  ERROR: Execute failed: %s\n", executor_get_error(&exec));
        executor_close(&exec);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    test_printf("  ✓ CREATE TABLE executed successfully\n");

    /* Verify table exists in catalog */
    rc = catalog_get_table(&cat, "users", &schema);
    if (rc != 0) {
        test_printf("  ERROR: Table 'users' not found in catalog\n");
        executor_close(&exec);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    /* Verify schema */
    if (strcmp(schema.name, "users") != 0) {
        test_printf("  ERROR: Table name mismatch\n");
        return -1;
    }

    if (schema.column_count != 3) {
        test_printf("  ERROR: Expected 3 columns, got %d\n", schema.column_count);
        return -1;
    }

    if (schema.primary_key_index != 0) {
        test_printf("  ERROR: Expected PRIMARY KEY at index 0, got %d\n", schema.primary_key_index);
        return -1;
    }

    if (schema.btree_root == 0) {
        test_printf("  ERROR: Table B+Tree not allocated\n");
        return -1;
    }

    test_printf("  ✓ Table schema verified: name='%s', columns=%d, pk_index=%d, btree_root=%u\n",
           schema.name, schema.column_count, schema.primary_key_index, schema.btree_root);

    executor_close(&exec);
    catalog_close(&cat);
    cache_destroy(cache);
    pager_close(pager);

    return 0;
}

/*
 * Test: CREATE TABLE end-to-end (implicit rowid)
 */
int test_e2e_create_table_implicit_rowid(void) {
    struct amidb_pager *pager;
    struct page_cache *cache;
    struct catalog cat;
    struct sql_executor exec;
    struct sql_lexer lex;
    struct sql_parser parser;
    struct sql_statement stmt;
    static struct table_schema schema;  /* Move off stack */
    const char *sql = "CREATE TABLE posts (title TEXT, body TEXT, published INTEGER)";
    int rc;

    test_printf("Testing E2E: CREATE TABLE with implicit rowid...\n");

    rc = pager_open("RAM:test_e2e2.db", 0, &pager);
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

    rc = executor_init(&exec, pager, cache, &cat);
    if (rc != 0) {
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    /* Parse and execute */
    test_printf("  SQL: %s\n", sql);
    lexer_init(&lex, sql);
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);

    rc = executor_execute(&exec, &stmt);
    if (rc != 0) {
        test_printf("  ERROR: Execute failed: %s\n", executor_get_error(&exec));
        executor_close(&exec);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    test_printf("  ✓ CREATE TABLE executed successfully\n");

    /* Verify implicit rowid */
    catalog_get_table(&cat, "posts", &schema);

    if (schema.primary_key_index != -1) {
        test_printf("  ERROR: Expected implicit rowid (pk_index=-1), got %d\n",
               schema.primary_key_index);
        return -1;
    }

    if (schema.next_rowid != 1) {
        test_printf("  ERROR: Expected next_rowid=1, got %u\n", schema.next_rowid);
        return -1;
    }

    test_printf("  ✓ Implicit rowid verified: pk_index=-1, next_rowid=%u\n", schema.next_rowid);

    executor_close(&exec);
    catalog_close(&cat);
    cache_destroy(cache);
    pager_close(pager);

    return 0;
}

/*
 * Test: CREATE TABLE validation errors
 */
int test_e2e_create_table_validation(void) {
    struct amidb_pager *pager;
    struct page_cache *cache;
    struct catalog cat;
    struct sql_executor exec;
    struct sql_lexer lex;
    struct sql_parser parser;
    struct sql_statement stmt;
    int rc;

    test_printf("Testing E2E: CREATE TABLE validation errors...\n");

    rc = pager_open("RAM:test_e2e3.db", 0, &pager);
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

    rc = executor_init(&exec, pager, cache, &cat);
    if (rc != 0) {
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    /* Test 1: Duplicate table name */
    test_printf("  Test: Duplicate table name...\n");
    lexer_init(&lex, "CREATE TABLE users (id INTEGER PRIMARY KEY)");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);  /* First CREATE should succeed */

    lexer_init(&lex, "CREATE TABLE users (id INTEGER PRIMARY KEY)");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    rc = executor_execute(&exec, &stmt);  /* Second should fail */

    if (rc == 0) {
        test_printf("    ERROR: Expected error for duplicate table\n");
        executor_close(&exec);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    test_printf("    ✓ Correctly rejected: %s\n", executor_get_error(&exec));

    /* Test 2: TEXT PRIMARY KEY (should fail) */
    test_printf("  Test: TEXT PRIMARY KEY...\n");
    lexer_init(&lex, "CREATE TABLE invalid (name TEXT PRIMARY KEY)");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    rc = executor_execute(&exec, &stmt);

    if (rc == 0) {
        test_printf("    ERROR: Expected error for TEXT PRIMARY KEY\n");
        executor_close(&exec);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    test_printf("    ✓ Correctly rejected: %s\n", executor_get_error(&exec));

    executor_close(&exec);
    catalog_close(&cat);
    cache_destroy(cache);
    pager_close(pager);

    return 0;
}

/*
 * Test: Multiple tables in same database
 */
int test_e2e_multiple_tables(void) {
    struct amidb_pager *pager;
    struct page_cache *cache;
    struct catalog cat;
    struct sql_executor exec;
    struct sql_lexer lex;
    struct sql_parser parser;
    struct sql_statement stmt;
    char table_names[10][64];
    int count;
    int rc;

    test_printf("Testing E2E: Multiple tables in same database...\n");

    rc = pager_open("RAM:test_e2e4.db", 0, &pager);
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

    rc = executor_init(&exec, pager, cache, &cat);
    if (rc != 0) {
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    /* Create table 1 */
    test_printf("  Creating table 'users'...\n");
    lexer_init(&lex, "CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT)");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    /* Create table 2 */
    test_printf("  Creating table 'posts'...\n");
    lexer_init(&lex, "CREATE TABLE posts (post_id INTEGER PRIMARY KEY, title TEXT)");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    /* Create table 3 */
    test_printf("  Creating table 'comments'...\n");
    lexer_init(&lex, "CREATE TABLE comments (body TEXT)");  /* Implicit rowid */
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    /* List all tables */
    count = catalog_list_tables(&cat, table_names, 10);
    if (count != 3) {
        test_printf("  ERROR: Expected 3 tables, got %d\n", count);
        executor_close(&exec);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    test_printf("  ✓ Created 3 tables successfully\n");
    test_printf("  Tables: %s, %s, %s\n", table_names[0], table_names[1], table_names[2]);

    executor_close(&exec);
    catalog_close(&cat);
    cache_destroy(cache);
    pager_close(pager);

    return 0;
}

/*
 * Test: INSERT with explicit PRIMARY KEY
 */
int test_e2e_insert_explicit_pk(void) {
    struct amidb_pager *pager;
    struct page_cache *cache;
    struct catalog cat;
    struct sql_executor exec;
    struct sql_lexer lex;
    struct sql_parser parser;
    struct sql_statement stmt;
    int rc;

    test_printf("Testing E2E: INSERT with explicit PRIMARY KEY...\n");

    rc = pager_open("RAM:test_insert1.db", 0, &pager);
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

    rc = executor_init(&exec, pager, cache, &cat);
    if (rc != 0) {
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    /* Create table */
    test_printf("  Creating table 'users'...\n");
    lexer_init(&lex, "CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT, email TEXT)");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    rc = executor_execute(&exec, &stmt);
    if (rc != 0) {
        test_printf("  ERROR: CREATE TABLE failed: %s\n", executor_get_error(&exec));
        executor_close(&exec);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    /* Insert row */
    test_printf("  Inserting row (1, 'Alice', 'alice@example.com')...\n");
    lexer_init(&lex, "INSERT INTO users VALUES (1, 'Alice', 'alice@example.com')");
    parser_init(&parser, &lex);
    rc = parser_parse_statement(&parser, &stmt);
    if (rc != 0) {
        test_printf("  ERROR: Parse failed: %s\n", parser_get_error(&parser));
        executor_close(&exec);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    rc = executor_execute(&exec, &stmt);
    if (rc != 0) {
        test_printf("  ERROR: INSERT failed: %s\n", executor_get_error(&exec));
        executor_close(&exec);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    test_printf("  ✓ INSERT successful\n");

    executor_close(&exec);
    catalog_close(&cat);
    cache_destroy(cache);
    pager_close(pager);

    return 0;
}

/*
 * Test: INSERT with implicit rowid
 */
int test_e2e_insert_implicit_rowid(void) {
    struct amidb_pager *pager;
    struct page_cache *cache;
    struct catalog cat;
    struct sql_executor exec;
    struct sql_lexer lex;
    struct sql_parser parser;
    struct sql_statement stmt;
    static struct table_schema schema;  /* Move off stack */
    int rc;

    test_printf("Testing E2E: INSERT with implicit rowid...\n");

    /* Delete old database file to ensure clean state */
    remove("RAM:test_insert2.db");

    rc = pager_open("RAM:test_insert2.db", 0, &pager);
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

    rc = executor_init(&exec, pager, cache, &cat);
    if (rc != 0) {
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    /* Create table */
    test_printf("  Creating table 'posts' (implicit rowid)...\n");
    lexer_init(&lex, "CREATE TABLE posts (title TEXT, body TEXT)");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    /* Insert first row */
    test_printf("  Inserting row 1...\n");
    lexer_init(&lex, "INSERT INTO posts VALUES ('First Post', 'Hello World')");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    rc = executor_execute(&exec, &stmt);
    if (rc != 0) {
        test_printf("  ERROR: INSERT failed: %s\n", executor_get_error(&exec));
        executor_close(&exec);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    /* Insert second row */
    test_printf("  Inserting row 2...\n");
    lexer_init(&lex, "INSERT INTO posts VALUES ('Second Post', 'Another day')");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    rc = executor_execute(&exec, &stmt);
    if (rc != 0) {
        test_printf("  ERROR: INSERT failed: %s\n", executor_get_error(&exec));
        executor_close(&exec);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    /* Verify next_rowid incremented */
    catalog_get_table(&cat, "posts", &schema);
    if (schema.next_rowid != 3) {
        test_printf("  ERROR: Expected next_rowid=3, got %u\n", schema.next_rowid);
        executor_close(&exec);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    test_printf("  ✓ Implicit rowid auto-increment working (next_rowid=%u)\n", schema.next_rowid);

    executor_close(&exec);
    catalog_close(&cat);
    cache_destroy(cache);
    pager_close(pager);

    return 0;
}

/*
 * Test: INSERT validation errors
 */
int test_e2e_insert_validation(void) {
    struct amidb_pager *pager;
    struct page_cache *cache;
    struct catalog cat;
    struct sql_executor exec;
    struct sql_lexer lex;
    struct sql_parser parser;
    struct sql_statement stmt;
    int rc;

    test_printf("Testing E2E: INSERT validation errors...\n");

    /* Delete old database file to ensure clean state */
    remove("RAM:test_insert3.db");

    rc = pager_open("RAM:test_insert3.db", 0, &pager);
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

    rc = executor_init(&exec, pager, cache, &cat);
    if (rc != 0) {
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    /* Create table */
    lexer_init(&lex, "CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT)");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    /* Test 1: Wrong value count */
    test_printf("  Test: Wrong value count...\n");
    lexer_init(&lex, "INSERT INTO users VALUES (1)");  /* Missing name */
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    rc = executor_execute(&exec, &stmt);
    if (rc == 0) {
        test_printf("    ERROR: Expected error for wrong value count\n");
        executor_close(&exec);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }
    test_printf("    ✓ Correctly rejected: %s\n", executor_get_error(&exec));

    /* Test 2: Type mismatch */
    test_printf("  Test: Type mismatch...\n");
    lexer_init(&lex, "INSERT INTO users VALUES ('not_an_int', 'Alice')");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    rc = executor_execute(&exec, &stmt);
    if (rc == 0) {
        test_printf("    ERROR: Expected error for type mismatch\n");
        executor_close(&exec);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }
    test_printf("    ✓ Correctly rejected: %s\n", executor_get_error(&exec));

    /* Test 3: Duplicate PRIMARY KEY */
    test_printf("  Test: Duplicate PRIMARY KEY...\n");
    lexer_init(&lex, "INSERT INTO users VALUES (1, 'Alice')");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);  /* First insert succeeds */

    lexer_init(&lex, "INSERT INTO users VALUES (1, 'Bob')");  /* Duplicate ID */
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    rc = executor_execute(&exec, &stmt);
    if (rc == 0) {
        test_printf("    ERROR: Expected error for duplicate PRIMARY KEY\n");
        executor_close(&exec);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }
    test_printf("    ✓ Correctly rejected: %s\n", executor_get_error(&exec));

    executor_close(&exec);
    catalog_close(&cat);
    cache_destroy(cache);
    pager_close(pager);

    return 0;
}
/*
 * Test: SELECT * FROM table (no WHERE)
 */
int test_e2e_select_all(void) {
    struct amidb_pager *pager;
    struct page_cache *cache;
    struct catalog cat;
    struct sql_executor exec;
    struct sql_lexer lex;
    struct sql_parser parser;
    struct sql_statement stmt;
    int rc;

    test_printf("Testing E2E: SELECT * FROM table...\n");

    rc = pager_open("RAM:test_select1.db", 0, &pager);
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

    rc = executor_init(&exec, pager, cache, &cat);
    if (rc != 0) {
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    /* Create table */
    test_printf("  Creating table and inserting data...\n");
    lexer_init(&lex, "CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT)");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    /* Insert test data */
    lexer_init(&lex, "INSERT INTO users VALUES (1, 'Alice')");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    lexer_init(&lex, "INSERT INTO users VALUES (2, 'Bob')");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    lexer_init(&lex, "INSERT INTO users VALUES (3, 'Charlie')");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    /* SELECT * FROM users */
    test_printf("  Executing SELECT * FROM users...\n");
    lexer_init(&lex, "SELECT * FROM users");
    parser_init(&parser, &lex);
    rc = parser_parse_statement(&parser, &stmt);
    if (rc != 0) {
        test_printf("  ERROR: Parse failed: %s\n", parser_get_error(&parser));
        executor_close(&exec);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    rc = executor_execute(&exec, &stmt);
    if (rc != 0) {
        test_printf("  ERROR: SELECT failed: %s\n", executor_get_error(&exec));
        executor_close(&exec);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    test_printf("  ✓ SELECT * completed successfully\n");

    executor_close(&exec);
    catalog_close(&cat);
    cache_destroy(cache);
    pager_close(pager);

    return 0;
}

/*
 * Test: SELECT with WHERE on PRIMARY KEY (fast path)
 */
int test_e2e_select_where_pk(void) {
    struct amidb_pager *pager;
    struct page_cache *cache;
    struct catalog cat;
    struct sql_executor exec;
    struct sql_lexer lex;
    struct sql_parser parser;
    struct sql_statement stmt;
    int rc;

    test_printf("Testing E2E: SELECT with WHERE on PRIMARY KEY...\n");

    rc = pager_open("RAM:test_select2.db", 0, &pager);
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

    rc = executor_init(&exec, pager, cache, &cat);
    if (rc != 0) {
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    /* Create table and insert data */
    test_printf("  Creating table and inserting data...\n");
    lexer_init(&lex, "CREATE TABLE products (id INTEGER PRIMARY KEY, name TEXT, price INTEGER)");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    lexer_init(&lex, "INSERT INTO products VALUES (100, 'Widget', 50)");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    lexer_init(&lex, "INSERT INTO products VALUES (200, 'Gadget', 75)");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    lexer_init(&lex, "INSERT INTO products VALUES (300, 'Gizmo', 100)");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    /* SELECT with WHERE on PRIMARY KEY */
    test_printf("  Executing SELECT * FROM products WHERE id = 200...\n");
    lexer_init(&lex, "SELECT * FROM products WHERE id = 200");
    parser_init(&parser, &lex);
    rc = parser_parse_statement(&parser, &stmt);
    if (rc != 0) {
        test_printf("  ERROR: Parse failed: %s\n", parser_get_error(&parser));
        executor_close(&exec);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    rc = executor_execute(&exec, &stmt);
    if (rc != 0) {
        test_printf("  ERROR: SELECT failed: %s\n", executor_get_error(&exec));
        executor_close(&exec);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    test_printf("  ✓ SELECT with WHERE on PRIMARY KEY completed (fast path)\n");

    executor_close(&exec);
    catalog_close(&cat);
    cache_destroy(cache);
    pager_close(pager);

    return 0;
}

/*
 * Test: SELECT with WHERE on non-PRIMARY KEY column (table scan)
 */
int test_e2e_select_where_nonpk(void) {
    struct amidb_pager *pager;
    struct page_cache *cache;
    struct catalog cat;
    struct sql_executor exec;
    struct sql_lexer lex;
    struct sql_parser parser;
    struct sql_statement stmt;
    int rc;

    test_printf("Testing E2E: SELECT with WHERE on non-PK column...\n");

    rc = pager_open("RAM:test_select3.db", 0, &pager);
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

    rc = executor_init(&exec, pager, cache, &cat);
    if (rc != 0) {
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    /* Create table and insert data */
    test_printf("  Creating table and inserting data...\n");
    lexer_init(&lex, "CREATE TABLE employees (id INTEGER PRIMARY KEY, name TEXT, dept TEXT)");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    lexer_init(&lex, "INSERT INTO employees VALUES (1, 'Alice', 'Engineering')");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    lexer_init(&lex, "INSERT INTO employees VALUES (2, 'Bob', 'Sales')");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    lexer_init(&lex, "INSERT INTO employees VALUES (3, 'Charlie', 'Engineering')");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    /* SELECT with WHERE on non-PRIMARY KEY column */
    test_printf("  Executing SELECT * FROM employees WHERE dept = 'Engineering'...\n");
    lexer_init(&lex, "SELECT * FROM employees WHERE dept = 'Engineering'");
    parser_init(&parser, &lex);
    rc = parser_parse_statement(&parser, &stmt);
    if (rc != 0) {
        test_printf("  ERROR: Parse failed: %s\n", parser_get_error(&parser));
        executor_close(&exec);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    rc = executor_execute(&exec, &stmt);
    if (rc != 0) {
        test_printf("  ERROR: SELECT failed: %s\n", executor_get_error(&exec));
        executor_close(&exec);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    test_printf("  ✓ SELECT with WHERE on non-PK column completed (table scan)\n");

    executor_close(&exec);
    catalog_close(&cat);
    cache_destroy(cache);
    pager_close(pager);

    return 0;
}

/*
 * Test: SELECT with no matching rows
 */
int test_e2e_select_no_match(void) {
    struct amidb_pager *pager;
    struct page_cache *cache;
    struct catalog cat;
    struct sql_executor exec;
    struct sql_lexer lex;
    struct sql_parser parser;
    struct sql_statement stmt;
    int rc;

    test_printf("Testing E2E: SELECT with no matching rows...\n");

    rc = pager_open("RAM:test_select4.db", 0, &pager);
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

    rc = executor_init(&exec, pager, cache, &cat);
    if (rc != 0) {
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    /* Create table and insert data */
    test_printf("  Creating table with limited data...\n");
    lexer_init(&lex, "CREATE TABLE items (id INTEGER PRIMARY KEY, name TEXT)");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    lexer_init(&lex, "INSERT INTO items VALUES (1, 'Item1')");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    /* SELECT with WHERE that matches nothing */
    test_printf("  Executing SELECT * FROM items WHERE id = 999...\n");
    lexer_init(&lex, "SELECT * FROM items WHERE id = 999");
    parser_init(&parser, &lex);
    rc = parser_parse_statement(&parser, &stmt);
    if (rc != 0) {
        test_printf("  ERROR: Parse failed: %s\n", parser_get_error(&parser));
        executor_close(&exec);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    rc = executor_execute(&exec, &stmt);
    if (rc != 0) {
        test_printf("  ERROR: SELECT failed: %s\n", executor_get_error(&exec));
        executor_close(&exec);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    test_printf("  ✓ SELECT with no matches completed (0 rows returned)\n");

    executor_close(&exec);
    catalog_close(&cat);
    cache_destroy(cache);
    pager_close(pager);

    return 0;
}
/*
 * Test: ORDER BY PRIMARY KEY ASC (fast path)
 */
int test_e2e_order_by_pk_asc(void) {
    struct amidb_pager *pager;
    struct page_cache *cache;
    struct catalog cat;
    struct sql_executor exec;
    struct sql_lexer lex;
    struct sql_parser parser;
    struct sql_statement stmt;
    int rc;

    test_printf("Testing E2E: ORDER BY PRIMARY KEY ASC...\n");

    rc = pager_open("RAM:test_order1.db", 0, &pager);
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

    rc = executor_init(&exec, pager, cache, &cat);
    if (rc != 0) {
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    /* Create table and insert data out of order */
    test_printf("  Creating table and inserting data...\n");
    lexer_init(&lex, "CREATE TABLE nums (id INTEGER PRIMARY KEY, value INTEGER)");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    lexer_init(&lex, "INSERT INTO nums VALUES (3, 30)");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    lexer_init(&lex, "INSERT INTO nums VALUES (1, 10)");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    lexer_init(&lex, "INSERT INTO nums VALUES (5, 50)");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    lexer_init(&lex, "INSERT INTO nums VALUES (2, 20)");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    /* SELECT with ORDER BY id ASC (fast path - already sorted) */
    test_printf("  Executing SELECT * FROM nums ORDER BY id ASC...\n");
    lexer_init(&lex, "SELECT * FROM nums ORDER BY id ASC");
    parser_init(&parser, &lex);
    rc = parser_parse_statement(&parser, &stmt);
    if (rc != 0) {
        test_printf("  ERROR: Parse failed: %s\n", parser_get_error(&parser));
        executor_close(&exec);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    rc = executor_execute(&exec, &stmt);
    if (rc != 0) {
        test_printf("  ERROR: SELECT failed: %s\n", executor_get_error(&exec));
        executor_close(&exec);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    test_printf("  ✓ ORDER BY PRIMARY KEY ASC completed (fast path)\n");

    executor_close(&exec);
    catalog_close(&cat);
    cache_destroy(cache);
    pager_close(pager);

    return 0;
}

/*
 * Test: ORDER BY non-PRIMARY KEY column
 */
int test_e2e_order_by_nonpk(void) {
    struct amidb_pager *pager;
    struct page_cache *cache;
    struct catalog cat;
    struct sql_executor exec;
    struct sql_lexer lex;
    struct sql_parser parser;
    struct sql_statement stmt;
    int rc;

    test_printf("Testing E2E: ORDER BY non-PK column...\n");

    rc = pager_open("RAM:test_order2.db", 0, &pager);
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

    rc = executor_init(&exec, pager, cache, &cat);
    if (rc != 0) {
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    /* Create table */
    test_printf("  Creating table with scores...\n");
    lexer_init(&lex, "CREATE TABLE scores (id INTEGER PRIMARY KEY, name TEXT, score INTEGER)");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    lexer_init(&lex, "INSERT INTO scores VALUES (1, 'Alice', 85)");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    lexer_init(&lex, "INSERT INTO scores VALUES (2, 'Bob', 92)");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    lexer_init(&lex, "INSERT INTO scores VALUES (3, 'Charlie', 78)");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    lexer_init(&lex, "INSERT INTO scores VALUES (4, 'Diana', 95)");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    /* ORDER BY score DESC */
    test_printf("  Executing SELECT * FROM scores ORDER BY score DESC...\n");
    lexer_init(&lex, "SELECT * FROM scores ORDER BY score DESC");
    parser_init(&parser, &lex);
    rc = parser_parse_statement(&parser, &stmt);
    if (rc != 0) {
        test_printf("  ERROR: Parse failed: %s\n", parser_get_error(&parser));
        executor_close(&exec);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    rc = executor_execute(&exec, &stmt);
    if (rc != 0) {
        test_printf("  ERROR: SELECT failed: %s\n", executor_get_error(&exec));
        executor_close(&exec);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    test_printf("  ✓ ORDER BY non-PK column DESC completed (in-memory sort)\n");

    executor_close(&exec);
    catalog_close(&cat);
    cache_destroy(cache);
    pager_close(pager);

    return 0;
}

/*
 * Test: LIMIT without ORDER BY
 */
int test_e2e_limit_only(void) {
    struct amidb_pager *pager;
    struct page_cache *cache;
    struct catalog cat;
    struct sql_executor exec;
    struct sql_lexer lex;
    struct sql_parser parser;
    struct sql_statement stmt;
    int rc;

    test_printf("Testing E2E: LIMIT without ORDER BY...\n");

    rc = pager_open("RAM:test_limit1.db", 0, &pager);
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

    rc = executor_init(&exec, pager, cache, &cat);
    if (rc != 0) {
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    /* Create table */
    test_printf("  Creating table with 10 rows...\n");
    lexer_init(&lex, "CREATE TABLE items (id INTEGER PRIMARY KEY, name TEXT)");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    /* Insert 10 rows */
    lexer_init(&lex, "INSERT INTO items VALUES (1, 'Item1')");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    lexer_init(&lex, "INSERT INTO items VALUES (2, 'Item2')");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    lexer_init(&lex, "INSERT INTO items VALUES (3, 'Item3')");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    lexer_init(&lex, "INSERT INTO items VALUES (4, 'Item4')");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    lexer_init(&lex, "INSERT INTO items VALUES (5, 'Item5')");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    /* SELECT with LIMIT 3 */
    test_printf("  Executing SELECT * FROM items LIMIT 3...\n");
    lexer_init(&lex, "SELECT * FROM items LIMIT 3");
    parser_init(&parser, &lex);
    rc = parser_parse_statement(&parser, &stmt);
    if (rc != 0) {
        test_printf("  ERROR: Parse failed: %s\n", parser_get_error(&parser));
        executor_close(&exec);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    rc = executor_execute(&exec, &stmt);
    if (rc != 0) {
        test_printf("  ERROR: SELECT failed: %s\n", executor_get_error(&exec));
        executor_close(&exec);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    test_printf("  ✓ LIMIT completed (should show 3 rows)\n");

    executor_close(&exec);
    catalog_close(&cat);
    cache_destroy(cache);
    pager_close(pager);

    return 0;
}

/*
 * Test: ORDER BY with LIMIT
 */
int test_e2e_order_limit_combined(void) {
    struct amidb_pager *pager;
    struct page_cache *cache;
    struct catalog cat;
    struct sql_executor exec;
    struct sql_lexer lex;
    struct sql_parser parser;
    struct sql_statement stmt;
    int rc;

    test_printf("Testing E2E: ORDER BY with LIMIT...\n");

    rc = pager_open("RAM:test_order_limit.db", 0, &pager);
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

    rc = executor_init(&exec, pager, cache, &cat);
    if (rc != 0) {
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    /* Create table */
    test_printf("  Creating table with rankings...\n");
    lexer_init(&lex, "CREATE TABLE ranks (id INTEGER PRIMARY KEY, player TEXT, points INTEGER)");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    lexer_init(&lex, "INSERT INTO ranks VALUES (1, 'Alice', 100)");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    lexer_init(&lex, "INSERT INTO ranks VALUES (2, 'Bob', 250)");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    lexer_init(&lex, "INSERT INTO ranks VALUES (3, 'Charlie', 150)");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    lexer_init(&lex, "INSERT INTO ranks VALUES (4, 'Diana', 300)");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    lexer_init(&lex, "INSERT INTO ranks VALUES (5, 'Eve', 200)");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    /* SELECT top 3 by points */
    test_printf("  Executing SELECT * FROM ranks ORDER BY points DESC LIMIT 3...\n");
    lexer_init(&lex, "SELECT * FROM ranks ORDER BY points DESC LIMIT 3");
    parser_init(&parser, &lex);
    rc = parser_parse_statement(&parser, &stmt);
    if (rc != 0) {
        test_printf("  ERROR: Parse failed: %s\n", parser_get_error(&parser));
        executor_close(&exec);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    rc = executor_execute(&exec, &stmt);
    if (rc != 0) {
        test_printf("  ERROR: SELECT failed: %s\n", executor_get_error(&exec));
        executor_close(&exec);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    test_printf("  ✓ ORDER BY with LIMIT completed (top 3 players)\n");

    executor_close(&exec);
    catalog_close(&cat);
    cache_destroy(cache);
    pager_close(pager);

    return 0;
}

/*
 * Test: DROP TABLE basic functionality
 */
int test_e2e_drop_table_basic(void) {
    struct amidb_pager *pager;
    struct page_cache *cache;
    struct catalog cat;
    struct sql_executor exec;
    struct sql_lexer lex;
    struct sql_parser parser;
    struct sql_statement stmt;
    static struct table_schema schema;
    char table_names[10][64];
    int count;
    int rc;

    test_printf("Testing E2E: DROP TABLE basic...\n");

    /* Delete old database file to ensure clean state */
    remove("RAM:test_drop1.db");

    rc = pager_open("RAM:test_drop1.db", 0, &pager);
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

    rc = executor_init(&exec, pager, cache, &cat);
    if (rc != 0) {
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    /* Create table */
    test_printf("  Creating table 'users'...\n");
    lexer_init(&lex, "CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT)");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    rc = executor_execute(&exec, &stmt);
    if (rc != 0) {
        test_printf("  ERROR: CREATE TABLE failed: %s\n", executor_get_error(&exec));
        executor_close(&exec);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    /* Verify table exists */
    rc = catalog_get_table(&cat, "users", &schema);
    if (rc != 0) {
        test_printf("  ERROR: Table 'users' not found after CREATE\n");
        executor_close(&exec);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }
    test_printf("  ✓ Table 'users' created successfully\n");

    /* Drop table */
    test_printf("  Dropping table 'users'...\n");
    lexer_init(&lex, "DROP TABLE users");
    parser_init(&parser, &lex);
    rc = parser_parse_statement(&parser, &stmt);
    if (rc != 0) {
        test_printf("  ERROR: Parse failed: %s\n", parser_get_error(&parser));
        executor_close(&exec);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    rc = executor_execute(&exec, &stmt);
    if (rc != 0) {
        test_printf("  ERROR: DROP TABLE failed: %s\n", executor_get_error(&exec));
        executor_close(&exec);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    /* Verify table no longer exists */
    rc = catalog_get_table(&cat, "users", &schema);
    if (rc == 0) {
        test_printf("  ERROR: Table 'users' still exists after DROP\n");
        executor_close(&exec);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    /* Verify table list is empty */
    count = catalog_list_tables(&cat, table_names, 10);
    if (count != 0) {
        test_printf("  ERROR: Expected 0 tables, got %d\n", count);
        executor_close(&exec);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    test_printf("  ✓ Table 'users' dropped successfully\n");

    executor_close(&exec);
    catalog_close(&cat);
    cache_destroy(cache);
    pager_close(pager);

    return 0;
}

/*
 * Test: DROP TABLE non-existent table (should fail)
 */
int test_e2e_drop_table_nonexistent(void) {
    struct amidb_pager *pager;
    struct page_cache *cache;
    struct catalog cat;
    struct sql_executor exec;
    struct sql_lexer lex;
    struct sql_parser parser;
    struct sql_statement stmt;
    int rc;

    test_printf("Testing E2E: DROP TABLE non-existent...\n");

    /* Delete old database file to ensure clean state */
    remove("RAM:test_drop2.db");

    rc = pager_open("RAM:test_drop2.db", 0, &pager);
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

    rc = executor_init(&exec, pager, cache, &cat);
    if (rc != 0) {
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    /* Try to drop non-existent table */
    test_printf("  Attempting to drop non-existent table 'fake_table'...\n");
    lexer_init(&lex, "DROP TABLE fake_table");
    parser_init(&parser, &lex);
    rc = parser_parse_statement(&parser, &stmt);
    if (rc != 0) {
        test_printf("  ERROR: Parse failed: %s\n", parser_get_error(&parser));
        executor_close(&exec);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    rc = executor_execute(&exec, &stmt);
    if (rc == 0) {
        test_printf("  ERROR: DROP TABLE should have failed for non-existent table\n");
        executor_close(&exec);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    test_printf("  ✓ Correctly rejected: %s\n", executor_get_error(&exec));

    executor_close(&exec);
    catalog_close(&cat);
    cache_destroy(cache);
    pager_close(pager);

    return 0;
}

/*
 * Test: DROP TABLE then recreate
 */
int test_e2e_drop_table_recreate(void) {
    struct amidb_pager *pager;
    struct page_cache *cache;
    struct catalog cat;
    struct sql_executor exec;
    struct sql_lexer lex;
    struct sql_parser parser;
    struct sql_statement stmt;
    static struct table_schema schema;
    int rc;

    test_printf("Testing E2E: DROP TABLE then recreate...\n");

    /* Delete old database file to ensure clean state */
    remove("RAM:test_drop3.db");

    rc = pager_open("RAM:test_drop3.db", 0, &pager);
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

    rc = executor_init(&exec, pager, cache, &cat);
    if (rc != 0) {
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    /* Create table with 2 columns */
    test_printf("  Creating table 'products' (2 columns)...\n");
    lexer_init(&lex, "CREATE TABLE products (id INTEGER PRIMARY KEY, name TEXT)");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    rc = executor_execute(&exec, &stmt);
    if (rc != 0) {
        test_printf("  ERROR: CREATE TABLE failed: %s\n", executor_get_error(&exec));
        executor_close(&exec);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    /* Drop table */
    test_printf("  Dropping table 'products'...\n");
    lexer_init(&lex, "DROP TABLE products");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    rc = executor_execute(&exec, &stmt);
    if (rc != 0) {
        test_printf("  ERROR: DROP TABLE failed: %s\n", executor_get_error(&exec));
        executor_close(&exec);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    /* Recreate table with different schema (3 columns) */
    test_printf("  Recreating table 'products' (3 columns)...\n");
    lexer_init(&lex, "CREATE TABLE products (id INTEGER PRIMARY KEY, name TEXT, price INTEGER)");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    rc = executor_execute(&exec, &stmt);
    if (rc != 0) {
        test_printf("  ERROR: CREATE TABLE failed on recreate: %s\n", executor_get_error(&exec));
        executor_close(&exec);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    /* Verify new schema */
    rc = catalog_get_table(&cat, "products", &schema);
    if (rc != 0) {
        test_printf("  ERROR: Table 'products' not found after recreate\n");
        executor_close(&exec);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    if (schema.column_count != 3) {
        test_printf("  ERROR: Expected 3 columns, got %d\n", schema.column_count);
        executor_close(&exec);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    test_printf("  ✓ Table recreated with new schema (3 columns)\n");

    executor_close(&exec);
    catalog_close(&cat);
    cache_destroy(cache);
    pager_close(pager);

    return 0;
}

/*
 * Test: COUNT(*) basic functionality
 */
int test_e2e_count_star_basic(void) {
    struct amidb_pager *pager;
    struct page_cache *cache;
    struct catalog cat;
    struct sql_executor exec;
    struct sql_lexer lex;
    struct sql_parser parser;
    struct sql_statement stmt;
    int rc;
    int32_t count_result;

    test_printf("Testing E2E: COUNT(*) basic...\n");

    remove("RAM:test_count1.db");

    rc = pager_open("RAM:test_count1.db", 0, &pager);
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

    rc = executor_init(&exec, pager, cache, &cat);
    if (rc != 0) {
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    /* Create table */
    test_printf("  Creating table 'users'...\n");
    lexer_init(&lex, "CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT)");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    /* Insert 5 rows */
    test_printf("  Inserting 5 rows...\n");
    lexer_init(&lex, "INSERT INTO users VALUES (1, 'Alice')");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    lexer_init(&lex, "INSERT INTO users VALUES (2, 'Bob')");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    lexer_init(&lex, "INSERT INTO users VALUES (3, 'Charlie')");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    lexer_init(&lex, "INSERT INTO users VALUES (4, 'Diana')");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    lexer_init(&lex, "INSERT INTO users VALUES (5, 'Eve')");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    /* COUNT(*) */
    test_printf("  Executing SELECT COUNT(*) FROM users...\n");
    lexer_init(&lex, "SELECT COUNT(*) FROM users");
    parser_init(&parser, &lex);
    rc = parser_parse_statement(&parser, &stmt);
    if (rc != 0) {
        test_printf("  ERROR: Parse failed: %s\n", parser_get_error(&parser));
        executor_close(&exec);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    rc = executor_execute(&exec, &stmt);
    if (rc != 0) {
        test_printf("  ERROR: COUNT(*) failed: %s\n", executor_get_error(&exec));
        executor_close(&exec);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    /* Verify result */
    if (exec.result_count != 1) {
        test_printf("  ERROR: Expected 1 result row, got %u\n", exec.result_count);
        executor_close(&exec);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    count_result = row_get_value(&exec.result_rows[0], 0)->u.i;
    if (count_result != 5) {
        test_printf("  ERROR: Expected COUNT=5, got %d\n", count_result);
        executor_close(&exec);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    test_printf("  ✓ COUNT(*) returned %d\n", count_result);

    executor_close(&exec);
    catalog_close(&cat);
    cache_destroy(cache);
    pager_close(pager);

    return 0;
}

/*
 * Test: COUNT(*) on empty table
 */
int test_e2e_count_star_empty(void) {
    struct amidb_pager *pager;
    struct page_cache *cache;
    struct catalog cat;
    struct sql_executor exec;
    struct sql_lexer lex;
    struct sql_parser parser;
    struct sql_statement stmt;
    int rc;
    int32_t count_result;

    test_printf("Testing E2E: COUNT(*) on empty table...\n");

    remove("RAM:test_count2.db");

    rc = pager_open("RAM:test_count2.db", 0, &pager);
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

    rc = executor_init(&exec, pager, cache, &cat);
    if (rc != 0) {
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    /* Create empty table */
    test_printf("  Creating empty table 'items'...\n");
    lexer_init(&lex, "CREATE TABLE items (id INTEGER PRIMARY KEY, name TEXT)");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    /* COUNT(*) on empty table */
    test_printf("  Executing SELECT COUNT(*) FROM items...\n");
    lexer_init(&lex, "SELECT COUNT(*) FROM items");
    parser_init(&parser, &lex);
    rc = parser_parse_statement(&parser, &stmt);
    if (rc != 0) {
        test_printf("  ERROR: Parse failed: %s\n", parser_get_error(&parser));
        executor_close(&exec);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    rc = executor_execute(&exec, &stmt);
    if (rc != 0) {
        test_printf("  ERROR: COUNT(*) failed: %s\n", executor_get_error(&exec));
        executor_close(&exec);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    /* Verify result is 0 */
    if (exec.result_count != 1) {
        test_printf("  ERROR: Expected 1 result row, got %u\n", exec.result_count);
        executor_close(&exec);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    count_result = row_get_value(&exec.result_rows[0], 0)->u.i;
    if (count_result != 0) {
        test_printf("  ERROR: Expected COUNT=0, got %d\n", count_result);
        executor_close(&exec);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    test_printf("  ✓ COUNT(*) on empty table returned %d\n", count_result);

    executor_close(&exec);
    catalog_close(&cat);
    cache_destroy(cache);
    pager_close(pager);

    return 0;
}

/*
 * Test: COUNT(*) with WHERE clause
 */
int test_e2e_count_star_where(void) {
    struct amidb_pager *pager;
    struct page_cache *cache;
    struct catalog cat;
    struct sql_executor exec;
    struct sql_lexer lex;
    struct sql_parser parser;
    struct sql_statement stmt;
    int rc;
    int32_t count_result;

    test_printf("Testing E2E: COUNT(*) with WHERE clause...\n");

    remove("RAM:test_count3.db");

    rc = pager_open("RAM:test_count3.db", 0, &pager);
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

    rc = executor_init(&exec, pager, cache, &cat);
    if (rc != 0) {
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    /* Create table */
    test_printf("  Creating table 'scores'...\n");
    lexer_init(&lex, "CREATE TABLE scores (id INTEGER PRIMARY KEY, score INTEGER)");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    /* Insert rows with different scores */
    lexer_init(&lex, "INSERT INTO scores VALUES (1, 50)");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    lexer_init(&lex, "INSERT INTO scores VALUES (2, 75)");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    lexer_init(&lex, "INSERT INTO scores VALUES (3, 90)");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    lexer_init(&lex, "INSERT INTO scores VALUES (4, 60)");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    lexer_init(&lex, "INSERT INTO scores VALUES (5, 85)");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    /* COUNT(*) with WHERE score >= 75 (should be 3: 75, 90, 85) */
    test_printf("  Executing SELECT COUNT(*) FROM scores WHERE score >= 75...\n");
    lexer_init(&lex, "SELECT COUNT(*) FROM scores WHERE score >= 75");
    parser_init(&parser, &lex);
    rc = parser_parse_statement(&parser, &stmt);
    if (rc != 0) {
        test_printf("  ERROR: Parse failed: %s\n", parser_get_error(&parser));
        executor_close(&exec);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    rc = executor_execute(&exec, &stmt);
    if (rc != 0) {
        test_printf("  ERROR: COUNT(*) failed: %s\n", executor_get_error(&exec));
        executor_close(&exec);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    /* Verify result */
    count_result = row_get_value(&exec.result_rows[0], 0)->u.i;
    if (count_result != 3) {
        test_printf("  ERROR: Expected COUNT=3, got %d\n", count_result);
        executor_close(&exec);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    test_printf("  ✓ COUNT(*) WHERE score >= 75 returned %d\n", count_result);

    executor_close(&exec);
    catalog_close(&cat);
    cache_destroy(cache);
    pager_close(pager);

    return 0;
}

/*
 * Test: COUNT(column) functionality
 */
int test_e2e_count_column(void) {
    struct amidb_pager *pager;
    struct page_cache *cache;
    struct catalog cat;
    struct sql_executor exec;
    struct sql_lexer lex;
    struct sql_parser parser;
    struct sql_statement stmt;
    int rc;
    int32_t count_result;

    test_printf("Testing E2E: COUNT(column)...\n");

    remove("RAM:test_count4.db");

    rc = pager_open("RAM:test_count4.db", 0, &pager);
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

    rc = executor_init(&exec, pager, cache, &cat);
    if (rc != 0) {
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    /* Create table */
    test_printf("  Creating table 'products'...\n");
    lexer_init(&lex, "CREATE TABLE products (id INTEGER PRIMARY KEY, name TEXT)");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    /* Insert rows */
    lexer_init(&lex, "INSERT INTO products VALUES (1, 'Apple')");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    lexer_init(&lex, "INSERT INTO products VALUES (2, 'Banana')");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    lexer_init(&lex, "INSERT INTO products VALUES (3, 'Cherry')");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    /* COUNT(name) */
    test_printf("  Executing SELECT COUNT(name) FROM products...\n");
    lexer_init(&lex, "SELECT COUNT(name) FROM products");
    parser_init(&parser, &lex);
    rc = parser_parse_statement(&parser, &stmt);
    if (rc != 0) {
        test_printf("  ERROR: Parse failed: %s\n", parser_get_error(&parser));
        executor_close(&exec);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    rc = executor_execute(&exec, &stmt);
    if (rc != 0) {
        test_printf("  ERROR: COUNT(column) failed: %s\n", executor_get_error(&exec));
        executor_close(&exec);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    /* Verify result */
    count_result = row_get_value(&exec.result_rows[0], 0)->u.i;
    if (count_result != 3) {
        test_printf("  ERROR: Expected COUNT=3, got %d\n", count_result);
        executor_close(&exec);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    test_printf("  ✓ COUNT(name) returned %d\n", count_result);

    executor_close(&exec);
    catalog_close(&cat);
    cache_destroy(cache);
    pager_close(pager);

    return 0;
}

/*
 * Test: SUM basic functionality
 */
int test_e2e_sum_basic(void) {
    struct amidb_pager *pager;
    struct page_cache *cache;
    struct catalog cat;
    struct sql_executor exec;
    struct sql_lexer lex;
    struct sql_parser parser;
    struct sql_statement stmt;
    int rc;
    int32_t sum_result;

    test_printf("Testing E2E: SUM basic...\n");

    remove("RAM:test_sum1.db");

    rc = pager_open("RAM:test_sum1.db", 0, &pager);
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

    rc = executor_init(&exec, pager, cache, &cat);
    if (rc != 0) {
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    /* Create table */
    test_printf("  Creating table 'scores'...\n");
    lexer_init(&lex, "CREATE TABLE scores (id INTEGER PRIMARY KEY, points INTEGER)");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    /* Insert rows: 10 + 20 + 30 + 40 + 50 = 150 */
    test_printf("  Inserting rows with points: 10, 20, 30, 40, 50...\n");
    lexer_init(&lex, "INSERT INTO scores VALUES (1, 10)");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    lexer_init(&lex, "INSERT INTO scores VALUES (2, 20)");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    lexer_init(&lex, "INSERT INTO scores VALUES (3, 30)");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    lexer_init(&lex, "INSERT INTO scores VALUES (4, 40)");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    lexer_init(&lex, "INSERT INTO scores VALUES (5, 50)");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    /* SUM(points) */
    test_printf("  Executing SELECT SUM(points) FROM scores...\n");
    lexer_init(&lex, "SELECT SUM(points) FROM scores");
    parser_init(&parser, &lex);
    rc = parser_parse_statement(&parser, &stmt);
    if (rc != 0) {
        test_printf("  ERROR: Parse failed: %s\n", parser_get_error(&parser));
        executor_close(&exec);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    rc = executor_execute(&exec, &stmt);
    if (rc != 0) {
        test_printf("  ERROR: SUM() failed: %s\n", executor_get_error(&exec));
        executor_close(&exec);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    /* Verify result: 10+20+30+40+50 = 150 */
    if (exec.result_count != 1) {
        test_printf("  ERROR: Expected 1 result row, got %u\n", exec.result_count);
        executor_close(&exec);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    sum_result = row_get_value(&exec.result_rows[0], 0)->u.i;
    if (sum_result != 150) {
        test_printf("  ERROR: Expected SUM=150, got %d\n", sum_result);
        executor_close(&exec);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    test_printf("  ✓ SUM(points) returned %d\n", sum_result);

    executor_close(&exec);
    catalog_close(&cat);
    cache_destroy(cache);
    pager_close(pager);

    return 0;
}

/*
 * Test: SUM on empty table
 */
int test_e2e_sum_empty(void) {
    struct amidb_pager *pager;
    struct page_cache *cache;
    struct catalog cat;
    struct sql_executor exec;
    struct sql_lexer lex;
    struct sql_parser parser;
    struct sql_statement stmt;
    int rc;
    int32_t sum_result;

    test_printf("Testing E2E: SUM on empty table...\n");

    remove("RAM:test_sum2.db");

    rc = pager_open("RAM:test_sum2.db", 0, &pager);
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

    rc = executor_init(&exec, pager, cache, &cat);
    if (rc != 0) {
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    /* Create empty table */
    test_printf("  Creating empty table 'values'...\n");
    lexer_init(&lex, "CREATE TABLE vals (id INTEGER PRIMARY KEY, amount INTEGER)");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    /* SUM on empty table */
    test_printf("  Executing SELECT SUM(amount) FROM vals...\n");
    lexer_init(&lex, "SELECT SUM(amount) FROM vals");
    parser_init(&parser, &lex);
    rc = parser_parse_statement(&parser, &stmt);
    if (rc != 0) {
        test_printf("  ERROR: Parse failed: %s\n", parser_get_error(&parser));
        executor_close(&exec);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    rc = executor_execute(&exec, &stmt);
    if (rc != 0) {
        test_printf("  ERROR: SUM() failed: %s\n", executor_get_error(&exec));
        executor_close(&exec);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    /* Verify result is 0 */
    sum_result = row_get_value(&exec.result_rows[0], 0)->u.i;
    if (sum_result != 0) {
        test_printf("  ERROR: Expected SUM=0, got %d\n", sum_result);
        executor_close(&exec);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    test_printf("  ✓ SUM on empty table returned %d\n", sum_result);

    executor_close(&exec);
    catalog_close(&cat);
    cache_destroy(cache);
    pager_close(pager);

    return 0;
}

/*
 * Test: SUM with WHERE clause
 */
int test_e2e_sum_where(void) {
    struct amidb_pager *pager;
    struct page_cache *cache;
    struct catalog cat;
    struct sql_executor exec;
    struct sql_lexer lex;
    struct sql_parser parser;
    struct sql_statement stmt;
    int rc;
    int32_t sum_result;

    test_printf("Testing E2E: SUM with WHERE clause...\n");

    remove("RAM:test_sum3.db");

    rc = pager_open("RAM:test_sum3.db", 0, &pager);
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

    rc = executor_init(&exec, pager, cache, &cat);
    if (rc != 0) {
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    /* Create table */
    test_printf("  Creating table 'orders'...\n");
    lexer_init(&lex, "CREATE TABLE orders (id INTEGER PRIMARY KEY, amount INTEGER)");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    /* Insert rows */
    lexer_init(&lex, "INSERT INTO orders VALUES (1, 100)");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    lexer_init(&lex, "INSERT INTO orders VALUES (2, 200)");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    lexer_init(&lex, "INSERT INTO orders VALUES (3, 300)");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    lexer_init(&lex, "INSERT INTO orders VALUES (4, 400)");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    lexer_init(&lex, "INSERT INTO orders VALUES (5, 500)");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    /* SUM with WHERE amount >= 300 (should be 300+400+500 = 1200) */
    test_printf("  Executing SELECT SUM(amount) FROM orders WHERE amount >= 300...\n");
    lexer_init(&lex, "SELECT SUM(amount) FROM orders WHERE amount >= 300");
    parser_init(&parser, &lex);
    rc = parser_parse_statement(&parser, &stmt);
    if (rc != 0) {
        test_printf("  ERROR: Parse failed: %s\n", parser_get_error(&parser));
        executor_close(&exec);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    rc = executor_execute(&exec, &stmt);
    if (rc != 0) {
        test_printf("  ERROR: SUM() failed: %s\n", executor_get_error(&exec));
        executor_close(&exec);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    /* Verify result: 300+400+500 = 1200 */
    sum_result = row_get_value(&exec.result_rows[0], 0)->u.i;
    if (sum_result != 1200) {
        test_printf("  ERROR: Expected SUM=1200, got %d\n", sum_result);
        executor_close(&exec);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    test_printf("  ✓ SUM WHERE amount >= 300 returned %d\n", sum_result);

    executor_close(&exec);
    catalog_close(&cat);
    cache_destroy(cache);
    pager_close(pager);

    return 0;
}

/*
 * Test: AVG aggregate basic
 */
int test_e2e_avg_basic(void) {
    struct amidb_pager *pager;
    struct page_cache *cache;
    struct catalog cat;
    struct sql_executor exec;
    struct sql_lexer lex;
    struct sql_parser parser;
    struct sql_statement stmt;
    int rc;
    int32_t avg_result;

    test_printf("Testing E2E: AVG aggregate basic...\n");

    remove("RAM:test_avg1.db");

    rc = pager_open("RAM:test_avg1.db", 0, &pager);
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

    rc = executor_init(&exec, pager, cache, &cat);
    if (rc != 0) {
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    /* Create table */
    test_printf("  Creating table 'scores'...\n");
    lexer_init(&lex, "CREATE TABLE scores (id INTEGER PRIMARY KEY, value INTEGER)");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    /* Insert rows: 10, 20, 30, 40 (avg = 25) */
    lexer_init(&lex, "INSERT INTO scores VALUES (1, 10)");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    lexer_init(&lex, "INSERT INTO scores VALUES (2, 20)");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    lexer_init(&lex, "INSERT INTO scores VALUES (3, 30)");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    lexer_init(&lex, "INSERT INTO scores VALUES (4, 40)");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    /* AVG(value) should be (10+20+30+40)/4 = 25 */
    test_printf("  Executing SELECT AVG(value) FROM scores...\n");
    lexer_init(&lex, "SELECT AVG(value) FROM scores");
    parser_init(&parser, &lex);
    rc = parser_parse_statement(&parser, &stmt);
    if (rc != 0) {
        test_printf("  ERROR: Parse failed: %s\n", parser_get_error(&parser));
        executor_close(&exec);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    rc = executor_execute(&exec, &stmt);
    if (rc != 0) {
        test_printf("  ERROR: AVG() failed: %s\n", executor_get_error(&exec));
        executor_close(&exec);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    /* Verify result */
    avg_result = row_get_value(&exec.result_rows[0], 0)->u.i;
    if (avg_result != 25) {
        test_printf("  ERROR: Expected AVG=25, got %d\n", avg_result);
        executor_close(&exec);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    test_printf("  ✓ AVG(value) returned %d\n", avg_result);

    executor_close(&exec);
    catalog_close(&cat);
    cache_destroy(cache);
    pager_close(pager);

    return 0;
}

/*
 * Test: AVG aggregate on empty table
 */
int test_e2e_avg_empty(void) {
    struct amidb_pager *pager;
    struct page_cache *cache;
    struct catalog cat;
    struct sql_executor exec;
    struct sql_lexer lex;
    struct sql_parser parser;
    struct sql_statement stmt;
    int rc;
    int32_t avg_result;

    test_printf("Testing E2E: AVG on empty table...\n");

    remove("RAM:test_avg2.db");

    rc = pager_open("RAM:test_avg2.db", 0, &pager);
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

    rc = executor_init(&exec, pager, cache, &cat);
    if (rc != 0) {
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    /* Create empty table */
    test_printf("  Creating empty table 'empty_avg'...\n");
    lexer_init(&lex, "CREATE TABLE empty_avg (id INTEGER PRIMARY KEY, value INTEGER)");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    /* AVG(value) on empty table should be 0 */
    test_printf("  Executing SELECT AVG(value) FROM empty_avg...\n");
    lexer_init(&lex, "SELECT AVG(value) FROM empty_avg");
    parser_init(&parser, &lex);
    rc = parser_parse_statement(&parser, &stmt);
    if (rc != 0) {
        test_printf("  ERROR: Parse failed: %s\n", parser_get_error(&parser));
        executor_close(&exec);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    rc = executor_execute(&exec, &stmt);
    if (rc != 0) {
        test_printf("  ERROR: AVG() failed: %s\n", executor_get_error(&exec));
        executor_close(&exec);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    /* Verify result is 0 for empty table */
    avg_result = row_get_value(&exec.result_rows[0], 0)->u.i;
    if (avg_result != 0) {
        test_printf("  ERROR: Expected AVG=0 for empty table, got %d\n", avg_result);
        executor_close(&exec);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    test_printf("  ✓ AVG on empty table returned %d\n", avg_result);

    executor_close(&exec);
    catalog_close(&cat);
    cache_destroy(cache);
    pager_close(pager);

    return 0;
}

/*
 * Test: AVG aggregate with WHERE clause
 */
int test_e2e_avg_where(void) {
    struct amidb_pager *pager;
    struct page_cache *cache;
    struct catalog cat;
    struct sql_executor exec;
    struct sql_lexer lex;
    struct sql_parser parser;
    struct sql_statement stmt;
    int rc;
    int32_t avg_result;

    test_printf("Testing E2E: AVG with WHERE clause...\n");

    remove("RAM:test_avg3.db");

    rc = pager_open("RAM:test_avg3.db", 0, &pager);
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

    rc = executor_init(&exec, pager, cache, &cat);
    if (rc != 0) {
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    /* Create table */
    test_printf("  Creating table 'grades'...\n");
    lexer_init(&lex, "CREATE TABLE grades (id INTEGER PRIMARY KEY, score INTEGER)");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    /* Insert rows: 50, 60, 70, 80, 90 */
    lexer_init(&lex, "INSERT INTO grades VALUES (1, 50)");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    lexer_init(&lex, "INSERT INTO grades VALUES (2, 60)");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    lexer_init(&lex, "INSERT INTO grades VALUES (3, 70)");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    lexer_init(&lex, "INSERT INTO grades VALUES (4, 80)");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    lexer_init(&lex, "INSERT INTO grades VALUES (5, 90)");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    /* AVG with WHERE score >= 70 (should be (70+80+90)/3 = 80) */
    test_printf("  Executing SELECT AVG(score) FROM grades WHERE score >= 70...\n");
    lexer_init(&lex, "SELECT AVG(score) FROM grades WHERE score >= 70");
    parser_init(&parser, &lex);
    rc = parser_parse_statement(&parser, &stmt);
    if (rc != 0) {
        test_printf("  ERROR: Parse failed: %s\n", parser_get_error(&parser));
        executor_close(&exec);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    rc = executor_execute(&exec, &stmt);
    if (rc != 0) {
        test_printf("  ERROR: AVG() failed: %s\n", executor_get_error(&exec));
        executor_close(&exec);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    /* Verify result: (70+80+90)/3 = 80 */
    avg_result = row_get_value(&exec.result_rows[0], 0)->u.i;
    if (avg_result != 80) {
        test_printf("  ERROR: Expected AVG=80, got %d\n", avg_result);
        executor_close(&exec);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    test_printf("  ✓ AVG WHERE score >= 70 returned %d\n", avg_result);

    executor_close(&exec);
    catalog_close(&cat);
    cache_destroy(cache);
    pager_close(pager);

    return 0;
}

/*
 * Test: MIN aggregate basic
 */
int test_e2e_min_basic(void) {
    struct amidb_pager *pager;
    struct page_cache *cache;
    struct catalog cat;
    struct sql_executor exec;
    struct sql_lexer lex;
    struct sql_parser parser;
    struct sql_statement stmt;
    int rc;
    int32_t min_result;

    test_printf("Testing E2E: MIN aggregate basic...\n");

    remove("RAM:test_min1.db");

    rc = pager_open("RAM:test_min1.db", 0, &pager);
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

    rc = executor_init(&exec, pager, cache, &cat);
    if (rc != 0) {
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    /* Create table */
    test_printf("  Creating table 'temps'...\n");
    lexer_init(&lex, "CREATE TABLE temps (id INTEGER PRIMARY KEY, value INTEGER)");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    /* Insert rows: 50, 20, 80, 10, 40 (min = 10) */
    lexer_init(&lex, "INSERT INTO temps VALUES (1, 50)");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    lexer_init(&lex, "INSERT INTO temps VALUES (2, 20)");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    lexer_init(&lex, "INSERT INTO temps VALUES (3, 80)");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    lexer_init(&lex, "INSERT INTO temps VALUES (4, 10)");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    lexer_init(&lex, "INSERT INTO temps VALUES (5, 40)");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    /* MIN(value) should be 10 */
    test_printf("  Executing SELECT MIN(value) FROM temps...\n");
    lexer_init(&lex, "SELECT MIN(value) FROM temps");
    parser_init(&parser, &lex);
    rc = parser_parse_statement(&parser, &stmt);
    if (rc != 0) {
        test_printf("  ERROR: Parse failed: %s\n", parser_get_error(&parser));
        executor_close(&exec);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    rc = executor_execute(&exec, &stmt);
    if (rc != 0) {
        test_printf("  ERROR: MIN() failed: %s\n", executor_get_error(&exec));
        executor_close(&exec);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    /* Verify result */
    min_result = row_get_value(&exec.result_rows[0], 0)->u.i;
    if (min_result != 10) {
        test_printf("  ERROR: Expected MIN=10, got %d\n", min_result);
        executor_close(&exec);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    test_printf("  ✓ MIN(value) returned %d\n", min_result);

    executor_close(&exec);
    catalog_close(&cat);
    cache_destroy(cache);
    pager_close(pager);

    return 0;
}

/*
 * Test: MIN aggregate on empty table
 */
int test_e2e_min_empty(void) {
    struct amidb_pager *pager;
    struct page_cache *cache;
    struct catalog cat;
    struct sql_executor exec;
    struct sql_lexer lex;
    struct sql_parser parser;
    struct sql_statement stmt;
    int rc;
    int32_t min_result;

    test_printf("Testing E2E: MIN on empty table...\n");

    remove("RAM:test_min2.db");

    rc = pager_open("RAM:test_min2.db", 0, &pager);
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

    rc = executor_init(&exec, pager, cache, &cat);
    if (rc != 0) {
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    /* Create empty table */
    test_printf("  Creating empty table 'empty_min'...\n");
    lexer_init(&lex, "CREATE TABLE empty_min (id INTEGER PRIMARY KEY, value INTEGER)");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    /* MIN(value) on empty table should be 0 */
    test_printf("  Executing SELECT MIN(value) FROM empty_min...\n");
    lexer_init(&lex, "SELECT MIN(value) FROM empty_min");
    parser_init(&parser, &lex);
    rc = parser_parse_statement(&parser, &stmt);
    if (rc != 0) {
        test_printf("  ERROR: Parse failed: %s\n", parser_get_error(&parser));
        executor_close(&exec);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    rc = executor_execute(&exec, &stmt);
    if (rc != 0) {
        test_printf("  ERROR: MIN() failed: %s\n", executor_get_error(&exec));
        executor_close(&exec);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    /* Verify result is 0 for empty table */
    min_result = row_get_value(&exec.result_rows[0], 0)->u.i;
    if (min_result != 0) {
        test_printf("  ERROR: Expected MIN=0 for empty table, got %d\n", min_result);
        executor_close(&exec);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    test_printf("  ✓ MIN on empty table returned %d\n", min_result);

    executor_close(&exec);
    catalog_close(&cat);
    cache_destroy(cache);
    pager_close(pager);

    return 0;
}

/*
 * Test: MIN aggregate with WHERE clause
 */
int test_e2e_min_where(void) {
    struct amidb_pager *pager;
    struct page_cache *cache;
    struct catalog cat;
    struct sql_executor exec;
    struct sql_lexer lex;
    struct sql_parser parser;
    struct sql_statement stmt;
    int rc;
    int32_t min_result;

    test_printf("Testing E2E: MIN with WHERE clause...\n");

    remove("RAM:test_min3.db");

    rc = pager_open("RAM:test_min3.db", 0, &pager);
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

    rc = executor_init(&exec, pager, cache, &cat);
    if (rc != 0) {
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    /* Create table */
    test_printf("  Creating table 'prices'...\n");
    lexer_init(&lex, "CREATE TABLE prices (id INTEGER PRIMARY KEY, amount INTEGER)");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    /* Insert rows: 100, 200, 300, 400, 500 */
    lexer_init(&lex, "INSERT INTO prices VALUES (1, 100)");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    lexer_init(&lex, "INSERT INTO prices VALUES (2, 200)");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    lexer_init(&lex, "INSERT INTO prices VALUES (3, 300)");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    lexer_init(&lex, "INSERT INTO prices VALUES (4, 400)");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    lexer_init(&lex, "INSERT INTO prices VALUES (5, 500)");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    /* MIN with WHERE amount >= 300 (should be min of 300, 400, 500 = 300) */
    test_printf("  Executing SELECT MIN(amount) FROM prices WHERE amount >= 300...\n");
    lexer_init(&lex, "SELECT MIN(amount) FROM prices WHERE amount >= 300");
    parser_init(&parser, &lex);
    rc = parser_parse_statement(&parser, &stmt);
    if (rc != 0) {
        test_printf("  ERROR: Parse failed: %s\n", parser_get_error(&parser));
        executor_close(&exec);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    rc = executor_execute(&exec, &stmt);
    if (rc != 0) {
        test_printf("  ERROR: MIN() failed: %s\n", executor_get_error(&exec));
        executor_close(&exec);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    /* Verify result: min of 300, 400, 500 = 300 */
    min_result = row_get_value(&exec.result_rows[0], 0)->u.i;
    if (min_result != 300) {
        test_printf("  ERROR: Expected MIN=300, got %d\n", min_result);
        executor_close(&exec);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    test_printf("  ✓ MIN WHERE amount >= 300 returned %d\n", min_result);

    executor_close(&exec);
    catalog_close(&cat);
    cache_destroy(cache);
    pager_close(pager);

    return 0;
}

/*
 * Test: MAX aggregate basic
 */
int test_e2e_max_basic(void) {
    struct amidb_pager *pager;
    struct page_cache *cache;
    struct catalog cat;
    struct sql_executor exec;
    struct sql_lexer lex;
    struct sql_parser parser;
    struct sql_statement stmt;
    int rc;
    int32_t max_result;

    test_printf("Testing E2E: MAX aggregate basic...\n");

    remove("RAM:test_max1.db");

    rc = pager_open("RAM:test_max1.db", 0, &pager);
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

    rc = executor_init(&exec, pager, cache, &cat);
    if (rc != 0) {
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    /* Create table */
    test_printf("  Creating table 'temps'...\n");
    lexer_init(&lex, "CREATE TABLE temps (id INTEGER PRIMARY KEY, value INTEGER)");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    /* Insert rows: 50, 20, 80, 10, 40 (max = 80) */
    lexer_init(&lex, "INSERT INTO temps VALUES (1, 50)");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    lexer_init(&lex, "INSERT INTO temps VALUES (2, 20)");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    lexer_init(&lex, "INSERT INTO temps VALUES (3, 80)");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    lexer_init(&lex, "INSERT INTO temps VALUES (4, 10)");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    lexer_init(&lex, "INSERT INTO temps VALUES (5, 40)");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    /* MAX(value) should be 80 */
    test_printf("  Executing SELECT MAX(value) FROM temps...\n");
    lexer_init(&lex, "SELECT MAX(value) FROM temps");
    parser_init(&parser, &lex);
    rc = parser_parse_statement(&parser, &stmt);
    if (rc != 0) {
        test_printf("  ERROR: Parse failed: %s\n", parser_get_error(&parser));
        executor_close(&exec);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    rc = executor_execute(&exec, &stmt);
    if (rc != 0) {
        test_printf("  ERROR: MAX() failed: %s\n", executor_get_error(&exec));
        executor_close(&exec);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    /* Verify result */
    max_result = row_get_value(&exec.result_rows[0], 0)->u.i;
    if (max_result != 80) {
        test_printf("  ERROR: Expected MAX=80, got %d\n", max_result);
        executor_close(&exec);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    test_printf("  ✓ MAX(value) returned %d\n", max_result);

    executor_close(&exec);
    catalog_close(&cat);
    cache_destroy(cache);
    pager_close(pager);

    return 0;
}

/*
 * Test: MAX aggregate on empty table
 */
int test_e2e_max_empty(void) {
    struct amidb_pager *pager;
    struct page_cache *cache;
    struct catalog cat;
    struct sql_executor exec;
    struct sql_lexer lex;
    struct sql_parser parser;
    struct sql_statement stmt;
    int rc;
    int32_t max_result;

    test_printf("Testing E2E: MAX on empty table...\n");

    remove("RAM:test_max2.db");

    rc = pager_open("RAM:test_max2.db", 0, &pager);
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

    rc = executor_init(&exec, pager, cache, &cat);
    if (rc != 0) {
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    /* Create empty table */
    test_printf("  Creating empty table 'empty_max'...\n");
    lexer_init(&lex, "CREATE TABLE empty_max (id INTEGER PRIMARY KEY, value INTEGER)");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    /* MAX(value) on empty table should be 0 */
    test_printf("  Executing SELECT MAX(value) FROM empty_max...\n");
    lexer_init(&lex, "SELECT MAX(value) FROM empty_max");
    parser_init(&parser, &lex);
    rc = parser_parse_statement(&parser, &stmt);
    if (rc != 0) {
        test_printf("  ERROR: Parse failed: %s\n", parser_get_error(&parser));
        executor_close(&exec);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    rc = executor_execute(&exec, &stmt);
    if (rc != 0) {
        test_printf("  ERROR: MAX() failed: %s\n", executor_get_error(&exec));
        executor_close(&exec);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    /* Verify result is 0 for empty table */
    max_result = row_get_value(&exec.result_rows[0], 0)->u.i;
    if (max_result != 0) {
        test_printf("  ERROR: Expected MAX=0 for empty table, got %d\n", max_result);
        executor_close(&exec);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    test_printf("  ✓ MAX on empty table returned %d\n", max_result);

    executor_close(&exec);
    catalog_close(&cat);
    cache_destroy(cache);
    pager_close(pager);

    return 0;
}

/*
 * Test: MAX aggregate with WHERE clause
 */
int test_e2e_max_where(void) {
    struct amidb_pager *pager;
    struct page_cache *cache;
    struct catalog cat;
    struct sql_executor exec;
    struct sql_lexer lex;
    struct sql_parser parser;
    struct sql_statement stmt;
    int rc;
    int32_t max_result;

    test_printf("Testing E2E: MAX with WHERE clause...\n");

    remove("RAM:test_max3.db");

    rc = pager_open("RAM:test_max3.db", 0, &pager);
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

    rc = executor_init(&exec, pager, cache, &cat);
    if (rc != 0) {
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    /* Create table */
    test_printf("  Creating table 'prices'...\n");
    lexer_init(&lex, "CREATE TABLE prices (id INTEGER PRIMARY KEY, amount INTEGER)");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    /* Insert rows: 100, 200, 300, 400, 500 */
    lexer_init(&lex, "INSERT INTO prices VALUES (1, 100)");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    lexer_init(&lex, "INSERT INTO prices VALUES (2, 200)");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    lexer_init(&lex, "INSERT INTO prices VALUES (3, 300)");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    lexer_init(&lex, "INSERT INTO prices VALUES (4, 400)");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    lexer_init(&lex, "INSERT INTO prices VALUES (5, 500)");
    parser_init(&parser, &lex);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    /* MAX with WHERE amount <= 300 (should be max of 100, 200, 300 = 300) */
    test_printf("  Executing SELECT MAX(amount) FROM prices WHERE amount <= 300...\n");
    lexer_init(&lex, "SELECT MAX(amount) FROM prices WHERE amount <= 300");
    parser_init(&parser, &lex);
    rc = parser_parse_statement(&parser, &stmt);
    if (rc != 0) {
        test_printf("  ERROR: Parse failed: %s\n", parser_get_error(&parser));
        executor_close(&exec);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    rc = executor_execute(&exec, &stmt);
    if (rc != 0) {
        test_printf("  ERROR: MAX() failed: %s\n", executor_get_error(&exec));
        executor_close(&exec);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    /* Verify result: max of 100, 200, 300 = 300 */
    max_result = row_get_value(&exec.result_rows[0], 0)->u.i;
    if (max_result != 300) {
        test_printf("  ERROR: Expected MAX=300, got %d\n", max_result);
        executor_close(&exec);
        catalog_close(&cat);
        cache_destroy(cache);
        pager_close(pager);
        return -1;
    }

    test_printf("  ✓ MAX WHERE amount <= 300 returned %d\n", max_result);

    executor_close(&exec);
    catalog_close(&cat);
    cache_destroy(cache);
    pager_close(pager);

    return 0;
}
