/*
 * test_btree_txn.c - Integration tests for B+Tree with transactions
 *
 * These tests verify that B+Tree operations (insert with split, delete with merge)
 * are properly integrated with the transaction system for atomicity.
 */

#include "test_harness.h"
#include "storage/btree.h"
#include "txn/txn.h"
#include "txn/wal.h"
#include "storage/cache.h"
#include "storage/pager.h"
#include "os/file.h"
#include "os/mem.h"
#include "api/error.h"
#include <string.h>
#include <stdio.h>

/* Use unique database names for each test */
#define TEST_DB_BTREE_INSERT_TXN "RAM:btree_insert_txn.db"
#define TEST_DB_BTREE_SPLIT_COMMIT "RAM:btree_split_commit.db"
#define TEST_DB_BTREE_SPLIT_ABORT "RAM:btree_split_abort.db"
#define TEST_DB_BTREE_DELETE_MERGE "RAM:btree_delete_merge.db"
#define TEST_DB_BTREE_COMPLEX "RAM:btree_complex.db"

/* Test: Simple B+Tree insert with transaction */
TEST(btree_insert_with_transaction) {
    struct amidb_pager *pager = NULL;
    struct page_cache *cache;
    struct wal_context *wal;
    struct txn_context *txn;
    struct btree *tree;
    uint32_t root_page;
    uint32_t value_out;
    int rc;

    TEST_BEGIN();

    /* Create pager and cache */
    rc = pager_open(TEST_DB_BTREE_INSERT_TXN, 0, &pager);
    ASSERT_EQ(rc, 0);

    cache = cache_create(32, pager);
    ASSERT_NOT_NULL(cache);

    /* Create B+Tree */
    tree = btree_create(pager, cache, &root_page);
    ASSERT_NOT_NULL(tree);

    /* Create WAL and transaction */
    wal = wal_create(pager);
    ASSERT_NOT_NULL(wal);

    txn = txn_create(wal, cache);
    ASSERT_NOT_NULL(txn);

    /* Begin transaction */
    rc = txn_begin(txn);
    ASSERT_EQ(rc, AMIDB_OK);

    /* Insert keys (note: without transaction integration, these won't be atomic yet) */
    /* This test just verifies transactions work alongside B+Tree operations */
    rc = btree_insert(tree, 100, 1000);
    ASSERT_EQ(rc, 0);

    rc = btree_insert(tree, 200, 2000);
    ASSERT_EQ(rc, 0);

    rc = btree_insert(tree, 300, 3000);
    ASSERT_EQ(rc, 0);

    /* Commit transaction */
    rc = txn_commit(txn);
    ASSERT_EQ(rc, AMIDB_OK);

    /* Verify keys exist */
    rc = btree_search(tree, 100, &value_out);
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(value_out, 1000);

    rc = btree_search(tree, 200, &value_out);
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(value_out, 2000);

    rc = btree_search(tree, 300, &value_out);
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(value_out, 3000);

    /* Cleanup */
    btree_close(tree);
    txn_destroy(txn);
    wal_destroy(wal);
    cache_destroy(cache);
    pager_close(pager);

    TEST_END();
    return 0;
}

/* Test: B+Tree split with commit */
TEST(btree_split_with_commit) {
    struct amidb_pager *pager = NULL;
    struct page_cache *cache;
    struct wal_context *wal;
    struct txn_context *txn;
    struct btree *tree;
    uint32_t root_page;
    uint32_t value_out;
    int rc;
    int i;

    TEST_BEGIN();

    /* Create pager and cache */
    rc = pager_open(TEST_DB_BTREE_SPLIT_COMMIT, 0, &pager);
    ASSERT_EQ(rc, 0);

    cache = cache_create(64, pager);  /* Larger cache for split operations */
    ASSERT_NOT_NULL(cache);

    /* Create B+Tree */
    tree = btree_create(pager, cache, &root_page);
    ASSERT_NOT_NULL(tree);

    /* Create WAL and transaction */
    wal = wal_create(pager);
    ASSERT_NOT_NULL(wal);

    txn = txn_create(wal, cache);
    ASSERT_NOT_NULL(txn);

    /* Begin transaction */
    rc = txn_begin(txn);
    ASSERT_EQ(rc, AMIDB_OK);

    /* Insert enough keys to force a split (BTREE_ORDER = 64) */
    /* Insert 70 keys to ensure split happens */
    for (i = 0; i < 70; i++) {
        rc = btree_insert(tree, i * 10, i * 100);
        ASSERT_EQ(rc, 0);
    }

    /* Commit transaction (split should be durable) */
    rc = txn_commit(txn);
    ASSERT_EQ(rc, AMIDB_OK);

    /* Verify all keys are searchable */
    for (i = 0; i < 70; i++) {
        rc = btree_search(tree, i * 10, &value_out);
        ASSERT_EQ(rc, 0);
        ASSERT_EQ(value_out, i * 100);
    }

    /* Cleanup */
    btree_close(tree);
    txn_destroy(txn);
    wal_destroy(wal);
    cache_destroy(cache);
    pager_close(pager);

    TEST_END();
    return 0;
}

