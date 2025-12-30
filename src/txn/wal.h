/*
 * wal.h - Write-Ahead Logging (WAL) for AmiDB
 *
 * Implements write-ahead logging for crash recovery and ACID transactions.
 * WAL region is stored at pages 3-34 (128KB) in the database file.
 *
 * Design: Eager checkpoint (checkpoint after every commit)
 */

#ifndef AMIDB_WAL_H
#define AMIDB_WAL_H

#include <stdint.h>
#include "storage/pager.h"

/*
 * WAL Configuration
 */
#define WAL_BUFFER_SIZE  32768        /* 32 KB in-memory buffer */
#define WAL_REGION_START 0x3000       /* Page 3 offset (12KB) */
#define WAL_REGION_SIZE  (32 * AMIDB_PAGE_SIZE)  /* 128 KB on disk (pages 3-34) */
#define WAL_MAX_RECORDS  256          /* Maximum records to track in recovery */

/*
 * WAL Record Types
 */
#define WAL_BEGIN      0x0001  /* Transaction start */
#define WAL_COMMIT     0x0002  /* Transaction commit */
#define WAL_ABORT      0x0003  /* Transaction abort */
#define WAL_PAGE       0x0010  /* Full page image */
#define WAL_CHECKPOINT 0x0020  /* Checkpoint marker */

/*
 * WAL Record Header (24 bytes)
 *
 * Every WAL record starts with this header.
 * The checksum covers the entire record (header + payload).
 */
struct wal_record_header {
    uint32_t magic;          /* 0x57414C52 ("WALR" in ASCII) */
    uint16_t record_type;    /* WAL_* type */
    uint16_t flags;          /* Reserved for future use */
    uint32_t record_size;    /* Total size including header */
    uint64_t txn_id;         /* Transaction ID */
    uint32_t checksum;       /* CRC32 of entire record */
};

/*
 * WAL Page Record (24 + 4 + 4096 = 4124 bytes)
 *
 * Stores a full page image for recovery.
 */
struct wal_page_record {
    struct wal_record_header header;
    uint32_t page_num;       /* Which page this is */
    uint8_t  page_data[AMIDB_PAGE_SIZE];  /* Full 4KB page */
};

/*
 * WAL Context
 *
 * Manages the in-memory WAL buffer and on-disk WAL region.
 */
struct wal_context {
    struct amidb_pager *pager;       /* For WAL region I/O */

    /* In-memory buffer */
    uint8_t buffer[WAL_BUFFER_SIZE]; /* 32KB buffer */
    uint32_t buffer_used;            /* Bytes used in buffer */

    /* Current transaction tracking */
    uint64_t current_txn_id;         /* Incrementing counter */
    uint32_t txn_start_offset;       /* Where current txn starts in buffer */

    /* WAL region tracking (on disk) */
    uint32_t wal_head;               /* Next write position in WAL region */
    uint32_t wal_tail;               /* Oldest unprocessed entry */

    /* Statistics */
    uint32_t checkpoint_count;
    uint32_t total_records;
};

/*
 * WAL API Functions
 */

/*
 * Create a new WAL context
 *
 * Returns: WAL context on success, NULL on failure
 */
struct wal_context *wal_create(struct amidb_pager *pager);

/*
 * Destroy WAL context and free resources
 */
void wal_destroy(struct wal_context *wal);

/*
 * Write a record to the WAL buffer
 *
 * Parameters:
 *   wal          - WAL context
 *   type         - Record type (WAL_BEGIN, WAL_COMMIT, WAL_PAGE, etc.)
 *   payload      - Record payload (NULL for BEGIN/COMMIT/ABORT)
 *   payload_size - Size of payload (0 for BEGIN/COMMIT/ABORT)
 *
 * Returns: 0 on success, error code on failure
 */
int wal_write_record(struct wal_context *wal, uint16_t type,
                     const void *payload, uint32_t payload_size);

/*
 * Flush WAL buffer to disk
 *
 * Writes the in-memory buffer to the WAL region and calls file_sync()
 * for durability. This is the critical durability point for transactions.
 *
 * Returns: 0 on success, error code on failure
 */
int wal_flush(struct wal_context *wal);

/*
 * Verify WAL record checksum
 *
 * Parameters:
 *   record_data - Pointer to start of WAL record
 *   record_size - Total size of record
 *
 * Returns: 1 if checksum valid, 0 if invalid
 */
int wal_verify_checksum(const uint8_t *record_data, uint32_t record_size);

/*
 * Crash Recovery: Replay committed transactions from WAL
 *
 * Scans the WAL region, identifies committed transactions, and replays
 * their page writes to the main database. Uncommitted transactions are
 * discarded.
 *
 * Algorithm:
 *   1. Read entire WAL region (128KB)
 *   2. PASS 1: Find all COMMIT records → build committed_txns[] array
 *   3. PASS 2: Replay PAGE records for committed transactions only
 *   4. Sync main database
 *   5. Clear WAL region
 *
 * Returns: 0 on success, error code on failure
 */
int wal_recover(struct wal_context *wal);

/*
 * Reset WAL buffer (called after checkpoint)
 */
void wal_reset_buffer(struct wal_context *wal);

#endif /* AMIDB_WAL_H */
