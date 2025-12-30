/*
 * txn.c - Transaction Manager implementation
 */

#include "txn/txn.h"
#include "txn/wal.h"
#include "os/mem.h"
#include "storage/pager.h"
#include "storage/cache.h"
#include "api/error.h"
#include <string.h>

/*
 * Create a new transaction context
 */
struct txn_context *txn_create(struct wal_context *wal, struct page_cache *cache)
{
    struct txn_context *txn;

    if (!wal || !cache) {
        return NULL;
    }

    /* Allocate transaction context */
    txn = (struct txn_context *)mem_alloc(sizeof(struct txn_context), AMIDB_MEM_CLEAR);
    if (!txn) {
        return NULL;
    }

    /* Initialize fields */
    txn->wal = wal;
    txn->cache = cache;
    txn->state = TXN_STATE_IDLE;
    txn->txn_id = 0;
    txn->dirty_count = 0;
    txn->pinned_count = 0;
    txn->pages_logged = 0;
    txn->commit_count = 0;
    txn->abort_count = 0;

    return txn;
}

/*
 * Destroy transaction context
 */
void txn_destroy(struct txn_context *txn)
{
    if (!txn) {
        return;
    }

    /* Ensure transaction is not active */
    if (txn->state == TXN_STATE_ACTIVE) {
        txn_abort(txn);
    }

    mem_free(txn, sizeof(struct txn_context));
}

/*
 * Begin a new transaction
 */
int txn_begin(struct txn_context *txn)
{
    int rc;

    if (!txn) {
        return AMIDB_ERROR;
    }

    /* Check state */
    if (txn->state != TXN_STATE_IDLE) {
        return AMIDB_BUSY;
    }

    /* Transition to ACTIVE state */
    txn->state = TXN_STATE_ACTIVE;
    txn->txn_id = ++txn->wal->current_txn_id;
    txn->dirty_count = 0;
    txn->pinned_count = 0;

    /* Write BEGIN record to WAL */
    rc = wal_write_record(txn->wal, WAL_BEGIN, NULL, 0);
    if (rc != AMIDB_OK) {
        txn->state = TXN_STATE_IDLE;
        return rc;
    }

    return AMIDB_OK;
}

/*
 * Commit the current transaction (with eager checkpoint)
 */
int txn_commit(struct txn_context *txn)
{
    uint32_t i;
    int rc;
    struct cache_entry *entry;
    struct {
        uint32_t page_num;
        uint8_t data[AMIDB_PAGE_SIZE];
    } payload;

    if (!txn) {
        return AMIDB_ERROR;
    }

    /* Check state */
    if (txn->state != TXN_STATE_ACTIVE) {
        return AMIDB_ERROR;
    }

    txn->state = TXN_STATE_COMMITTING;

    /* Step 1: Write all dirty pages to WAL */
    for (i = 0; i < txn->dirty_count; i++) {
        uint32_t page_num = txn->dirty_pages[i];

        /* Get page from cache */
        entry = cache_find_entry(txn->cache, page_num);
        if (entry && entry->state == CACHE_ENTRY_DIRTY) {
            /* Prepare payload */
            payload.page_num = page_num;
            memcpy(payload.data, entry->data, AMIDB_PAGE_SIZE);

            /* Write PAGE record to WAL */
            rc = wal_write_record(txn->wal, WAL_PAGE, &payload, sizeof(payload));
            if (rc != AMIDB_OK) {
                txn_abort(txn);
                return rc;
            }

            txn->pages_logged++;
        }
    }

    /* Step 2: Write COMMIT record */
    rc = wal_write_record(txn->wal, WAL_COMMIT, NULL, 0);
    if (rc != AMIDB_OK) {
        txn_abort(txn);
        return rc;
    }

    /* Step 3: Flush WAL to disk (DURABILITY POINT) */
    rc = wal_flush(txn->wal);
    if (rc != AMIDB_OK) {
        /* At this point, WAL may be partially written, but COMMIT was not flushed */
        /* On recovery, this transaction will be ignored (uncommitted) */
        txn->state = TXN_STATE_IDLE;
        return rc;
    }

    /* Transaction is now DURABLE */
    txn->state = TXN_STATE_COMMITTED;

    /* Step 4: EAGER CHECKPOINT - Write dirty pages to main DB */
    for (i = 0; i < txn->dirty_count; i++) {
        uint32_t page_num = txn->dirty_pages[i];

        entry = cache_find_entry(txn->cache, page_num);
        if (entry) {
            /* Write page to main database */
            rc = pager_write_page(txn->wal->pager, page_num, entry->data);
            if (rc != AMIDB_OK) {
                /* Checkpoint failed, but transaction is already durable in WAL */
                /* This is non-fatal - recovery will replay from WAL */
                continue;
            }

            /* Mark page as clean */
            entry->state = CACHE_ENTRY_CLEAN;
            entry->txn_id = 0;
        }
    }

    /* Sync main database */
    pager_sync(txn->wal->pager);

    /* Step 5: Reset WAL buffer (checkpoint complete) */
    wal_reset_buffer(txn->wal);

    /* Step 6: Unpin all pages */
    for (i = 0; i < txn->pinned_count; i++) {
        cache_unpin(txn->cache, txn->pinned_pages[i]);
    }

    /* Reset state */
    txn->dirty_count = 0;
    txn->pinned_count = 0;
    txn->state = TXN_STATE_IDLE;
    txn->commit_count++;

    return AMIDB_OK;
}