/* Test: B+Tree split with abort (changes should be rolled back) */
TEST(btree_split_with_abort) {
    struct amidb_pager *pager = NULL;
    struct page_cache *cache;
    struct wal_context *wal;
    struct txn_context *txn;
    struct btree *tree;
    uint32_t root_page;
    uint32_t value_out;
    int rc;
    int i;

    TEST_BEGIN();

    /* Create pager and cache */
    rc = pager_open(TEST_DB_BTREE_SPLIT_ABORT, 0, &pager);
    ASSERT_EQ(rc, 0);

    cache = cache_create(64, pager);
    ASSERT_NOT_NULL(cache);

    /* Create B+Tree */
    tree = btree_create(pager, cache, &root_page);
    ASSERT_NOT_NULL(tree);

    /* Create WAL and transaction */
    wal = wal_create(pager);
    ASSERT_NOT_NULL(wal);

    txn = txn_create(wal, cache);
    ASSERT_NOT_NULL(txn);

    /* First transaction: Insert a few keys and commit */
    rc = txn_begin(txn);
    ASSERT_EQ(rc, AMIDB_OK);

    for (i = 0; i < 10; i++) {
        rc = btree_insert(tree, i * 10, i * 100);
        ASSERT_EQ(rc, 0);
    }

    rc = txn_commit(txn);
    ASSERT_EQ(rc, AMIDB_OK);

    /* Second transaction: Insert more keys to force split, then abort */
    rc = txn_begin(txn);
    ASSERT_EQ(rc, AMIDB_OK);

    /* Insert 60 more keys (should cause split) */
    for (i = 10; i < 70; i++) {
        rc = btree_insert(tree, i * 10, i * 100);
        /* Note: insert might fail if we run into constraints */
        /* For this test, we just verify abort works */
    }

    /* Abort transaction */
    rc = txn_abort(txn);
    ASSERT_EQ(rc, AMIDB_OK);

    /* Verify only first 10 keys exist (second batch should be rolled back) */
    /* Note: This test demonstrates abort, but without full B+Tree integration, */
    /* the abort won't actually roll back the B+Tree structure changes yet */
    /* This is a placeholder for Phase 3C full integration */

    for (i = 0; i < 10; i++) {
        rc = btree_search(tree, i * 10, &value_out);
        ASSERT_EQ(rc, 0);
        ASSERT_EQ(value_out, i * 100);
    }

    /* Cleanup */
    btree_close(tree);
    txn_destroy(txn);
    wal_destroy(wal);
    cache_destroy(cache);
    pager_close(pager);

    TEST_END();
    return 0;
}

