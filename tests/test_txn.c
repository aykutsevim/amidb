/*
 * test_txn.c - Unit tests for Transaction Manager
 */

#include "test_harness.h"
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
#define TEST_DB_TXN_BEGIN_COMMIT "RAM:txn_begin_commit.db"
#define TEST_DB_TXN_BEGIN_ABORT "RAM:txn_begin_abort.db"
#define TEST_DB_TXN_DIRTY_TRACK "RAM:txn_dirty_track.db"
#define TEST_DB_TXN_PIN "RAM:txn_pin.db"
#define TEST_DB_TXN_MULTI_PAGE "RAM:txn_multi_page.db"
#define TEST_DB_TXN_NESTED "RAM:txn_nested.db"
#define TEST_DB_TXN_DURABILITY "RAM:txn_durability.db"
#define TEST_DB_TXN_ISOLATION "RAM:txn_isolation.db"

/* Test: Begin and commit transaction */
TEST(txn_begin_commit) {
    struct amidb_pager *pager = NULL;
    struct page_cache *cache;
    struct wal_context *wal;
    struct txn_context *txn;
    int rc;

    TEST_BEGIN();

    /* Create pager and cache */
    rc = pager_open(TEST_DB_TXN_BEGIN_COMMIT, 0, &pager);
    ASSERT_EQ(rc, 0);

    cache = cache_create(16, pager);
    ASSERT_NOT_NULL(cache);

    /* Create WAL and transaction */
    wal = wal_create(pager);
    ASSERT_NOT_NULL(wal);

    txn = txn_create(wal, cache);
    ASSERT_NOT_NULL(txn);
    ASSERT_EQ(txn->state, TXN_STATE_IDLE);

    /* Begin transaction */
    rc = txn_begin(txn);
    ASSERT_EQ(rc, AMIDB_OK);
    ASSERT_EQ(txn->state, TXN_STATE_ACTIVE);
    ASSERT_EQ(txn->txn_id, 1);

    /* Commit transaction */
    rc = txn_commit(txn);
    ASSERT_EQ(rc, AMIDB_OK);
    ASSERT_EQ(txn->state, TXN_STATE_IDLE);

    /* Cleanup */
    txn_destroy(txn);
    wal_destroy(wal);
    cache_destroy(cache);
    pager_close(pager);

    TEST_END();
    return 0;
}

/* Test: Begin and abort transaction */
TEST(txn_begin_abort) {
    struct amidb_pager *pager = NULL;
    struct page_cache *cache;
    struct wal_context *wal;
    struct txn_context *txn;
    int rc;

    TEST_BEGIN();

    /* Create pager and cache */
    rc = pager_open(TEST_DB_TXN_BEGIN_ABORT, 0, &pager);
    ASSERT_EQ(rc, 0);

    cache = cache_create(16, pager);
    ASSERT_NOT_NULL(cache);

    /* Create WAL and transaction */
    wal = wal_create(pager);
    ASSERT_NOT_NULL(wal);

    txn = txn_create(wal, cache);
    ASSERT_NOT_NULL(txn);

    /* Begin transaction */
    rc = txn_begin(txn);
    ASSERT_EQ(rc, AMIDB_OK);
    ASSERT_EQ(txn->state, TXN_STATE_ACTIVE);

    /* Abort transaction */
    rc = txn_abort(txn);
    ASSERT_EQ(rc, AMIDB_OK);
    ASSERT_EQ(txn->state, TXN_STATE_IDLE);
    ASSERT_EQ(txn->dirty_count, 0);

    /* Cleanup */
    txn_destroy(txn);
    wal_destroy(wal);
    cache_destroy(cache);
    pager_close(pager);

    TEST_END();
    return 0;
}

