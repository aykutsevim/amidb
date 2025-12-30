/*
 * test_btree_basic.c - Unit tests for B+Tree basic operations (Phase 3A)
 */

#include "test_harness.h"
#include "storage/btree.h"
#include "storage/pager.h"
#include "storage/cache.h"
#include "os/file.h"
#include "os/mem.h"
#include <stdio.h>

/* Test: Create and close B+Tree */
TEST(btree_create_close) {
    struct amidb_pager *pager = NULL;
    struct page_cache *cache;
    struct btree *tree;
    uint32_t root_page;
    int rc;

    TEST_BEGIN();

    /* Create pager */
    rc = pager_open("RAM:btree_create_close.db", 0, &pager);
    ASSERT_EQ(rc, 0);

    /* Create cache */
    cache = cache_create(16, pager);
    ASSERT_NOT_NULL(cache);

    /* Create B+Tree */
    tree = btree_create(pager, cache, &root_page);
    test_printf("  Created B+Tree with root page %u\n", root_page);
    ASSERT_NOT_NULL(tree);

    /* Close tree */
    btree_close(tree);
    cache_destroy(cache);
    pager_close(pager);

    TEST_END();
    return 0;
}

/* Test: Insert and search single entry */
TEST(btree_single_entry) {
    struct amidb_pager *pager = NULL;
    struct page_cache *cache;
    struct btree *tree;
    uint32_t root_page;
    uint32_t value;
    int rc;

    TEST_BEGIN();

    rc = pager_open("RAM:btree_single_entry.db", 0, &pager);
    ASSERT_EQ(rc, 0);

    cache = cache_create(16, pager);
    ASSERT_NOT_NULL(cache);

    tree = btree_create(pager, cache, &root_page);
    ASSERT_NOT_NULL(tree);

    /* Insert key/value */
    rc = btree_insert(tree, 42, 1000);
    test_printf("  btree_insert(42, 1000) returned: %d\n", rc);
    ASSERT_EQ(rc, 0);

    /* Search for key */
    rc = btree_search(tree, 42, &value);
    test_printf("  btree_search(42) returned: %d, value=%u\n", rc, value);
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(value, 1000);

    /* Search for non-existent key */
    rc = btree_search(tree, 99, &value);
    test_printf("  btree_search(99) returned: %d (expected -1)\n", rc);
    ASSERT_EQ(rc, -1);

    btree_close(tree);
    cache_destroy(cache);
    pager_close(pager);

    TEST_END();
    return 0;
}

/* Test: Insert multiple entries */
TEST(btree_multiple_entries) {
    struct amidb_pager *pager = NULL;
    struct page_cache *cache;
    struct btree *tree;
    uint32_t root_page;
    uint32_t value;
    int rc;
    int i;

    TEST_BEGIN();

    rc = pager_open("RAM:btree_multiple_entries.db", 0, &pager);
    ASSERT_EQ(rc, 0);

    cache = cache_create(16, pager);
    ASSERT_NOT_NULL(cache);

    tree = btree_create(pager, cache, &root_page);
    ASSERT_NOT_NULL(tree);

    /* Insert 10 entries */
    for (i = 0; i < 10; i++) {
        rc = btree_insert(tree, i * 10, i * 100);
        ASSERT_EQ(rc, 0);
    }

    test_printf("  Inserted 10 entries\n");

    /* Search for all entries */
    for (i = 0; i < 10; i++) {
        rc = btree_search(tree, i * 10, &value);
        ASSERT_EQ(rc, 0);
        ASSERT_EQ(value, (uint32_t)(i * 100));
    }

    test_printf("  All 10 entries found\n");

    btree_close(tree);
    cache_destroy(cache);
    pager_close(pager);

    TEST_END();
    return 0;
}

/* Test: Insert in reverse order */
TEST(btree_reverse_order) {
    struct amidb_pager *pager = NULL;
    struct page_cache *cache;
    struct btree *tree;
    uint32_t root_page;
    uint32_t value;
    int rc;
    int i;

    TEST_BEGIN();

    rc = pager_open("RAM:btree_reverse_order.db", 0, &pager);
    ASSERT_EQ(rc, 0);

    cache = cache_create(16, pager);
    ASSERT_NOT_NULL(cache);

    tree = btree_create(pager, cache, &root_page);
    ASSERT_NOT_NULL(tree);

    /* Insert 10 entries in reverse order */
    for (i = 9; i >= 0; i--) {
        rc = btree_insert(tree, i * 10, i * 100);
        ASSERT_EQ(rc, 0);
    }

    test_printf("  Inserted 10 entries in reverse order\n");

    /* Search for all entries */
    for (i = 0; i < 10; i++) {
        rc = btree_search(tree, i * 10, &value);
        ASSERT_EQ(rc, 0);
        ASSERT_EQ(value, (uint32_t)(i * 100));
    }

    test_printf("  All 10 entries found\n");

    btree_close(tree);
    cache_destroy(cache);
    pager_close(pager);

    TEST_END();
    return 0;
}

