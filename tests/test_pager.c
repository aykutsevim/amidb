/*
 * test_pager.c - Unit tests for pager
 */

#include "test_harness.h"
#include "storage/pager.h"
#include "os/file.h"
#include "os/mem.h"
#include <string.h>
#include <stdio.h>

/* Use unique database names for each test to avoid conflicts */
#define TEST_DB_CREATE_NEW "RAM:test_create_new.db"
#define TEST_DB_ALLOCATE "RAM:test_allocate.db"
#define TEST_DB_WRITE_READ "RAM:test_write_read.db"
#define TEST_DB_CHECKSUM "RAM:test_checksum.db"
#define TEST_DB_REOPEN "RAM:test_reopen.db"

/* Test: Memory allocation */
TEST(pager_mem_test) {
    void *ptr1, *ptr2, *ptr3, *ptr4;

    TEST_BEGIN();

    /* Clean up test files */
    file_delete("RAM:test_simple.dat");
    file_delete("RAM:test_manual.dat");
    file_delete(TEST_DB_CREATE_NEW);
    file_delete(TEST_DB_ALLOCATE);
    file_delete(TEST_DB_WRITE_READ);
    file_delete(TEST_DB_CHECKSUM);
    file_delete(TEST_DB_REOPEN);
    file_delete("RAM:cache_create.db");
    file_delete("RAM:cache_loads.db");
    file_delete("RAM:cache_lru.db");
    file_delete("RAM:cache_pin.db");
    file_delete("RAM:cache_dirty.db");
    file_delete("RAM:cache_pinlist.db");
    file_delete("RAM:test_btree.db");

    test_printf("  Testing memory allocations...\n");

    /* Test small allocation (pager struct ~60 bytes) */
    ptr1 = mem_alloc(64, AMIDB_MEM_CLEAR);
    test_printf("  mem_alloc(64) = %p\n", ptr1);
    ASSERT_NOT_NULL(ptr1);

    /* Test medium allocation (file path ~30 bytes) */
    ptr2 = mem_alloc(32, 0);
    test_printf("  mem_alloc(32) = %p\n", ptr2);
    ASSERT_NOT_NULL(ptr2);

    /* Test large allocation (page buffer 4096 bytes) */
    ptr3 = mem_alloc(4096, AMIDB_MEM_CLEAR);
    test_printf("  mem_alloc(4096) = %p\n", ptr3);
    ASSERT_NOT_NULL(ptr3);

    /* Test bitmap allocation (512 bytes) */
    ptr4 = mem_alloc(512, AMIDB_MEM_CLEAR);
    test_printf("  mem_alloc(512) = %p\n", ptr4);
    ASSERT_NOT_NULL(ptr4);

    /* Free in reverse order */
    mem_free(ptr4, 512);
    mem_free(ptr3, 4096);
    mem_free(ptr2, 32);
    mem_free(ptr1, 64);

    TEST_END();
    return 0;
}

/* Test: Basic file creation */
TEST(pager_file_test) {
    void *file;
    uint8_t buf[10];
    int32_t result;

    TEST_BEGIN();

    /* Try to create a simple file */
    test_printf("  Attempting file_open with CREATE flag...\n");
    file = file_open("RAM:test_simple.dat", AMIDB_O_RDWR | AMIDB_O_CREATE);
    test_printf("  file_open returned: %p\n", file);
    ASSERT_NOT_NULL(file);

    /* Try to write something */
    memset(buf, 0xAA, sizeof(buf));
    result = file_write(file, buf, sizeof(buf));
    test_printf("  file_write returned: %d\n", (int)result);
    ASSERT_EQ(result, sizeof(buf));

    file_close(file);

    TEST_END();
    return 0;
}

