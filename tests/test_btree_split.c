/*
 * test_btree_split.c - Stress tests for B+Tree split operations (Phase 3B)
 */

#include "test_harness.h"
#include "storage/btree.h"
#include "storage/pager.h"
#include "storage/cache.h"
#include "os/file.h"
#include "os/mem.h"
#include <stdio.h>

/* Test: Insert 100 keys (forces multiple splits) */
TEST(btree_split_100_keys) {
    struct amidb_pager *pager = NULL;
    struct page_cache *cache;
    struct btree *tree;
    uint32_t root_page;
    uint32_t value;
    uint32_t num_entries, height, num_nodes;
    int rc;
    int i;

    TEST_BEGIN();

    rc = pager_open("RAM:btree_split_100.db", 0, &pager);
    ASSERT_EQ(rc, 0);

    cache = cache_create(32, pager);
    ASSERT_NOT_NULL(cache);

    tree = btree_create(pager, cache, &root_page);
    ASSERT_NOT_NULL(tree);

    /* Insert 100 keys */
    for (i = 0; i < 100; i++) {
        rc = btree_insert(tree, i, i * 10);
        ASSERT_EQ(rc, 0);
    }

    test_printf("  Inserted 100 keys\n");

    /* Get stats */
    btree_get_stats(tree, &num_entries, &height, &num_nodes);
    test_printf("  Stats: entries=%u, height=%u, nodes=%u\n",
                num_entries, height, num_nodes);

    ASSERT_EQ(num_entries, 100);

    /* Verify all keys */
    for (i = 0; i < 100; i++) {
        rc = btree_search(tree, i, &value);
        ASSERT_EQ(rc, 0);
        ASSERT_EQ(value, (uint32_t)(i * 10));
    }

    test_printf("  All 100 keys verified\n");

    btree_close(tree);
    cache_destroy(cache);
    pager_close(pager);

    TEST_END();
    return 0;
}

/* Test: Insert 500 keys (creates multi-level tree) */
TEST(btree_split_500_keys) {
    struct amidb_pager *pager = NULL;
    struct page_cache *cache;
    struct btree *tree;
    uint32_t root_page;
    uint32_t value;
    uint32_t num_entries, height, num_nodes;
    int rc;
    int i;

    TEST_BEGIN();

    rc = pager_open("RAM:btree_split_500.db", 0, &pager);
    ASSERT_EQ(rc, 0);

    cache = cache_create(64, pager);
    ASSERT_NOT_NULL(cache);

    tree = btree_create(pager, cache, &root_page);
    ASSERT_NOT_NULL(tree);

    /* Insert 500 keys */
    for (i = 0; i < 500; i++) {
        rc = btree_insert(tree, i, i * 100);
        if (rc != 0) {
            test_printf("  FAIL: Insert failed at key %d\n", i);
            ASSERT_EQ(rc, 0);
        }
    }

    test_printf("  Inserted 500 keys\n");

    /* Get stats */
    btree_get_stats(tree, &num_entries, &height, &num_nodes);
    test_printf("  Stats: entries=%u, height=%u, nodes=%u\n",
                num_entries, height, num_nodes);

    ASSERT_EQ(num_entries, 500);

    /* Verify all keys */
    for (i = 0; i < 500; i++) {
        rc = btree_search(tree, i, &value);
        if (rc != 0) {
            test_printf("  FAIL: Search failed for key %d\n", i);
            ASSERT_EQ(rc, 0);
        }
        ASSERT_EQ(value, (uint32_t)(i * 100));
    }

    test_printf("  All 500 keys verified\n");

    btree_close(tree);
    cache_destroy(cache);
    pager_close(pager);

    TEST_END();
    return 0;
}

