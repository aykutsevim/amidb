/*
 * txn.h - Transaction Manager for AmiDB
 *
 * Provides ACID transaction support with BEGIN/COMMIT/ROLLBACK semantics.
 * Uses Write-Ahead Logging for durability and crash recovery.
 */

#ifndef AMIDB_TXN_H
#define AMIDB_TXN_H

#include <stdint.h>
#include "txn/wal.h"
#include "storage/cache.h"

/*
 * Transaction States
 */
typedef enum {
    TXN_STATE_IDLE = 0,       /* No active transaction */
    TXN_STATE_ACTIVE,         /* Transaction in progress */
    TXN_STATE_COMMITTING,     /* Writing to WAL, not yet synced */
    TXN_STATE_ABORTING,       /* Rolling back changes */
    TXN_STATE_COMMITTED       /* Commit complete (transient state) */
} txn_state_t;

/*
 * Transaction Context
 *
 * Tracks all state for an active transaction, including dirty pages,
 * pinned pages, and transaction ID.
 */
struct txn_context {
    struct wal_context *wal;        /* Associated WAL */
    struct page_cache *cache;       /* For pinning dirty pages */

    /* Transaction state */
    txn_state_t state;
    uint64_t txn_id;                /* Current transaction ID */

    /* Dirty page tracking */
    uint32_t dirty_pages[64];       /* Max 64 pages modified per txn */
    uint32_t dirty_count;           /* Number of dirty pages */

    /* Pin tracking (to unpin on commit/abort) */
    uint32_t pinned_pages[64];
    uint32_t pinned_count;

    /* Statistics */
    uint32_t pages_logged;
    uint32_t commit_count;
    uint32_t abort_count;
};

/*
 * Transaction API Functions
 */

/*
 * Create a new transaction context
 *
 * Parameters:
 *   wal   - WAL context to use for logging
 *   cache - Page cache for dirty page management
 *
 * Returns: Transaction context on success, NULL on failure
 */
struct txn_context *txn_create(struct wal_context *wal, struct page_cache *cache);

/*
 * Destroy transaction context
 */
void txn_destroy(struct txn_context *txn);

/*
 * Begin a new transaction
 *
 * Writes a WAL_BEGIN record and transitions to TXN_STATE_ACTIVE.
 *
 * Returns: 0 on success, AMIDB_BUSY if transaction already active
 */
int txn_begin(struct txn_context *txn);

/*
 * Commit the current transaction (with eager checkpoint)
 *
 * Algorithm:
 *   1. Write all dirty pages to WAL
 *   2. Write WAL_COMMIT record
 *   3. Flush WAL to disk (DURABILITY POINT)
 *   4. EAGER CHECKPOINT: Write dirty pages to main DB
 *   5. Reset WAL buffer
 *   6. Unpin all pages
 *
 * Returns: 0 on success, error code on failure
 */
int txn_commit(struct txn_context *txn);

/*
 * Abort the current transaction
 *
 * Discards all changes by reloading dirty pages from disk.
 * Unpins all pages.
 *
 * Returns: 0 on success, error code on failure
 */
int txn_abort(struct txn_context *txn);

/*
 * Add a page to the transaction's dirty page list
 *
 * Also adds to pinned_pages list if not already present.
 *
 * Parameters:
 *   txn      - Transaction context
 *   page_num - Page number to track
 *
 * Returns: 0 on success, AMIDB_FULL if too many dirty pages
 */
int txn_add_dirty_page(struct txn_context *txn, uint32_t page_num);

/*
 * Check if a page is dirty in this transaction
 *
 * Returns: 1 if dirty, 0 if not
 */
int txn_is_page_dirty(struct txn_context *txn, uint32_t page_num);

#endif /* AMIDB_TXN_H */