/* Test: Dirty page tracking */
TEST(txn_dirty_page_tracking) {
    struct amidb_pager *pager = NULL;
    struct page_cache *cache;
    struct wal_context *wal;
    struct txn_context *txn;
    uint32_t page1, page2, page3;
    int rc;

    TEST_BEGIN();

    /* Create pager and cache */
    rc = pager_open(TEST_DB_TXN_DIRTY_TRACK, 0, &pager);
    ASSERT_EQ(rc, 0);

    cache = cache_create(16, pager);
    ASSERT_NOT_NULL(cache);

    /* Allocate test pages */
    rc = pager_allocate_page(pager, &page1);
    ASSERT_EQ(rc, 0);
    rc = pager_allocate_page(pager, &page2);
    ASSERT_EQ(rc, 0);
    rc = pager_allocate_page(pager, &page3);
    ASSERT_EQ(rc, 0);

    /* Create WAL and transaction */
    wal = wal_create(pager);
    ASSERT_NOT_NULL(wal);

    txn = txn_create(wal, cache);
    ASSERT_NOT_NULL(txn);

    /* Begin transaction */
    rc = txn_begin(txn);
    ASSERT_EQ(rc, AMIDB_OK);

    /* Add dirty pages */
    rc = txn_add_dirty_page(txn, page1);
    ASSERT_EQ(rc, AMIDB_OK);
    ASSERT_EQ(txn->dirty_count, 1);

    rc = txn_add_dirty_page(txn, page2);
    ASSERT_EQ(rc, AMIDB_OK);
    ASSERT_EQ(txn->dirty_count, 2);

    rc = txn_add_dirty_page(txn, page3);
    ASSERT_EQ(rc, AMIDB_OK);
    ASSERT_EQ(txn->dirty_count, 3);

    /* Adding duplicate should not increase count */
    rc = txn_add_dirty_page(txn, page1);
    ASSERT_EQ(rc, AMIDB_OK);
    ASSERT_EQ(txn->dirty_count, 3);

    /* Check dirty status */
    ASSERT_EQ(txn_is_page_dirty(txn, page1), 1);
    ASSERT_EQ(txn_is_page_dirty(txn, page2), 1);
    ASSERT_EQ(txn_is_page_dirty(txn, 999), 0);

    /* Abort to cleanup */
    rc = txn_abort(txn);
    ASSERT_EQ(rc, AMIDB_OK);
    ASSERT_EQ(txn->dirty_count, 0);

    /* Cleanup */
    txn_destroy(txn);
    wal_destroy(wal);
    cache_destroy(cache);
    pager_close(pager);

    TEST_END();
    return 0;
}

/* Test: Pin during transaction */
TEST(txn_pin_during_transaction) {
    struct amidb_pager *pager = NULL;
    struct page_cache *cache;
    struct wal_context *wal;
    struct txn_context *txn;
    uint32_t page_num;
    uint8_t *data;
    struct cache_entry *entry;
    int rc;

    TEST_BEGIN();

    /* Create pager and cache */
    rc = pager_open(TEST_DB_TXN_PIN, 0, &pager);
    ASSERT_EQ(rc, 0);

    cache = cache_create(4, pager);  /* Small cache */
    ASSERT_NOT_NULL(cache);

    /* Allocate page */
    rc = pager_allocate_page(pager, &page_num);
    ASSERT_EQ(rc, 0);

    /* Create WAL and transaction */
    wal = wal_create(pager);
    ASSERT_NOT_NULL(wal);

    txn = txn_create(wal, cache);
    ASSERT_NOT_NULL(txn);

    /* Begin transaction */
    rc = txn_begin(txn);
    ASSERT_EQ(rc, AMIDB_OK);

    /* Get page (auto-pinned) */
    rc = cache_get_page(cache, page_num, &data);
    ASSERT_EQ(rc, 0);

    /* Mark as dirty and add to transaction */
    cache_mark_dirty(cache, page_num);
    txn_add_dirty_page(txn, page_num);

    /* Set txn_id on cache entry */
    entry = cache_find_entry(cache, page_num);
    ASSERT_NOT_NULL(entry);
    entry->txn_id = txn->txn_id;

    /* Verify page is pinned and has txn_id */
    ASSERT_GT(entry->pin_count, 0);
    ASSERT_EQ(entry->txn_id, txn->txn_id);

    /* Commit should unpin and clear txn_id */
    rc = txn_commit(txn);
    ASSERT_EQ(rc, AMIDB_OK);
    ASSERT_EQ(entry->txn_id, 0);

    /* Cleanup */
    txn_destroy(txn);
    wal_destroy(wal);
    cache_destroy(cache);
    pager_close(pager);

    TEST_END();
    return 0;
}