/* Test: Delete entries */
TEST(btree_delete) {
    struct amidb_pager *pager = NULL;
    struct page_cache *cache;
    struct btree *tree;
    uint32_t root_page;
    uint32_t value;
    int rc;
    int i;

    TEST_BEGIN();

    rc = pager_open("RAM:btree_delete.db", 0, &pager);
    ASSERT_EQ(rc, 0);

    cache = cache_create(16, pager);
    ASSERT_NOT_NULL(cache);

    tree = btree_create(pager, cache, &root_page);
    ASSERT_NOT_NULL(tree);

    /* Insert 5 entries */
    for (i = 0; i < 5; i++) {
        rc = btree_insert(tree, i * 10, i * 100);
        ASSERT_EQ(rc, 0);
    }

    test_printf("  Inserted 5 entries\n");

    /* Delete middle entry */
    rc = btree_delete(tree, 20);
    test_printf("  Deleted key 20: %d\n", rc);
    ASSERT_EQ(rc, 0);

    /* Verify deletion */
    rc = btree_search(tree, 20, &value);
    ASSERT_EQ(rc, -1);  /* Should not be found */

    /* Verify others still exist */
    rc = btree_search(tree, 0, &value);
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(value, 0);

    rc = btree_search(tree, 30, &value);
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(value, 300);

    test_printf("  Other entries still found\n");

    btree_close(tree);
    cache_destroy(cache);
    pager_close(pager);

    TEST_END();
    return 0;
}

/* Test: Cursor iteration */
TEST(btree_cursor) {
    struct amidb_pager *pager = NULL;
    struct page_cache *cache;
    struct btree *tree;
    struct btree_cursor cursor;
    uint32_t root_page;
    int32_t key;
    uint32_t value;
    int rc;
    int i;
    int count;

    TEST_BEGIN();

    rc = pager_open("RAM:btree_cursor.db", 0, &pager);
    ASSERT_EQ(rc, 0);

    cache = cache_create(16, pager);
    ASSERT_NOT_NULL(cache);

    tree = btree_create(pager, cache, &root_page);
    ASSERT_NOT_NULL(tree);

    /* Insert 5 entries */
    for (i = 0; i < 5; i++) {
        rc = btree_insert(tree, i * 10, i * 100);
        ASSERT_EQ(rc, 0);
    }

    /* Iterate using cursor */
    rc = btree_cursor_first(tree, &cursor);
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(btree_cursor_valid(&cursor), 1);

    count = 0;
    while (btree_cursor_valid(&cursor)) {
        rc = btree_cursor_get(&cursor, &key, &value);
        ASSERT_EQ(rc, 0);
        test_printf("  Entry %d: key=%d, value=%u\n", count, key, value);
        ASSERT_EQ(key, count * 10);
        ASSERT_EQ(value, (uint32_t)(count * 100));

        count++;
        btree_cursor_next(&cursor);
    }

    ASSERT_EQ(count, 5);
    test_printf("  Iterated over %d entries\n", count);

    btree_close(tree);
    cache_destroy(cache);
    pager_close(pager);

    TEST_END();
    return 0;
}

/* Test: Insert up to 50 keys (within node capacity) */
TEST(btree_many_keys) {
    struct amidb_pager *pager = NULL;
    struct page_cache *cache;
    struct btree *tree;
    uint32_t root_page;
    uint32_t value;
    uint32_t num_entries, height, num_nodes;
    int rc;
    int i;

    TEST_BEGIN();

    rc = pager_open("RAM:btree_many_keys.db", 0, &pager);
    ASSERT_EQ(rc, 0);

    cache = cache_create(32, pager);
    ASSERT_NOT_NULL(cache);

    tree = btree_create(pager, cache, &root_page);
    ASSERT_NOT_NULL(tree);

    /* Insert 50 entries (should fit in single leaf node with BTREE_ORDER=64) */
    for (i = 0; i < 50; i++) {
        rc = btree_insert(tree, i, i * 10);
        ASSERT_EQ(rc, 0);
    }

    test_printf("  Inserted 50 entries\n");

    /* Get stats */
    btree_get_stats(tree, &num_entries, &height, &num_nodes);
    test_printf("  Stats: entries=%u, height=%u, nodes=%u\n",
                num_entries, height, num_nodes);
    ASSERT_EQ(num_entries, 50);

    /* Verify all entries */
    for (i = 0; i < 50; i++) {
        rc = btree_search(tree, i, &value);
        ASSERT_EQ(rc, 0);
        ASSERT_EQ(value, (uint32_t)(i * 10));
    }

    test_printf("  All 50 entries verified\n");

    btree_close(tree);
    cache_destroy(cache);
    pager_close(pager);

    TEST_END();
    return 0;
}

/* Test: Update existing key */
TEST(btree_update) {
    struct amidb_pager *pager = NULL;
    struct page_cache *cache;
    struct btree *tree;
    uint32_t root_page;
    uint32_t value;
    int rc;

    TEST_BEGIN();

    rc = pager_open("RAM:btree_update.db", 0, &pager);
    ASSERT_EQ(rc, 0);

    cache = cache_create(16, pager);
    ASSERT_NOT_NULL(cache);

    tree = btree_create(pager, cache, &root_page);
    ASSERT_NOT_NULL(tree);

    /* Insert key/value */
    rc = btree_insert(tree, 42, 1000);
    ASSERT_EQ(rc, 0);

    /* Verify initial value */
    rc = btree_search(tree, 42, &value);
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(value, 1000);

    /* Update value */
    rc = btree_insert(tree, 42, 2000);
    ASSERT_EQ(rc, 0);

    /* Verify updated value */
    rc = btree_search(tree, 42, &value);
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(value, 2000);

    test_printf("  Successfully updated key 42: 1000 -> 2000\n");

    btree_close(tree);
    cache_destroy(cache);
    pager_close(pager);

    TEST_END();
    return 0;
}
