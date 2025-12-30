/*
 * test_btree_merge.c - Stress tests for B+Tree merge/borrow operations (Phase 3B)
 */

#include "test_harness.h"
#include "storage/btree.h"
#include "storage/pager.h"
#include "storage/cache.h"
#include "os/file.h"
#include "os/mem.h"
#include <stdio.h>

/* Test: Delete keys to trigger borrow from sibling */
TEST(btree_merge_borrow) {
    struct amidb_pager *pager = NULL;
    struct page_cache *cache;
    struct btree *tree;
    uint32_t root_page;
    uint32_t value;
    uint32_t num_entries, height, num_nodes;
    int rc;
    int i;

    TEST_BEGIN();

    rc = pager_open("RAM:btree_merge_borrow.db", 0, &pager);
    ASSERT_EQ(rc, 0);

    cache = cache_create(32, pager);
    ASSERT_NOT_NULL(cache);

    tree = btree_create(pager, cache, &root_page);
    ASSERT_NOT_NULL(tree);

    /* Insert 100 keys to create multi-node tree */
    for (i = 0; i < 100; i++) {
        rc = btree_insert(tree, i, i * 10);
        ASSERT_EQ(rc, 0);
    }

    test_printf("  Inserted 100 keys\n");

    /* Delete 20 keys from the beginning (should trigger borrow) */
    for (i = 0; i < 20; i++) {
        rc = btree_delete(tree, i);
        ASSERT_EQ(rc, 0);
    }

    test_printf("  Deleted 20 keys from beginning\n");

    /* Verify remaining keys */
    for (i = 20; i < 100; i++) {
        rc = btree_search(tree, i, &value);
        ASSERT_EQ(rc, 0);
        ASSERT_EQ(value, (uint32_t)(i * 10));
    }

    /* Verify deleted keys are gone */
    for (i = 0; i < 20; i++) {
        rc = btree_search(tree, i, &value);
        ASSERT_EQ(rc, -1);
    }

    btree_get_stats(tree, &num_entries, &height, &num_nodes);
    test_printf("  After deletions: entries=%u, height=%u, nodes=%u\n",
                num_entries, height, num_nodes);
    ASSERT_EQ(num_entries, 80);

    btree_close(tree);
    cache_destroy(cache);
    pager_close(pager);

    TEST_END();
    return 0;
}

/* Test: Delete many keys to trigger merges */
TEST(btree_merge_trigger) {
    struct amidb_pager *pager = NULL;
    struct page_cache *cache;
    struct btree *tree;
    uint32_t root_page;
    uint32_t value;
    uint32_t num_entries, height_before, height_after, num_nodes;
    int rc;
    int i;

    TEST_BEGIN();

    rc = pager_open("RAM:btree_merge_trigger.db", 0, &pager);
    ASSERT_EQ(rc, 0);

    cache = cache_create(32, pager);
    ASSERT_NOT_NULL(cache);

    tree = btree_create(pager, cache, &root_page);
    ASSERT_NOT_NULL(tree);

    /* Insert 200 keys */
    for (i = 0; i < 200; i++) {
        rc = btree_insert(tree, i, i * 10);
        ASSERT_EQ(rc, 0);
    }

    test_printf("  Inserted 200 keys\n");
    btree_get_stats(tree, &num_entries, &height_before, &num_nodes);
    test_printf("  Before deletions: entries=%u, height=%u, nodes=%u\n",
                num_entries, height_before, num_nodes);

    /* Delete 150 keys (should trigger multiple merges) */
    for (i = 0; i < 150; i++) {
        rc = btree_delete(tree, i);
        ASSERT_EQ(rc, 0);
    }

    test_printf("  Deleted 150 keys\n");

    /* Verify remaining keys */
    for (i = 150; i < 200; i++) {
        rc = btree_search(tree, i, &value);
        ASSERT_EQ(rc, 0);
        ASSERT_EQ(value, (uint32_t)(i * 10));
    }

    btree_get_stats(tree, &num_entries, &height_after, &num_nodes);
    test_printf("  After deletions: entries=%u, height=%u, nodes=%u\n",
                num_entries, height_after, num_nodes);
    ASSERT_EQ(num_entries, 50);

    btree_close(tree);
    cache_destroy(cache);
    pager_close(pager);

    TEST_END();
    return 0;
}