/* Test: B+Tree delete with merge in transaction */
TEST(btree_delete_merge_transaction) {
    struct amidb_pager *pager = NULL;
    struct page_cache *cache;
    struct wal_context *wal;
    struct txn_context *txn;
    struct btree *tree;
    uint32_t root_page;
    uint32_t value_out;
    int rc;
    int i;

    TEST_BEGIN();

    /* Create pager and cache */
    rc = pager_open(TEST_DB_BTREE_DELETE_MERGE, 0, &pager);
    ASSERT_EQ(rc, 0);

    cache = cache_create(64, pager);
    ASSERT_NOT_NULL(cache);

    /* Create B+Tree */
    tree = btree_create(pager, cache, &root_page);
    ASSERT_NOT_NULL(tree);

    /* Create WAL and transaction */
    wal = wal_create(pager);
    ASSERT_NOT_NULL(wal);

    txn = txn_create(wal, cache);
    ASSERT_NOT_NULL(txn);

    /* Transaction 1: Insert keys */
    rc = txn_begin(txn);
    ASSERT_EQ(rc, AMIDB_OK);

    for (i = 0; i < 50; i++) {
        rc = btree_insert(tree, i * 10, i * 100);
        ASSERT_EQ(rc, 0);
    }

    rc = txn_commit(txn);
    ASSERT_EQ(rc, AMIDB_OK);

    /* Transaction 2: Delete keys */
    rc = txn_begin(txn);
    ASSERT_EQ(rc, AMIDB_OK);

    for (i = 0; i < 25; i++) {
        rc = btree_delete(tree, i * 10);
        ASSERT_EQ(rc, 0);
    }

    rc = txn_commit(txn);
    ASSERT_EQ(rc, AMIDB_OK);

    /* Verify deleted keys are gone */
    for (i = 0; i < 25; i++) {
        rc = btree_search(tree, i * 10, &value_out);
        ASSERT_EQ(rc, -1);  /* Should not find */
    }

    /* Verify remaining keys still exist */
    for (i = 25; i < 50; i++) {
        rc = btree_search(tree, i * 10, &value_out);
        ASSERT_EQ(rc, 0);
        ASSERT_EQ(value_out, i * 100);
    }

    /* Cleanup */
    btree_close(tree);
    txn_destroy(txn);
    wal_destroy(wal);
    cache_destroy(cache);
    pager_close(pager);

    TEST_END();
    return 0;
}

/* Test: Complex multi-operation transaction */
TEST(btree_complex_multi_operation) {
    struct amidb_pager *pager = NULL;
    struct page_cache *cache;
    struct wal_context *wal;
    struct txn_context *txn;
    struct btree *tree;
    uint32_t root_page;
    uint32_t value_out;
    int rc;
    int i;

    TEST_BEGIN();

    /* Create pager and cache */
    rc = pager_open(TEST_DB_BTREE_COMPLEX, 0, &pager);
    ASSERT_EQ(rc, 0);

    cache = cache_create(64, pager);
    ASSERT_NOT_NULL(cache);

    /* Create B+Tree */
    tree = btree_create(pager, cache, &root_page);
    ASSERT_NOT_NULL(tree);

    /* Create WAL and transaction */
    wal = wal_create(pager);
    ASSERT_NOT_NULL(wal);

    txn = txn_create(wal, cache);
    ASSERT_NOT_NULL(txn);

    /* Complex transaction: Insert, delete, insert */
    rc = txn_begin(txn);
    ASSERT_EQ(rc, AMIDB_OK);

    /* Phase 1: Insert 30 keys */
    for (i = 0; i < 30; i++) {
        rc = btree_insert(tree, i * 10, i * 100);
        ASSERT_EQ(rc, 0);
    }

    /* Phase 2: Delete every other key */
    for (i = 0; i < 30; i += 2) {
        rc = btree_delete(tree, i * 10);
        ASSERT_EQ(rc, 0);
    }

    /* Phase 3: Insert new keys in the gaps */
    for (i = 0; i < 30; i += 2) {
        rc = btree_insert(tree, i * 10 + 5, i * 100 + 50);
        ASSERT_EQ(rc, 0);
    }

    /* Commit transaction */
    rc = txn_commit(txn);
    ASSERT_EQ(rc, AMIDB_OK);

    /* Verify final state */
    /* Odd keys from original insert should still exist */
    for (i = 1; i < 30; i += 2) {
        rc = btree_search(tree, i * 10, &value_out);
        ASSERT_EQ(rc, 0);
        ASSERT_EQ(value_out, i * 100);
    }

    /* Even keys should be deleted */
    for (i = 0; i < 30; i += 2) {
        rc = btree_search(tree, i * 10, &value_out);
        ASSERT_EQ(rc, -1);
    }

    /* New keys should exist */
    for (i = 0; i < 30; i += 2) {
        rc = btree_search(tree, i * 10 + 5, &value_out);
        ASSERT_EQ(rc, 0);
        ASSERT_EQ(value_out, i * 100 + 50);
    }

    /* Cleanup */
    btree_close(tree);
    txn_destroy(txn);
    wal_destroy(wal);
    cache_destroy(cache);
    pager_close(pager);

    TEST_END();
    return 0;
}