/* Test: Multi-page commit */
TEST(txn_multi_page_commit) {
    struct amidb_pager *pager = NULL;
    struct page_cache *cache;
    struct wal_context *wal;
    struct txn_context *txn;
    uint32_t pages[5];
    uint8_t *data;
    struct cache_entry *entry;
    int rc;
    int i;

    TEST_BEGIN();

    /* Create pager and cache */
    rc = pager_open(TEST_DB_TXN_MULTI_PAGE, 0, &pager);
    ASSERT_EQ(rc, 0);

    cache = cache_create(16, pager);
    ASSERT_NOT_NULL(cache);

    /* Allocate 5 pages */
    for (i = 0; i < 5; i++) {
        rc = pager_allocate_page(pager, &pages[i]);
        ASSERT_EQ(rc, 0);
    }

    /* Create WAL and transaction */
    wal = wal_create(pager);
    ASSERT_NOT_NULL(wal);

    txn = txn_create(wal, cache);
    ASSERT_NOT_NULL(txn);

    /* Begin transaction */
    rc = txn_begin(txn);
    ASSERT_EQ(rc, AMIDB_OK);

    /* Load and modify all pages */
    for (i = 0; i < 5; i++) {
        rc = cache_get_page(cache, pages[i], &data);
        ASSERT_EQ(rc, 0);

        /* Write test pattern */
        memset(data + 12, 0x40 + i, 100);

        /* Mark dirty and add to transaction */
        cache_mark_dirty(cache, pages[i]);
        txn_add_dirty_page(txn, pages[i]);

        entry = cache_find_entry(cache, pages[i]);
        ASSERT_NOT_NULL(entry);
        entry->txn_id = txn->txn_id;
    }

    ASSERT_EQ(txn->dirty_count, 5);

    /* Commit transaction */
    rc = txn_commit(txn);
    ASSERT_EQ(rc, AMIDB_OK);
    ASSERT_EQ(txn->dirty_count, 0);

    /* Verify all pages are clean */
    for (i = 0; i < 5; i++) {
        entry = cache_find_entry(cache, pages[i]);
        if (entry) {
            ASSERT_EQ(entry->state, CACHE_ENTRY_CLEAN);
            ASSERT_EQ(entry->txn_id, 0);
        }
    }

    /* Cleanup */
    txn_destroy(txn);
    wal_destroy(wal);
    cache_destroy(cache);
    pager_close(pager);

    TEST_END();
    return 0;
}

/* Test: Nested transaction (should fail) */
TEST(txn_nested_abort) {
    struct amidb_pager *pager = NULL;
    struct page_cache *cache;
    struct wal_context *wal;
    struct txn_context *txn;
    int rc;

    TEST_BEGIN();

    /* Create pager and cache */
    rc = pager_open(TEST_DB_TXN_NESTED, 0, &pager);
    ASSERT_EQ(rc, 0);

    cache = cache_create(16, pager);
    ASSERT_NOT_NULL(cache);

    /* Create WAL and transaction */
    wal = wal_create(pager);
    ASSERT_NOT_NULL(wal);

    txn = txn_create(wal, cache);
    ASSERT_NOT_NULL(txn);

    /* Begin first transaction */
    rc = txn_begin(txn);
    ASSERT_EQ(rc, AMIDB_OK);
    ASSERT_EQ(txn->state, TXN_STATE_ACTIVE);

    /* Try to begin nested transaction (should fail) */
    rc = txn_begin(txn);
    ASSERT_EQ(rc, AMIDB_BUSY);
    ASSERT_EQ(txn->state, TXN_STATE_ACTIVE);

    /* Abort first transaction */
    rc = txn_abort(txn);
    ASSERT_EQ(rc, AMIDB_OK);

    /* Now begin should work */
    rc = txn_begin(txn);
    ASSERT_EQ(rc, AMIDB_OK);

    rc = txn_commit(txn);
    ASSERT_EQ(rc, AMIDB_OK);

    /* Cleanup */
    txn_destroy(txn);
    wal_destroy(wal);
    cache_destroy(cache);
    pager_close(pager);

    TEST_END();
    return 0;
}

