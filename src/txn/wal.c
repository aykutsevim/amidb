/*
 * wal.c - Write-Ahead Logging implementation
 */

#include "txn/wal.h"
#include "os/mem.h"
#include "os/file.h"
#include "util/crc32.h"
#include "api/error.h"
#include <string.h>
#include <stddef.h>  /* For offsetof */

/*
 * Create a new WAL context
 */
struct wal_context *wal_create(struct amidb_pager *pager)
{
    struct wal_context *wal;

    if (!pager) {
        return NULL;
    }

    /* Allocate WAL context */
    wal = (struct wal_context *)mem_alloc(sizeof(struct wal_context), AMIDB_MEM_CLEAR);
    if (!wal) {
        return NULL;
    }

    /* Initialize fields */
    wal->pager = pager;
    wal->buffer_used = 0;
    wal->current_txn_id = 0;
    wal->txn_start_offset = 0;
    wal->wal_head = 0;
    wal->wal_tail = 0;
    wal->checkpoint_count = 0;
    wal->total_records = 0;

    return wal;
}

/*
 * Destroy WAL context
 */
void wal_destroy(struct wal_context *wal)
{
    if (!wal) {
        return;
    }

    mem_free(wal, sizeof(struct wal_context));
}

/*
 * Write a record to the WAL buffer
 */
int wal_write_record(struct wal_context *wal, uint16_t type,
                     const void *payload, uint32_t payload_size)
{
    struct wal_record_header hdr;
    uint32_t record_size;
    uint8_t *write_pos;
    uint32_t crc;

    if (!wal) {
        return AMIDB_ERROR;
    }

    record_size = sizeof(hdr) + payload_size;

    /* Check buffer space */
    if (wal->buffer_used + record_size > WAL_BUFFER_SIZE) {
        return AMIDB_FULL;  /* Checkpoint should have prevented this */
    }

    /* Build header (checksum will be computed last) */
    hdr.magic = 0x57414C52;  /* "WALR" */
    hdr.record_type = type;
    hdr.flags = 0;
    hdr.record_size = record_size;
    hdr.txn_id = wal->current_txn_id;
    hdr.checksum = 0;  /* Will be computed below */

    /* Compute checksum (header + payload, excluding checksum field) */
    crc32_init();
    crc = 0;
    /* Hash header fields before checksum */
    crc = crc32_update(crc, (const uint8_t*)&hdr, offsetof(struct wal_record_header, checksum));
    /* Hash payload if present */
    if (payload_size > 0 && payload) {
        crc = crc32_update(crc, (const uint8_t*)payload, payload_size);
    }
    hdr.checksum = crc;

    /* Append to buffer */
    write_pos = wal->buffer + wal->buffer_used;

    /* Copy header */
    memcpy(write_pos, &hdr, sizeof(hdr));

    /* Copy payload if present */
    if (payload_size > 0 && payload) {
        memcpy(write_pos + sizeof(hdr), payload, payload_size);
    }

    wal->buffer_used += record_size;
    wal->total_records++;

    return AMIDB_OK;
}

/*
 * Flush WAL buffer to disk
 */
int wal_flush(struct wal_context *wal)
{
    int32_t wal_file_offset;
    int32_t bytes_written;
    int rc;

    if (!wal) {
        return AMIDB_ERROR;
    }

    /* Nothing to flush */
    if (wal->buffer_used == 0) {
        return AMIDB_OK;
    }

    /* Calculate disk offset in WAL region */
    wal_file_offset = WAL_REGION_START + wal->wal_head;

    /* Check WAL region capacity */
    if (wal->wal_head + wal->buffer_used > WAL_REGION_SIZE) {
        return AMIDB_FULL;  /* Must checkpoint first */
    }

    /* Seek to WAL region */
    rc = file_seek(wal->pager->file_handle, wal_file_offset, AMIDB_SEEK_SET);
    if (rc != 0) {
        return AMIDB_IOERR;
    }

    /* Write buffer to disk */
    bytes_written = file_write(wal->pager->file_handle, wal->buffer, wal->buffer_used);
    if (bytes_written != (int32_t)wal->buffer_used) {
        return AMIDB_IOERR;
    }

    /* CRITICAL: Fsync for durability */
    rc = file_sync(wal->pager->file_handle);
    if (rc != 0) {
        return AMIDB_IOERR;
    }

    /* Update WAL head */
    wal->wal_head += wal->buffer_used;

    return AMIDB_OK;
}

/*
 * Verify WAL record checksum
 */
