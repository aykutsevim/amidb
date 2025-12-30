/*
 * test_wal.c - Unit tests for Write-Ahead Logging
 */

#include "test_harness.h"
#include "txn/wal.h"
#include "storage/pager.h"
#include "os/file.h"
#include "os/mem.h"
#include "api/error.h"
#include <string.h>
#include <stdio.h>

/* Use unique database names for each test */
#define TEST_DB_WAL_CREATE "RAM:wal_create.db"
#define TEST_DB_WAL_WRITE "RAM:wal_write.db"
#define TEST_DB_WAL_PAGE "RAM:wal_page.db"
#define TEST_DB_WAL_FLUSH "RAM:wal_flush.db"
#define TEST_DB_WAL_OVERFLOW "RAM:wal_overflow.db"
#define TEST_DB_WAL_CHECKSUM "RAM:wal_checksum.db"

/* Test: Create and destroy WAL context */
TEST(wal_create_destroy) {
    struct amidb_pager *pager = NULL;
    struct wal_context *wal;
    int rc;

    TEST_BEGIN();

    /* Create pager */
    rc = pager_open(TEST_DB_WAL_CREATE, 0, &pager);
    ASSERT_EQ(rc, 0);
    ASSERT_NOT_NULL(pager);

    /* Create WAL context */
    wal = wal_create(pager);
    ASSERT_NOT_NULL(wal);
    ASSERT_EQ(wal->pager, pager);
    ASSERT_EQ(wal->buffer_used, 0);
    ASSERT_EQ(wal->current_txn_id, 0);
    ASSERT_EQ(wal->wal_head, 0);
    ASSERT_EQ(wal->wal_tail, 0);

    /* Destroy WAL */
    wal_destroy(wal);

    pager_close(pager);

    TEST_END();
    return 0;
}

/* Test: Write BEGIN and COMMIT records */
TEST(wal_write_begin_commit) {
    struct amidb_pager *pager = NULL;
    struct wal_context *wal;
    int rc;

    TEST_BEGIN();

    /* Create pager and WAL */
    rc = pager_open(TEST_DB_WAL_WRITE, 0, &pager);
    ASSERT_EQ(rc, 0);

    wal = wal_create(pager);
    ASSERT_NOT_NULL(wal);

    /* Write BEGIN record */
    wal->current_txn_id = 1;
    rc = wal_write_record(wal, WAL_BEGIN, NULL, 0);
    ASSERT_EQ(rc, AMIDB_OK);
    ASSERT_GT(wal->buffer_used, 0);

    /* Write COMMIT record */
    rc = wal_write_record(wal, WAL_COMMIT, NULL, 0);
    ASSERT_EQ(rc, AMIDB_OK);

    /* Verify buffer has grown */
    ASSERT_GT(wal->buffer_used, sizeof(struct wal_record_header));

    /* Cleanup */
    wal_destroy(wal);
    pager_close(pager);

    TEST_END();
    return 0;
}

/* Test: Write PAGE record with full page data */
TEST(wal_write_page_record) {
    struct amidb_pager *pager = NULL;
    struct wal_context *wal;
    struct {
        uint32_t page_num;
        uint8_t data[AMIDB_PAGE_SIZE];
    } payload;
    uint32_t initial_used;
    int rc;
    uint32_t i;

    TEST_BEGIN();

    /* Create pager and WAL */
    rc = pager_open(TEST_DB_WAL_PAGE, 0, &pager);
    ASSERT_EQ(rc, 0);

    wal = wal_create(pager);
    ASSERT_NOT_NULL(wal);

    /* Prepare test payload */
    payload.page_num = 10;
    for (i = 0; i < AMIDB_PAGE_SIZE; i++) {
        payload.data[i] = (uint8_t)(i & 0xFF);
    }

    /* Write PAGE record */
    wal->current_txn_id = 1;
    initial_used = wal->buffer_used;
    rc = wal_write_record(wal, WAL_PAGE, &payload, sizeof(payload));
    ASSERT_EQ(rc, AMIDB_OK);

    /* Verify buffer grew by header + payload size */
    ASSERT_EQ(wal->buffer_used, initial_used + sizeof(struct wal_record_header) + sizeof(payload));

    /* Cleanup */
    wal_destroy(wal);
    pager_close(pager);

    TEST_END();
    return 0;
}