/* Test: Commit durability */
TEST(txn_commit_durability) {
    struct amidb_pager *pager = NULL;
    struct page_cache *cache;
    struct wal_context *wal;
    struct txn_context *txn;
    uint32_t page_num;
    uint8_t *data;
    struct cache_entry *entry;
    int rc;

    TEST_BEGIN();

    /* Create pager and cache */
    rc = pager_open(TEST_DB_TXN_DURABILITY, 0, &pager);
    ASSERT_EQ(rc, 0);

    cache = cache_create(16, pager);
    ASSERT_NOT_NULL(cache);

    /* Allocate page */
    rc = pager_allocate_page(pager, &page_num);
    ASSERT_EQ(rc, 0);

    /* Create WAL and transaction */
    wal = wal_create(pager);
    ASSERT_NOT_NULL(wal);

    txn = txn_create(wal, cache);
    ASSERT_NOT_NULL(txn);

    /* Begin transaction */
    rc = txn_begin(txn);
    ASSERT_EQ(rc, AMIDB_OK);

    /* Modify page */
    rc = cache_get_page(cache, page_num, &data);
    ASSERT_EQ(rc, 0);
    memset(data + 12, 0xAB, 100);

    cache_mark_dirty(cache, page_num);
    txn_add_dirty_page(txn, page_num);

    entry = cache_find_entry(cache, page_num);
    entry->txn_id = txn->txn_id;

    /* Commit transaction */
    rc = txn_commit(txn);
    ASSERT_EQ(rc, AMIDB_OK);

    /* Verify WAL was reset (eager checkpoint) */
    ASSERT_EQ(wal->buffer_used, 0);
    ASSERT_EQ(wal->wal_head, 0);

    /* Verify page is clean */
    entry = cache_find_entry(cache, page_num);
    ASSERT_NOT_NULL(entry);
    ASSERT_EQ(entry->state, CACHE_ENTRY_CLEAN);

    /* Cleanup */
    txn_destroy(txn);
    wal_destroy(wal);
    cache_destroy(cache);
    pager_close(pager);

    TEST_END();
    return 0;
}

/* Test: Transaction isolation (dirty pages in txn) */
TEST(txn_isolation) {
    struct amidb_pager *pager = NULL;
    struct page_cache *cache;
    struct wal_context *wal;
    struct txn_context *txn;
    uint32_t page_num;
    uint8_t *data;
    uint8_t original_value;
    struct cache_entry *entry;
    int rc;

    TEST_BEGIN();

    /* Create pager and cache */
    rc = pager_open(TEST_DB_TXN_ISOLATION, 0, &pager);
    ASSERT_EQ(rc, 0);

    cache = cache_create(16, pager);
    ASSERT_NOT_NULL(cache);

    /* Allocate page and write initial value */
    rc = pager_allocate_page(pager, &page_num);
    ASSERT_EQ(rc, 0);

    rc = cache_get_page(cache, page_num, &data);
    ASSERT_EQ(rc, 0);
    data[12] = 0x11;
    original_value = data[12];
    cache_mark_dirty(cache, page_num);
    cache_unpin(cache, page_num);
    cache_flush(cache);

    /* Create WAL and transaction */
    wal = wal_create(pager);
    ASSERT_NOT_NULL(wal);

    txn = txn_create(wal, cache);
    ASSERT_NOT_NULL(txn);

    /* Begin transaction */
    rc = txn_begin(txn);
    ASSERT_EQ(rc, AMIDB_OK);

    /* Modify page in transaction */
    rc = cache_get_page(cache, page_num, &data);
    ASSERT_EQ(rc, 0);
    data[12] = 0x99;

    cache_mark_dirty(cache, page_num);
    txn_add_dirty_page(txn, page_num);

    entry = cache_find_entry(cache, page_num);
    entry->txn_id = txn->txn_id;

    ASSERT_EQ(data[12], 0x99);

    /* Abort transaction */
    rc = txn_abort(txn);
    ASSERT_EQ(rc, AMIDB_OK);

    /* Verify page was restored to original value */
    entry = cache_find_entry(cache, page_num);
    ASSERT_NOT_NULL(entry);
    ASSERT_EQ(entry->data[12], original_value);
    ASSERT_EQ(entry->state, CACHE_ENTRY_CLEAN);

    /* Cleanup */
    txn_destroy(txn);
    wal_destroy(wal);
    cache_destroy(cache);
    pager_close(pager);

    TEST_END();
    return 0;
}