/* Test: Manual pager_open steps */
TEST(pager_manual_open) {
    void *file;
    void *pager_mem;
    void *page_buf;
    void *bitmap;
    int32_t write_result;

    TEST_BEGIN();

    test_printf("  Step 1: Open file...\n");
    file = file_open("RAM:test_manual.dat", AMIDB_O_RDWR | AMIDB_O_CREATE);
    test_printf("    file = %p\n", file);
    ASSERT_NOT_NULL(file);

    test_printf("  Step 2: Allocate pager struct (64 bytes)...\n");
    pager_mem = mem_alloc(64, AMIDB_MEM_CLEAR);
    test_printf("    pager_mem = %p\n", pager_mem);
    ASSERT_NOT_NULL(pager_mem);

    test_printf("  Step 3: Allocate page buffer (4096 bytes)...\n");
    page_buf = mem_alloc(AMIDB_PAGE_SIZE, AMIDB_MEM_CLEAR);
    test_printf("    page_buf = %p\n", page_buf);
    ASSERT_NOT_NULL(page_buf);

    test_printf("  Step 4: Allocate bitmap (512 bytes)...\n");
    bitmap = mem_alloc(512, AMIDB_MEM_CLEAR);
    test_printf("    bitmap = %p\n", bitmap);
    ASSERT_NOT_NULL(bitmap);

    test_printf("  Step 5: Write page (4096 bytes)...\n");
    write_result = file_write(file, page_buf, AMIDB_PAGE_SIZE);
    test_printf("    file_write returned: %d (expected 4096)\n", (int)write_result);
    ASSERT_EQ(write_result, AMIDB_PAGE_SIZE);

    test_printf("  Step 6: Cleanup...\n");
    file_close(file);
    mem_free(bitmap, 512);
    mem_free(page_buf, AMIDB_PAGE_SIZE);
    mem_free(pager_mem, 64);

    TEST_END();
    return 0;
}

/* Test: Create new database */
TEST(pager_create_new) {
    struct amidb_pager *pager = NULL;
    int rc;

    TEST_BEGIN();

    /* Create new database with unique filename */
    test_printf("  Creating new database...\n");
    rc = pager_open(TEST_DB_CREATE_NEW, 0, &pager);
    test_printf("  pager_open returned: %d, pager=%p\n", rc, pager);
    ASSERT_EQ(rc, 0);
    ASSERT_NOT_NULL(pager);

    /* Verify initial page count */
    test_printf("  Checking page count...\n");
    test_printf("  pager_get_page_count returned: %u\n", pager_get_page_count(pager));
    ASSERT_EQ(pager_get_page_count(pager), 1);

    pager_close(pager);

    TEST_END();
    return 0;
}

/* Test: Allocate pages */
TEST(pager_allocate_pages) {
    struct amidb_pager *pager = NULL;
    uint32_t page1, page2, page3;
    int rc;

    TEST_BEGIN();

    /* Use unique database file */
    rc = pager_open(TEST_DB_ALLOCATE, 0, &pager);
    ASSERT_EQ(rc, 0);

    /* Allocate three pages */
    rc = pager_allocate_page(pager, &page1);
    ASSERT_EQ(rc, 0);
    test_printf("  First allocated page: %u (expected 1)\n", page1);
    ASSERT_EQ(page1, 1);  /* First page after header */

    rc = pager_allocate_page(pager, &page2);
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(page2, 2);

    rc = pager_allocate_page(pager, &page3);
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(page3, 3);

    ASSERT_EQ(pager_get_page_count(pager), 4);

    pager_close(pager);

    TEST_END();
    return 0;
}

