/*
 * test_recovery.c - Unit tests for WAL crash recovery
 */

#include "test_harness.h"
#include "txn/wal.h"
#include "txn/txn.h"
#include "storage/pager.h"
#include "storage/cache.h"
#include "os/file.h"
#include "os/mem.h"
#include "api/error.h"
#include <string.h>
#include <stdio.h>

/* Use unique database names for each test */
#define TEST_DB_RECOVERY_COMMIT "RAM:recovery_commit.db"
#define TEST_DB_RECOVERY_UNCOMMIT "RAM:recovery_uncommit.db"
#define TEST_DB_RECOVERY_PARTIAL "RAM:recovery_partial.db"
#define TEST_DB_RECOVERY_MULTI "RAM:recovery_multi.db"
#define TEST_DB_RECOVERY_CORRUPT "RAM:recovery_corrupt.db"
#define TEST_DB_RECOVERY_EMPTY "RAM:recovery_empty.db"

/* Test: Recovery of committed transaction */
TEST(recovery_committed_transaction) {
    struct amidb_pager *pager = NULL;
    struct page_cache *cache;
    struct wal_context *wal;
    struct txn_context *txn;
    uint32_t page_num;
    uint8_t *data;
    struct cache_entry *entry;
    uint8_t test_pattern[100];
    int rc;
    uint32_t i;

    TEST_BEGIN();

    /* Phase 1: Create database and commit transaction */
    rc = pager_open(TEST_DB_RECOVERY_COMMIT, 0, &pager);
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

    /* Write test pattern */
    rc = cache_get_page(cache, page_num, &data);
    ASSERT_EQ(rc, 0);
    for (i = 0; i < 100; i++) {
        data[12 + i] = (uint8_t)(0xA0 + i);
        test_pattern[i] = data[12 + i];
    }

    cache_mark_dirty(cache, page_num);
    txn_add_dirty_page(txn, page_num);

    entry = cache_find_entry(cache, page_num);
    entry->txn_id = txn->txn_id;

    /* Commit transaction (eager checkpoint writes to main DB) */
    rc = txn_commit(txn);
    ASSERT_EQ(rc, AMIDB_OK);

    /* Close database cleanly */
    txn_destroy(txn);
    wal_destroy(wal);
    cache_destroy(cache);
    pager_close(pager);

    /* Phase 2: Reopen database and verify data persisted */
    rc = pager_open(TEST_DB_RECOVERY_COMMIT, 0, &pager);
    ASSERT_EQ(rc, 0);

    cache = cache_create(16, pager);
    ASSERT_NOT_NULL(cache);

    /* Read page and verify pattern */
    rc = cache_get_page(cache, page_num, &data);
    ASSERT_EQ(rc, 0);

    for (i = 0; i < 100; i++) {
        ASSERT_EQ(data[12 + i], test_pattern[i]);
    }

    cache_unpin(cache, page_num);
    cache_destroy(cache);
    pager_close(pager);

    TEST_END();
    return 0;
}

/* Test: Recovery ignores uncommitted transaction */
TEST(recovery_uncommitted_transaction) {
    struct amidb_pager *pager = NULL;
    struct page_cache *cache;
    struct wal_context *wal;
    struct txn_context *txn;
    uint32_t page_num;
    uint8_t *data;
    uint8_t original_value;
    int rc;

    TEST_BEGIN();

    /* Phase 1: Create database with initial value */
    rc = pager_open(TEST_DB_RECOVERY_UNCOMMIT, 0, &pager);
    ASSERT_EQ(rc, 0);

    cache = cache_create(16, pager);
    ASSERT_NOT_NULL(cache);

    /* Allocate page and write initial value */
    rc = pager_allocate_page(pager, &page_num);
    ASSERT_EQ(rc, 0);

    rc = cache_get_page(cache, page_num, &data);
    ASSERT_EQ(rc, 0);
    data[12] = 0x11;
    original_value = 0x11;

    cache_mark_dirty(cache, page_num);
    cache_unpin(cache, page_num);
    cache_flush(cache);
    pager_sync(pager);

    /* Create WAL and transaction */
    wal = wal_create(pager);
    ASSERT_NOT_NULL(wal);

    txn = txn_create(wal, cache);
    ASSERT_NOT_NULL(txn);

    /* Begin transaction and modify page */
    rc = txn_begin(txn);
    ASSERT_EQ(rc, AMIDB_OK);

    rc = cache_get_page(cache, page_num, &data);
    ASSERT_EQ(rc, 0);
    data[12] = 0x99;  /* This should NOT persist */

    /* Write to WAL buffer but DON'T commit */
    /* Just write BEGIN record, no COMMIT */

    /* Simulate crash: close without commit */
    /* Keep dirty flag set */
    pager->header.flags |= DB_FLAG_DIRTY;
    pager_write_header(pager);

    txn_destroy(txn);
    wal_destroy(wal);
    cache_destroy(cache);
    pager_close(pager);

    /* Phase 2: Reopen database (triggers recovery) */
    rc = pager_open(TEST_DB_RECOVERY_UNCOMMIT, 0, &pager);
    ASSERT_EQ(rc, 0);

    cache = cache_create(16, pager);
    ASSERT_NOT_NULL(cache);

    /* Verify original value (uncommitted change should be ignored) */
    rc = cache_get_page(cache, page_num, &data);
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(data[12], original_value);

    cache_unpin(cache, page_num);
    cache_destroy(cache);
    pager_close(pager);

    TEST_END();
    return 0;
}