/* Test: Flush WAL to disk */
TEST(wal_flush_to_disk) {
    struct amidb_pager *pager = NULL;
    struct wal_context *wal;
    uint32_t buffer_used_before;
    int rc;

    TEST_BEGIN();

    /* Create pager and WAL */
    rc = pager_open(TEST_DB_WAL_FLUSH, 0, &pager);
    ASSERT_EQ(rc, 0);

    wal = wal_create(pager);
    ASSERT_NOT_NULL(wal);

    /* Write some records */
    wal->current_txn_id = 1;
    rc = wal_write_record(wal, WAL_BEGIN, NULL, 0);
    ASSERT_EQ(rc, AMIDB_OK);

    rc = wal_write_record(wal, WAL_COMMIT, NULL, 0);
    ASSERT_EQ(rc, AMIDB_OK);

    buffer_used_before = wal->buffer_used;
    ASSERT_GT(buffer_used_before, 0);

    /* Flush to disk */
    rc = wal_flush(wal);
    ASSERT_EQ(rc, AMIDB_OK);

    /* Verify wal_head has advanced */
    ASSERT_EQ(wal->wal_head, buffer_used_before);

    /* Cleanup */
    wal_destroy(wal);
    pager_close(pager);

    TEST_END();
    return 0;
}

/* Test: WAL buffer overflow detection */
TEST(wal_buffer_overflow) {
    struct amidb_pager *pager = NULL;
    struct wal_context *wal;
    struct {
        uint32_t page_num;
        uint8_t data[AMIDB_PAGE_SIZE];
    } payload;
    int rc;
    int i;

    TEST_BEGIN();

    /* Create pager and WAL */
    rc = pager_open(TEST_DB_WAL_OVERFLOW, 0, &pager);
    ASSERT_EQ(rc, 0);

    wal = wal_create(pager);
    ASSERT_NOT_NULL(wal);

    /* Try to write many PAGE records to overflow buffer */
    wal->current_txn_id = 1;
    payload.page_num = 1;
    memset(payload.data, 0xAB, AMIDB_PAGE_SIZE);

    /* WAL_BUFFER_SIZE is 32KB, each PAGE record is ~4124 bytes */
    /* So we can fit about 7-8 records before overflow */
    for (i = 0; i < 10; i++) {
        rc = wal_write_record(wal, WAL_PAGE, &payload, sizeof(payload));
        if (rc == AMIDB_FULL) {
            /* Expected - buffer is full */
            break;
        }
        ASSERT_EQ(rc, AMIDB_OK);
    }

    /* Should have hit AMIDB_FULL before writing all 10 records */
    ASSERT_LT(i, 10);

    /* Cleanup */
    wal_destroy(wal);
    pager_close(pager);

    TEST_END();
    return 0;
}

/* Test: WAL checksum validation */
TEST(wal_checksum_validation) {
    struct amidb_pager *pager = NULL;
    struct wal_context *wal;
    struct wal_record_header *hdr;
    uint32_t original_checksum;
    int valid;
    int rc;

    TEST_BEGIN();

    /* Create pager and WAL */
    rc = pager_open(TEST_DB_WAL_CHECKSUM, 0, &pager);
    ASSERT_EQ(rc, 0);

    wal = wal_create(pager);
    ASSERT_NOT_NULL(wal);

    /* Write a BEGIN record */
    wal->current_txn_id = 1;
    rc = wal_write_record(wal, WAL_BEGIN, NULL, 0);
    ASSERT_EQ(rc, AMIDB_OK);

    /* Verify checksum is valid */
    hdr = (struct wal_record_header *)wal->buffer;
    valid = wal_verify_checksum(wal->buffer, hdr->record_size);
    ASSERT_EQ(valid, 1);

    /* Corrupt the record and verify checksum fails */
    original_checksum = hdr->checksum;
    hdr->checksum = 0xDEADBEEF;
    valid = wal_verify_checksum(wal->buffer, hdr->record_size);
    ASSERT_EQ(valid, 0);

    /* Restore original checksum */
    hdr->checksum = original_checksum;
    valid = wal_verify_checksum(wal->buffer, hdr->record_size);
    ASSERT_EQ(valid, 1);

    /* Cleanup */
    wal_destroy(wal);
    pager_close(pager);

    TEST_END();
    return 0;
}