/* Test: Write and read page */
TEST(pager_write_read_page) {
    struct amidb_pager *pager = NULL;
    uint32_t page_num;
    uint8_t write_data[AMIDB_PAGE_SIZE];
    uint8_t read_data[AMIDB_PAGE_SIZE];
    int rc;
    uint32_t i;

    TEST_BEGIN();

    /* Use unique database file */
    rc = pager_open(TEST_DB_WRITE_READ, 0, &pager);
    ASSERT_EQ(rc, 0);

    /* Allocate page */
    rc = pager_allocate_page(pager, &page_num);
    test_printf("  Allocated page_num=%u, page_count=%u (expected page_num=1, page_count=2)\n",
                page_num, pager_get_page_count(pager));
    ASSERT_EQ(rc, 0);

    /* Prepare test data */
    memset(write_data, 0, AMIDB_PAGE_SIZE);
    write_data[4] = PAGE_TYPE_BTREE;  /* Set page type */
    for (i = 12; i < 100; i++) {
        write_data[i] = (uint8_t)(i & 0xFF);
    }

    /* Write page */
    rc = pager_write_page(pager, page_num, write_data);
    test_printf("  pager_write_page returned: %d\n", rc);
    ASSERT_EQ(rc, 0);

    /* Sync to ensure data is written */
    pager_sync(pager);

    /* Read back */
    test_printf("  Attempting to read page %u...\n", page_num);
    rc = pager_read_page(pager, page_num, read_data);
    test_printf("  pager_read_page returned: %d\n", rc);
    ASSERT_EQ(rc, 0);

    /* Verify data (skip header which gets modified) */
    for (i = 12; i < 100; i++) {
        ASSERT_EQ(read_data[i], (uint8_t)(i & 0xFF));
    }

    pager_close(pager);

    TEST_END();
    return 0;
}

/* Test: Checksum detects corruption */
TEST(pager_checksum_verification) {
    struct amidb_pager *pager = NULL;
    uint32_t page_num;
    uint8_t write_data[AMIDB_PAGE_SIZE];
    uint8_t read_data[AMIDB_PAGE_SIZE];
    int rc;
    void *file;

    TEST_BEGIN();

    /* Use unique database file */
    rc = pager_open(TEST_DB_CHECKSUM, 0, &pager);
    ASSERT_EQ(rc, 0);

    /* Allocate and write page */
    rc = pager_allocate_page(pager, &page_num);
    ASSERT_EQ(rc, 0);

    memset(write_data, 0xAA, AMIDB_PAGE_SIZE);
    write_data[4] = PAGE_TYPE_BTREE;

    rc = pager_write_page(pager, page_num, write_data);
    ASSERT_EQ(rc, 0);

    pager_sync(pager);
    pager_close(pager);

    /* Manually corrupt the page on disk */
    file = file_open(TEST_DB_CHECKSUM, AMIDB_O_RDWR);
    ASSERT_NOT_NULL(file);

    file_seek(file, page_num * AMIDB_PAGE_SIZE + 100, AMIDB_SEEK_SET);
    write_data[0] = 0xFF;  /* Corrupt one byte */
    file_write(file, write_data, 1);
    file_close(file);

    /* Try to read corrupted page - should fail */
    rc = pager_open(TEST_DB_CHECKSUM, 0, &pager);
    ASSERT_EQ(rc, 0);

    rc = pager_read_page(pager, page_num, read_data);
    ASSERT_NEQ(rc, 0);  /* Should fail checksum */

    pager_close(pager);

    TEST_END();
    return 0;
}

/* Test: Reopen database */
TEST(pager_reopen_database) {
    struct amidb_pager *pager = NULL;
    uint32_t page_num;
    uint8_t write_data[AMIDB_PAGE_SIZE];
    uint8_t read_data[AMIDB_PAGE_SIZE];
    int rc;
    uint32_t i;

    TEST_BEGIN();

    /* Create and write with unique database file */
    rc = pager_open(TEST_DB_REOPEN, 0, &pager);
    ASSERT_EQ(rc, 0);

    rc = pager_allocate_page(pager, &page_num);
    ASSERT_EQ(rc, 0);

    memset(write_data, 0, AMIDB_PAGE_SIZE);
    write_data[4] = PAGE_TYPE_BTREE;
    for (i = 12; i < 50; i++) {
        write_data[i] = 0x42;
    }

    rc = pager_write_page(pager, page_num, write_data);
    ASSERT_EQ(rc, 0);

    pager_sync(pager);
    pager_close(pager);

    /* Reopen and verify */
    rc = pager_open(TEST_DB_REOPEN, 0, &pager);
    ASSERT_EQ(rc, 0);

    rc = pager_read_page(pager, page_num, read_data);
    ASSERT_EQ(rc, 0);

    for (i = 12; i < 50; i++) {
        ASSERT_EQ(read_data[i], 0x42);
    }

    pager_close(pager);

    TEST_END();
    return 0;
}
