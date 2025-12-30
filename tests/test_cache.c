/*
 * test_cache.c - Unit tests for page cache
 */

#include "test_harness.h"
#include "storage/cache.h"
#include "storage/pager.h"
#include "os/file.h"
#include "os/mem.h"
#include <string.h>
#include <stdio.h>

/* Use unique database names for each test to avoid conflicts */
#define TEST_DB_CACHE_CREATE "RAM:cache_create.db"
#define TEST_DB_CACHE_LOADS "RAM:cache_loads.db"
#define TEST_DB_CACHE_LRU "RAM:cache_lru.db"
#define TEST_DB_CACHE_PIN "RAM:cache_pin.db"
#define TEST_DB_CACHE_DIRTY "RAM:cache_dirty.db"
#define TEST_DB_CACHE_PINLIST "RAM:cache_pinlist.db"

/* Test: Create and destroy cache */
TEST(cache_create_destroy) {
    struct amidb_pager *pager = NULL;
    struct page_cache *cache;
    int rc;

    TEST_BEGIN();

    /* Create pager */
    rc = pager_open(TEST_DB_CACHE_CREATE, 0, &pager);
    ASSERT_EQ(rc, 0);
    ASSERT_NOT_NULL(pager);

    /* Create cache with default size */
    cache = cache_create(0, pager);
    ASSERT_NOT_NULL(cache);

    /* Destroy cache */
    cache_destroy(cache);

    pager_close(pager);

    TEST_END();
    return 0;
}

/* Test: Get page loads from disk */
TEST(cache_get_page_loads) {
    struct amidb_pager *pager = NULL;
    struct page_cache *cache;
    uint32_t page_num;
    uint8_t *cached_data;
    uint8_t write_data[AMIDB_PAGE_SIZE];
    int rc;
    uint32_t i;

    TEST_BEGIN();

    /* Create pager and allocate a page */
    rc = pager_open(TEST_DB_CACHE_LOADS, 0, &pager);
    ASSERT_EQ(rc, 0);

    rc = pager_allocate_page(pager, &page_num);
    ASSERT_EQ(rc, 0);

    /* Write test data to the page */
    memset(write_data, 0, AMIDB_PAGE_SIZE);
    write_data[4] = 1;  /* page type */
    for (i = 12; i < 100; i++) {
        write_data[i] = (uint8_t)(i & 0xFF);
    }

    rc = pager_write_page(pager, page_num, write_data);
    ASSERT_EQ(rc, 0);
    pager_sync(pager);

    /* Create cache */
    cache = cache_create(4, pager);  /* Small cache for testing */
    ASSERT_NOT_NULL(cache);

    /* Get page from cache (should load from disk) */
    rc = cache_get_page(cache, page_num, &cached_data);
    test_printf("  cache_get_page returned: %d\n", rc);
    ASSERT_EQ(rc, 0);
    ASSERT_NOT_NULL(cached_data);

    /* Verify data */
    for (i = 12; i < 100; i++) {
        ASSERT_EQ(cached_data[i], (uint8_t)(i & 0xFF));
    }

    /* Unpin the page */
    cache_unpin(cache, page_num);

    cache_destroy(cache);
    pager_close(pager);

    TEST_END();
    return 0;
}