/* Test: Insert keys in reverse order */
TEST(btree_split_reverse_500) {
    struct amidb_pager *pager = NULL;
    struct page_cache *cache;
    struct btree *tree;
    uint32_t root_page;
    uint32_t value;
    uint32_t num_entries, height, num_nodes;
    int rc;
    int i;

    TEST_BEGIN();

    rc = pager_open("RAM:btree_split_reverse.db", 0, &pager);
    ASSERT_EQ(rc, 0);

    cache = cache_create(64, pager);
    ASSERT_NOT_NULL(cache);

    tree = btree_create(pager, cache, &root_page);
    ASSERT_NOT_NULL(tree);

    /* Insert 500 keys in reverse order */
    for (i = 499; i >= 0; i--) {
        rc = btree_insert(tree, i, i * 100);
        if (rc != 0) {
            test_printf("  FAIL: Insert failed at key %d\n", i);
            ASSERT_EQ(rc, 0);
        }
    }

    test_printf("  Inserted 500 keys in reverse order\n");

    /* Get stats */
    btree_get_stats(tree, &num_entries, &height, &num_nodes);
    test_printf("  Stats: entries=%u, height=%u, nodes=%u\n",
                num_entries, height, num_nodes);

    ASSERT_EQ(num_entries, 500);

    /* Verify all keys */
    for (i = 0; i < 500; i++) {
        rc = btree_search(tree, i, &value);
        if (rc != 0) {
            test_printf("  FAIL: Search failed for key %d\n", i);
            ASSERT_EQ(rc, 0);
        }
        ASSERT_EQ(value, (uint32_t)(i * 100));
    }

    test_printf("  All 500 keys verified\n");

    btree_close(tree);
    cache_destroy(cache);
    pager_close(pager);

    TEST_END();
    return 0;
}

/* Test: Cursor iteration after splits */
TEST(btree_split_cursor_iteration) {
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

    rc = pager_open("RAM:btree_split_cursor.db", 0, &pager);
    ASSERT_EQ(rc, 0);

    cache = cache_create(32, pager);
    ASSERT_NOT_NULL(cache);

    tree = btree_create(pager, cache, &root_page);
    ASSERT_NOT_NULL(tree);

    /* Insert 100 keys */
    for (i = 0; i < 100; i++) {
        rc = btree_insert(tree, i * 2, i);  /* Even numbers only */
        ASSERT_EQ(rc, 0);
    }

    test_printf("  Inserted 100 keys\n");

    /* Iterate using cursor */
    rc = btree_cursor_first(tree, &cursor);
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(btree_cursor_valid(&cursor), 1);

    count = 0;
    while (btree_cursor_valid(&cursor)) {
        rc = btree_cursor_get(&cursor, &key, &value);
        ASSERT_EQ(rc, 0);
        ASSERT_EQ(key, count * 2);
        ASSERT_EQ(value, (uint32_t)count);

        count++;
        btree_cursor_next(&cursor);
    }

    ASSERT_EQ(count, 100);
    test_printf("  Cursor iterated over %d entries correctly\n", count);

    btree_close(tree);
    cache_destroy(cache);
    pager_close(pager);

    TEST_END();
    return 0;
}

/* Test: Update keys after splits */
TEST(btree_split_update_after_split) {
    struct amidb_pager *pager = NULL;
    struct page_cache *cache;
    struct btree *tree;
    uint32_t root_page;
    uint32_t value;
    int rc;
    int i;

    TEST_BEGIN();

    rc = pager_open("RAM:btree_split_update.db", 0, &pager);
    ASSERT_EQ(rc, 0);

    cache = cache_create(32, pager);
    ASSERT_NOT_NULL(cache);

    tree = btree_create(pager, cache, &root_page);
    ASSERT_NOT_NULL(tree);

    /* Insert 100 keys */
    for (i = 0; i < 100; i++) {
        rc = btree_insert(tree, i, i);
        ASSERT_EQ(rc, 0);
    }

    test_printf("  Inserted 100 keys\n");

    /* Update all keys */
    for (i = 0; i < 100; i++) {
        rc = btree_insert(tree, i, i * 1000);
        ASSERT_EQ(rc, 0);
    }

    test_printf("  Updated all 100 keys\n");

    /* Verify updated values */
    for (i = 0; i < 100; i++) {
        rc = btree_search(tree, i, &value);
        ASSERT_EQ(rc, 0);
        ASSERT_EQ(value, (uint32_t)(i * 1000));
    }

    test_printf("  All updates verified\n");

    btree_close(tree);
    cache_destroy(cache);
    pager_close(pager);

    TEST_END();
    return 0;
}