/* Test: Recovery of partial commit (COMMIT not flushed) */
TEST(recovery_partial_commit) {
    struct amidb_pager *pager = NULL;
    struct page_cache *cache;
    struct wal_context *wal;
    uint32_t page_num;
    uint8_t *data;
    uint8_t original_value;
    int rc;

    TEST_BEGIN();

    /* Phase 1: Create database with initial value */
    rc = pager_open(TEST_DB_RECOVERY_PARTIAL, 0, &pager);
    ASSERT_EQ(rc, 0);

    cache = cache_create(16, pager);
    ASSERT_NOT_NULL(cache);

    /* Allocate page and write initial value */
    rc = pager_allocate_page(pager, &page_num);
    ASSERT_EQ(rc, 0);

    rc = cache_get_page(cache, page_num, &data);
    ASSERT_EQ(rc, 0);
    data[12] = 0x22;
    original_value = 0x22;

    cache_mark_dirty(cache, page_num);
    cache_unpin(cache, page_num);
    cache_flush(cache);
    pager_sync(pager);

    /* Create WAL */
    wal = wal_create(pager);
    ASSERT_NOT_NULL(wal);

    /* Write BEGIN and PAGE to WAL buffer, but not COMMIT */
    wal->current_txn_id = 1;
    rc = wal_write_record(wal, WAL_BEGIN, NULL, 0);
    ASSERT_EQ(rc, AMIDB_OK);

    /* Write PAGE record (partial commit) */
    struct {
        uint32_t page_num;
        uint8_t data[AMIDB_PAGE_SIZE];
    } payload;
    payload.page_num = page_num;
    memset(payload.data, 0, AMIDB_PAGE_SIZE);
    payload.data[12] = 0xCC;  /* Different from original */

    rc = wal_write_record(wal, WAL_PAGE, &payload, sizeof(payload));
    ASSERT_EQ(rc, AMIDB_OK);

    /* NO COMMIT RECORD - simulate crash before commit */
    /* Flush WAL buffer to disk (partial state) */
    rc = wal_flush(wal);
    ASSERT_EQ(rc, AMIDB_OK);

    /* Mark dirty and close */
    pager->header.flags |= DB_FLAG_DIRTY;
    pager->header.wal_head = wal->wal_head;
    pager_write_header(pager);

    wal_destroy(wal);
    cache_destroy(cache);
    pager_close(pager);

    /* Phase 2: Reopen database (triggers recovery) */
    rc = pager_open(TEST_DB_RECOVERY_PARTIAL, 0, &pager);
    ASSERT_EQ(rc, 0);

    cache = cache_create(16, pager);
    ASSERT_NOT_NULL(cache);

    /* Verify original value (uncommitted txn should be ignored) */
    rc = cache_get_page(cache, page_num, &data);
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(data[12], original_value);

    cache_unpin(cache, page_num);
    cache_destroy(cache);
    pager_close(pager);

    TEST_END();
    return 0;
}

/* Test: Recovery of multiple transactions */
TEST(recovery_multiple_transactions) {
    struct amidb_pager *pager = NULL;
    struct page_cache *cache;
    struct wal_context *wal;
    struct txn_context *txn;
    uint32_t page1, page2;
    uint8_t *data;
    struct cache_entry *entry;
    int rc;

    TEST_BEGIN();

    /* Phase 1: Create database and commit two transactions */
    rc = pager_open(TEST_DB_RECOVERY_MULTI, 0, &pager);
    ASSERT_EQ(rc, 0);

    cache = cache_create(16, pager);
    ASSERT_NOT_NULL(cache);

    /* Allocate two pages */
    rc = pager_allocate_page(pager, &page1);
    ASSERT_EQ(rc, 0);
    rc = pager_allocate_page(pager, &page2);
    ASSERT_EQ(rc, 0);

    /* Create WAL and transaction */
    wal = wal_create(pager);
    ASSERT_NOT_NULL(wal);

    txn = txn_create(wal, cache);
    ASSERT_NOT_NULL(txn);

    /* Transaction 1: Modify page1 */
    rc = txn_begin(txn);
    ASSERT_EQ(rc, AMIDB_OK);

    rc = cache_get_page(cache, page1, &data);
    ASSERT_EQ(rc, 0);
    data[12] = 0xAA;

    cache_mark_dirty(cache, page1);
    txn_add_dirty_page(txn, page1);
    entry = cache_find_entry(cache, page1);
    entry->txn_id = txn->txn_id;

    rc = txn_commit(txn);
    ASSERT_EQ(rc, AMIDB_OK);

    /* Transaction 2: Modify page2 */
    rc = txn_begin(txn);
    ASSERT_EQ(rc, AMIDB_OK);

    rc = cache_get_page(cache, page2, &data);
    ASSERT_EQ(rc, 0);
    data[12] = 0xBB;

    cache_mark_dirty(cache, page2);
    txn_add_dirty_page(txn, page2);
    entry = cache_find_entry(cache, page2);
    entry->txn_id = txn->txn_id;

    rc = txn_commit(txn);
    ASSERT_EQ(rc, AMIDB_OK);

    /* Close cleanly */
    txn_destroy(txn);
    wal_destroy(wal);
    cache_destroy(cache);
    pager_close(pager);

    /* Phase 2: Reopen and verify both transactions persisted */
    rc = pager_open(TEST_DB_RECOVERY_MULTI, 0, &pager);
    ASSERT_EQ(rc, 0);

    cache = cache_create(16, pager);
    ASSERT_NOT_NULL(cache);

    rc = cache_get_page(cache, page1, &data);
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(data[12], 0xAA);
    cache_unpin(cache, page1);

    rc = cache_get_page(cache, page2, &data);
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(data[12], 0xBB);
    cache_unpin(cache, page2);

    cache_destroy(cache);
    pager_close(pager);

    TEST_END();
    return 0;
}