/* Test: Large scale insert/delete cycle */
TEST(btree_merge_500_delete_400) {
    struct amidb_pager *pager = NULL;
    struct page_cache *cache;
    struct btree *tree;
    uint32_t root_page;
    uint32_t value;
    uint32_t num_entries, height, num_nodes;
    int rc;
    int i;

    TEST_BEGIN();

    rc = pager_open("RAM:btree_merge_large.db", 0, &pager);
    ASSERT_EQ(rc, 0);

    cache = cache_create(64, pager);
    ASSERT_NOT_NULL(cache);

    tree = btree_create(pager, cache, &root_page);
    ASSERT_NOT_NULL(tree);

    /* Insert 500 keys */
    for (i = 0; i < 500; i++) {
        rc = btree_insert(tree, i, i * 10);
        ASSERT_EQ(rc, 0);
    }

    test_printf("  Inserted 500 keys\n");
    btree_get_stats(tree, &num_entries, &height, &num_nodes);
    test_printf("  After insertions: entries=%u, height=%u, nodes=%u\n",
                num_entries, height, num_nodes);

    /* Delete 400 keys (80% deletion rate) */
    for (i = 0; i < 400; i++) {
        rc = btree_delete(tree, i);
        ASSERT_EQ(rc, 0);
    }

    test_printf("  Deleted 400 keys\n");

    /* Verify remaining 100 keys */
    for (i = 400; i < 500; i++) {
        rc = btree_search(tree, i, &value);
        ASSERT_EQ(rc, 0);
        ASSERT_EQ(value, (uint32_t)(i * 10));
    }

    test_printf("  All 100 remaining keys verified\n");

    btree_get_stats(tree, &num_entries, &height, &num_nodes);
    test_printf("  After deletions: entries=%u, height=%u, nodes=%u\n",
                num_entries, height, num_nodes);
    ASSERT_EQ(num_entries, 100);

    btree_close(tree);
    cache_destroy(cache);
    pager_close(pager);

    TEST_END();
    return 0;
}

/* Test: Delete all keys (empty tree) */
TEST(btree_merge_delete_all) {
    struct amidb_pager *pager = NULL;
    struct page_cache *cache;
    struct btree *tree;
    uint32_t root_page;
    uint32_t value;
    uint32_t num_entries, height, num_nodes;
    int rc;
    int i;

    TEST_BEGIN();

    rc = pager_open("RAM:btree_merge_empty.db", 0, &pager);
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

    /* Delete all keys */
    for (i = 0; i < 100; i++) {
        rc = btree_delete(tree, i);
        ASSERT_EQ(rc, 0);
    }

    test_printf("  Deleted all 100 keys\n");

    /* Verify tree is empty */
    btree_get_stats(tree, &num_entries, &height, &num_nodes);
    test_printf("  After all deletions: entries=%u, height=%u, nodes=%u\n",
                num_entries, height, num_nodes);
    ASSERT_EQ(num_entries, 0);

    /* Verify no keys found */
    for (i = 0; i < 100; i++) {
        rc = btree_search(tree, i, &value);
        ASSERT_EQ(rc, -1);
    }

    test_printf("  Tree is empty as expected\n");

    btree_close(tree);
    cache_destroy(cache);
    pager_close(pager);

    TEST_END();
    return 0;
}

/* Test: Delete in reverse order */
TEST(btree_merge_reverse_delete) {
    struct amidb_pager *pager = NULL;
    struct page_cache *cache;
    struct btree *tree;
    uint32_t root_page;
    uint32_t value;
    uint32_t num_entries, height, num_nodes;
    int rc;
    int i;

    TEST_BEGIN();

    rc = pager_open("RAM:btree_merge_reverse.db", 0, &pager);
    ASSERT_EQ(rc, 0);

    cache = cache_create(32, pager);
    ASSERT_NOT_NULL(cache);

    tree = btree_create(pager, cache, &root_page);
    ASSERT_NOT_NULL(tree);

    /* Insert 200 keys */
    for (i = 0; i < 200; i++) {
        rc = btree_insert(tree, i, i * 10);
        ASSERT_EQ(rc, 0);
    }

    test_printf("  Inserted 200 keys\n");

    /* Delete in reverse order (from largest to smallest) */
    for (i = 199; i >= 100; i--) {
        rc = btree_delete(tree, i);
        ASSERT_EQ(rc, 0);
    }

    test_printf("  Deleted 100 keys in reverse order\n");

    /* Verify remaining keys */
    for (i = 0; i < 100; i++) {
        rc = btree_search(tree, i, &value);
        ASSERT_EQ(rc, 0);
        ASSERT_EQ(value, (uint32_t)(i * 10));
    }

    btree_get_stats(tree, &num_entries, &height, &num_nodes);
    test_printf("  After deletions: entries=%u, height=%u, nodes=%u\n",
                num_entries, height, num_nodes);
    ASSERT_EQ(num_entries, 100);

    btree_close(tree);
    cache_destroy(cache);
    pager_close(pager);

    TEST_END();
    return 0;
}