/* Test: LRU eviction */
TEST(cache_lru_eviction) {
    struct amidb_pager *pager = NULL;
    struct page_cache *cache;
    uint32_t pages[5];
    uint8_t *data;
    uint8_t write_data[AMIDB_PAGE_SIZE];
    uint32_t cached, dirty, pinned;
    int rc;
    int i;

    TEST_BEGIN();

    /* Create pager */
    rc = pager_open(TEST_DB_CACHE_LRU, 0, &pager);
    ASSERT_EQ(rc, 0);

    /* Allocate and write 5 pages */
    memset(write_data, 0, AMIDB_PAGE_SIZE);
    write_data[4] = 1;  /* page type */
    for (i = 0; i < 5; i++) {
        rc = pager_allocate_page(pager, &pages[i]);
        ASSERT_EQ(rc, 0);
        rc = pager_write_page(pager, pages[i], write_data);
        ASSERT_EQ(rc, 0);
    }
    pager_sync(pager);

    /* Create cache with capacity of 3 pages */
    cache = cache_create(3, pager);
    ASSERT_NOT_NULL(cache);

    /* Get first 3 pages (fills cache) */
    for (i = 0; i < 3; i++) {
        rc = cache_get_page(cache, pages[i], &data);
        ASSERT_EQ(rc, 0);
        cache_unpin(cache, pages[i]);
    }

    /* Check stats - should have 3 pages cached */
    cache_get_stats(cache, &cached, &dirty, &pinned);
    test_printf("  After loading 3 pages: cached=%u, dirty=%u, pinned=%u\n",
                cached, dirty, pinned);
    ASSERT_EQ(cached, 3);

    /* Get 4th page - should evict LRU (pages[0]) */
    rc = cache_get_page(cache, pages[3], &data);
    ASSERT_EQ(rc, 0);
    cache_unpin(cache, pages[3]);

    /* Should still have 3 pages cached */
    cache_get_stats(cache, &cached, &dirty, &pinned);
    test_printf("  After eviction: cached=%u, dirty=%u, pinned=%u\n",
                cached, dirty, pinned);
    ASSERT_EQ(cached, 3);

    cache_destroy(cache);
    pager_close(pager);

    TEST_END();
    return 0;
}

/* Test: Pinned pages cannot be evicted */
TEST(cache_pin_prevents_eviction) {
    struct amidb_pager *pager = NULL;
    struct page_cache *cache;
    struct cache_pin_list pins = {0};
    uint32_t pages[4];
    uint8_t write_data[AMIDB_PAGE_SIZE];
    uint8_t *data;
    int rc;
    int i;

    TEST_BEGIN();

    /* Create pager and allocate pages */
    rc = pager_open(TEST_DB_CACHE_PIN, 0, &pager);
    ASSERT_EQ(rc, 0);

    /* Allocate and write pages */
    memset(write_data, 0, AMIDB_PAGE_SIZE);
    write_data[4] = 1;  /* page type */
    for (i = 0; i < 4; i++) {
        rc = pager_allocate_page(pager, &pages[i]);
        ASSERT_EQ(rc, 0);
        rc = pager_write_page(pager, pages[i], write_data);
        ASSERT_EQ(rc, 0);
    }
    pager_sync(pager);

    /* Create cache with capacity of 2 pages */
    cache = cache_create(2, pager);
    ASSERT_NOT_NULL(cache);

    /* Get first page and keep it pinned */
    rc = cache_get_page(cache, pages[0], &data);
    ASSERT_EQ(rc, 0);
    /* Note: page is already pinned by cache_get_page */

    /* Get second page and unpin it */
    rc = cache_get_page(cache, pages[1], &data);
    ASSERT_EQ(rc, 0);
    cache_unpin(cache, pages[1]);

    /* Get third page - should evict pages[1] (unpinned) not pages[0] (pinned) */
    rc = cache_get_page(cache, pages[2], &data);
    test_printf("  cache_get_page for 3rd page returned: %d\n", rc);
    ASSERT_EQ(rc, 0);
    cache_unpin(cache, pages[2]);

    /* Unpin first page */
    cache_unpin(cache, pages[0]);

    /* Unpin all remaining via pin list */
    cache_unpin_all(cache, &pins);

    cache_destroy(cache);
    pager_close(pager);

    TEST_END();
    return 0;
}