/* Test: Recovery stops at corrupt WAL record */
TEST(recovery_corrupt_wal_record) {
    struct amidb_pager *pager = NULL;
    struct page_cache *cache;
    struct wal_context *wal;
    uint32_t page_num;
    uint8_t *data;
    struct wal_record_header *hdr;
    void *file_handle;
    int rc;

    TEST_BEGIN();

    /* Phase 1: Create database and write valid WAL record */
    rc = pager_open(TEST_DB_RECOVERY_CORRUPT, 0, &pager);
    ASSERT_EQ(rc, 0);

    cache = cache_create(16, pager);
    ASSERT_NOT_NULL(cache);

    rc = pager_allocate_page(pager, &page_num);
    ASSERT_EQ(rc, 0);

    /* Create WAL and write BEGIN */
    wal = wal_create(pager);
    ASSERT_NOT_NULL(wal);

    wal->current_txn_id = 1;
    rc = wal_write_record(wal, WAL_BEGIN, NULL, 0);
    ASSERT_EQ(rc, AMIDB_OK);

    /* Flush to disk */
    rc = wal_flush(wal);
    ASSERT_EQ(rc, AMIDB_OK);

    /* Corrupt the WAL record on disk */
    file_handle = file_open(TEST_DB_RECOVERY_CORRUPT, AMIDB_O_RDWR);
    ASSERT_NOT_NULL(file_handle);

    file_seek(file_handle, WAL_REGION_START, AMIDB_SEEK_SET);

    /* Read record header */
    struct wal_record_header corrupt_hdr;
    file_read(file_handle, &corrupt_hdr, sizeof(corrupt_hdr));

    /* Corrupt the checksum */
    corrupt_hdr.checksum = 0xDEADBEEF;

    /* Write back corrupted header */
    file_seek(file_handle, WAL_REGION_START, AMIDB_SEEK_SET);
    file_write(file_handle, &corrupt_hdr, sizeof(corrupt_hdr));
    file_sync(file_handle);
    file_close(file_handle);

    /* Mark dirty */
    pager->header.flags |= DB_FLAG_DIRTY;
    pager->header.wal_head = wal->wal_head;
    pager_write_header(pager);

    wal_destroy(wal);
    cache_destroy(cache);
    pager_close(pager);

    /* Phase 2: Reopen database (recovery should handle corrupt record) */
    rc = pager_open(TEST_DB_RECOVERY_CORRUPT, 0, &pager);
    ASSERT_EQ(rc, 0);  /* Should still open successfully */

    /* Recovery should have stopped at corrupt record and cleared dirty flag */
    ASSERT_EQ(pager->header.flags & DB_FLAG_DIRTY, 0);

    pager_close(pager);

    TEST_END();
    return 0;
}

/* Test: Recovery with empty WAL */
TEST(recovery_empty_wal) {
    struct amidb_pager *pager = NULL;
    int rc;

    TEST_BEGIN();

    /* Phase 1: Create database and set dirty flag without WAL data */
    rc = pager_open(TEST_DB_RECOVERY_EMPTY, 0, &pager);
    ASSERT_EQ(rc, 0);

    /* Mark dirty with no WAL data */
    pager->header.flags |= DB_FLAG_DIRTY;
    pager->header.wal_head = 0;
    pager->header.wal_tail = 0;
    pager_write_header(pager);
    pager_sync(pager);

    pager_close(pager);

    /* Phase 2: Reopen database (recovery should handle empty WAL) */
    rc = pager_open(TEST_DB_RECOVERY_EMPTY, 0, &pager);
    ASSERT_EQ(rc, 0);

    /* Should successfully recover (no-op) and clear dirty flag */
    ASSERT_EQ(pager->header.flags & DB_FLAG_DIRTY, 0);

    pager_close(pager);

    TEST_END();
    return 0;
}