int wal_verify_checksum(const uint8_t *record_data, uint32_t record_size)
{
    struct wal_record_header hdr;
    uint32_t stored_checksum;
    uint32_t computed_checksum;
    const uint8_t *payload;
    uint32_t payload_size;

    if (!record_data || record_size < sizeof(struct wal_record_header)) {
        return 0;
    }

    /* Copy header */
    memcpy(&hdr, record_data, sizeof(hdr));

    stored_checksum = hdr.checksum;
    payload = record_data + sizeof(hdr);
    payload_size = hdr.record_size - sizeof(hdr);

    /* Compute checksum (same method as writing) */
    crc32_init();
    computed_checksum = 0;
    /* Hash header up to checksum field */
    computed_checksum = crc32_update(computed_checksum, record_data, offsetof(struct wal_record_header, checksum));
    /* Hash payload if present */
    if (payload_size > 0) {
        computed_checksum = crc32_update(computed_checksum, payload, payload_size);
    }

    return (computed_checksum == stored_checksum) ? 1 : 0;
}

/*
 * Crash Recovery: Replay committed transactions from WAL
 */
int wal_recover(struct wal_context *wal)
{
    uint8_t *wal_buffer;
    uint64_t committed_txns[WAL_MAX_RECORDS];
    uint32_t num_committed;
    uint32_t offset;
    struct wal_record_header hdr;
    int32_t bytes_read;
    int rc;

    if (!wal) {
        return AMIDB_ERROR;
    }

    /* Allocate temporary buffer for entire WAL region (128KB) */
    wal_buffer = (uint8_t *)mem_alloc(WAL_REGION_SIZE, 0);
    if (!wal_buffer) {
        return AMIDB_NOMEM;
    }

    /* Read entire WAL region from disk */
    rc = file_seek(wal->pager->file_handle, WAL_REGION_START, AMIDB_SEEK_SET);
    if (rc != 0) {
        mem_free(wal_buffer, WAL_REGION_SIZE);
        return AMIDB_IOERR;
    }

    bytes_read = file_read(wal->pager->file_handle, wal_buffer, WAL_REGION_SIZE);
    if (bytes_read < 0) {
        mem_free(wal_buffer, WAL_REGION_SIZE);
        return AMIDB_IOERR;
    }

    /* PASS 1: Find all committed transaction IDs */
    num_committed = 0;
    offset = 0;

    while (offset < wal->wal_head && offset + sizeof(hdr) <= WAL_REGION_SIZE) {
        /* Copy header */
        memcpy(&hdr, wal_buffer + offset, sizeof(hdr));

        /* Validate magic */
        if (hdr.magic != 0x57414C52) {
            break;  /* Stop at corruption */
        }

        /* Verify checksum */
        if (!wal_verify_checksum(wal_buffer + offset, hdr.record_size)) {
            break;  /* Stop at checksum failure */
        }

        /* Track COMMIT records */
        if (hdr.record_type == WAL_COMMIT) {
            if (num_committed < WAL_MAX_RECORDS) {
                committed_txns[num_committed++] = hdr.txn_id;
            }
        }

        offset += hdr.record_size;
    }

    /* PASS 2: Replay PAGE records for committed transactions only */
    offset = 0;

    while (offset < wal->wal_head && offset + sizeof(hdr) <= WAL_REGION_SIZE) {
        /* Copy header */
        memcpy(&hdr, wal_buffer + offset, sizeof(hdr));

        /* Validate magic */
        if (hdr.magic != 0x57414C52) {
            break;
        }

        /* Check if this transaction committed */
        int is_committed = 0;
        uint32_t i;
        for (i = 0; i < num_committed; i++) {
            if (committed_txns[i] == hdr.txn_id) {
                is_committed = 1;
                break;
            }
        }

        /* Replay PAGE records for committed transactions */
        if (is_committed && hdr.record_type == WAL_PAGE) {
            struct wal_page_record *page_rec;
            page_rec = (struct wal_page_record *)(wal_buffer + offset);

            /* Write page to main database (bypass transaction) */
            rc = pager_write_page(wal->pager, page_rec->page_num, page_rec->page_data);
            if (rc != 0) {
                mem_free(wal_buffer, WAL_REGION_SIZE);
                return rc;
            }
        }

        offset += hdr.record_size;
    }

    /* Sync main database */
    rc = pager_sync(wal->pager);
    if (rc != 0) {
        mem_free(wal_buffer, WAL_REGION_SIZE);
        return rc;
    }

    /* Clear WAL region */
    wal->wal_head = 0;
    wal->wal_tail = 0;
    wal->buffer_used = 0;

    /* Free temporary buffer */
    mem_free(wal_buffer, WAL_REGION_SIZE);

    return AMIDB_OK;
}

/*
 * Reset WAL buffer (called after checkpoint)
 */
void wal_reset_buffer(struct wal_context *wal)
{
    if (!wal) {
        return;
    }

    wal->buffer_used = 0;
    wal->wal_head = 0;
    wal->wal_tail = 0;
}