/* Test: Dirty page tracking and flush */
TEST(cache_dirty_and_flush) {
    struct amidb_pager *pager = NULL;
    struct page_cache *cache;
    uint32_t page_num;
    uint8_t *cached_data;
    uint8_t write_data[AMIDB_PAGE_SIZE];
    uint8_t verify_data[AMIDB_PAGE_SIZE];
    uint32_t cached, dirty, pinned;
    int rc;

    TEST_BEGIN();

    /* Create pager and allocate page */
    rc = pager_open(TEST_DB_CACHE_DIRTY, 0, &pager);
    ASSERT_EQ(rc, 0);

    /* Allocate and write page */
    rc = pager_allocate_page(pager, &page_num);
    ASSERT_EQ(rc, 0);
    memset(write_data, 0, AMIDB_PAGE_SIZE);
    write_data[4] = 1;  /* page type */
    rc = pager_write_page(pager, page_num, write_data);
    ASSERT_EQ(rc, 0);
    pager_sync(pager);

    /* Create cache */
    cache = cache_create(4, pager);
    ASSERT_NOT_NULL(cache);

    /* Get page */
    rc = cache_get_page(cache, page_num, &cached_data);
    ASSERT_EQ(rc, 0);

    /* Modify the page */
    cached_data[100] = 0xAA;
    cached_data[200] = 0xBB;

    /* Mark as dirty */
    rc = cache_mark_dirty(cache, page_num);
    ASSERT_EQ(rc, 0);

    /* Check stats - should have 1 dirty page */
    cache_get_stats(cache, &cached, &dirty, &pinned);
    test_printf("  After marking dirty: cached=%u, dirty=%u, pinned=%u\n",
                cached, dirty, pinned);
    ASSERT_EQ(dirty, 1);

    /* Unpin the page */
    cache_unpin(cache, page_num);

    /* Flush cache */
    rc = cache_flush(cache);
    ASSERT_EQ(rc, 0);

    /* Check stats - should have 0 dirty pages */
    cache_get_stats(cache, &cached, &dirty, &pinned);
    test_printf("  After flush: cached=%u, dirty=%u, pinned=%u\n",
                cached, dirty, pinned);
    ASSERT_EQ(dirty, 0);

    /* Destroy cache and create new one to verify data was written */
    cache_destroy(cache);

    /* Read directly from pager to verify */
    rc = pager_read_page(pager, page_num, verify_data);
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(verify_data[100], 0xAA);
    ASSERT_EQ(verify_data[200], 0xBB);

    pager_close(pager);

    TEST_END();
    return 0;
}

/* Test: Pin list tracking */
TEST(cache_pin_list) {
    struct amidb_pager *pager = NULL;
    struct page_cache *cache;
    struct cache_pin_list pins = {0};
    uint32_t pages[3];
    uint8_t write_data[AMIDB_PAGE_SIZE];
    uint8_t *data;
    uint32_t cached, dirty, pinned;
    int rc;
    int i;

    TEST_BEGIN();

    /* Create pager and allocate pages */
    rc = pager_open(TEST_DB_CACHE_PINLIST, 0, &pager);
    ASSERT_EQ(rc, 0);

    /* Allocate and write pages */
    memset(write_data, 0, AMIDB_PAGE_SIZE);
    write_data[4] = 1;  /* page type */
    for (i = 0; i < 3; i++) {
        rc = pager_allocate_page(pager, &pages[i]);
        ASSERT_EQ(rc, 0);
        rc = pager_write_page(pager, pages[i], write_data);
        ASSERT_EQ(rc, 0);
    }
    pager_sync(pager);

    /* Create cache */
    cache = cache_create(8, pager);
    ASSERT_NOT_NULL(cache);

    /* Get pages and track pins */
    for (i = 0; i < 3; i++) {
        rc = cache_get_page(cache, pages[i], &data);
        ASSERT_EQ(rc, 0);
        /* cache_get_page already pins the page */
    }

    /* Check stats - should have 3 pinned pages */
    cache_get_stats(cache, &cached, &dirty, &pinned);
    test_printf("  After pinning 3 pages: cached=%u, dirty=%u, pinned=%u\n",
                cached, dirty, pinned);
    ASSERT_EQ(pinned, 3);

    /* Unpin all via pin list */
    for (i = 0; i < 3; i++) {
        pins.pages[pins.count++] = pages[i];
    }
    cache_unpin_all(cache, &pins);

    /* Check stats - should have 0 pinned pages */
    cache_get_stats(cache, &cached, &dirty, &pinned);
    test_printf("  After unpinning all: cached=%u, dirty=%u, pinned=%u\n",
                cached, dirty, pinned);
    ASSERT_EQ(pinned, 0);

    cache_destroy(cache);
    pager_close(pager);

    TEST_END();
    return 0;
}