/*
 * Abort the current transaction
 */
int txn_abort(struct txn_context *txn)
{
    uint32_t i;
    struct cache_entry *entry;
    uint8_t temp_buf[AMIDB_PAGE_SIZE];
    int rc;

    if (!txn) {
        return AMIDB_ERROR;
    }

    txn->state = TXN_STATE_ABORTING;

    /* Reload dirty pages from disk (discard changes) */
    for (i = 0; i < txn->dirty_count; i++) {
        uint32_t page_num = txn->dirty_pages[i];

        entry = cache_find_entry(txn->cache, page_num);
        if (entry) {
            /* Read page from disk */
            rc = pager_read_page(txn->wal->pager, page_num, temp_buf);
            if (rc == AMIDB_OK) {
                /* Restore clean version */
                memcpy(entry->data, temp_buf, AMIDB_PAGE_SIZE);
                entry->state = CACHE_ENTRY_CLEAN;
            } else {
                /* Read failed - invalidate cache entry */
                entry->state = CACHE_ENTRY_INVALID;
            }

            entry->txn_id = 0;
        }
    }

    /* Unpin all pages */
    for (i = 0; i < txn->pinned_count; i++) {
        cache_unpin(txn->cache, txn->pinned_pages[i]);
    }

    /* Reset state and discard WAL buffer */
    txn->dirty_count = 0;
    txn->pinned_count = 0;
    txn->state = TXN_STATE_IDLE;
    txn->wal->buffer_used = txn->wal->txn_start_offset;
    txn->abort_count++;

    return AMIDB_OK;
}

/*
 * Add a page to the transaction's dirty page list
 */
int txn_add_dirty_page(struct txn_context *txn, uint32_t page_num)
{
    uint32_t i;

    if (!txn) {
        return AMIDB_ERROR;
    }

    /* Check if already in dirty list */
    for (i = 0; i < txn->dirty_count; i++) {
        if (txn->dirty_pages[i] == page_num) {
            return AMIDB_OK;  /* Already tracked */
        }
    }

    /* Add to dirty list */
    if (txn->dirty_count >= 64) {
        return AMIDB_FULL;  /* Too many dirty pages */
    }

    txn->dirty_pages[txn->dirty_count++] = page_num;

    /* Also add to pinned list if not present */
    for (i = 0; i < txn->pinned_count; i++) {
        if (txn->pinned_pages[i] == page_num) {
            return AMIDB_OK;  /* Already pinned */
        }
    }

    if (txn->pinned_count >= 64) {
        return AMIDB_FULL;
    }

    txn->pinned_pages[txn->pinned_count++] = page_num;

    return AMIDB_OK;
}

/*
 * Check if a page is dirty in this transaction
 */
int txn_is_page_dirty(struct txn_context *txn, uint32_t page_num)
{
    uint32_t i;

    if (!txn) {
        return 0;
    }

    for (i = 0; i < txn->dirty_count; i++) {
        if (txn->dirty_pages[i] == page_num) {
            return 1;
        }
    }

    return 0;
}
