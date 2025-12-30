/*
 * cache.h - LRU page cache for AmiDB
 *
 * Implements a fixed-size LRU (Least Recently Used) page cache
 * with support for page pinning to prevent eviction during operations.
 */

#ifndef AMIDB_CACHE_H
#define AMIDB_CACHE_H

#include <stdint.h>

/* Default cache size: 64 pages = 256KB */
#define AMIDB_DEFAULT_CACHE_SIZE 64

/* Maximum pinned pages per operation */
#define AMIDB_MAX_PINNED_PAGES 16

/* Page size */
#define AMIDB_PAGE_SIZE 4096

/* Forward declarations */
struct amidb_pager;

/* Cache entry states */
#define CACHE_ENTRY_INVALID 0
#define CACHE_ENTRY_CLEAN   1
#define CACHE_ENTRY_DIRTY   2

/* Cache entry */
struct cache_entry {
    uint32_t page_num;        /* Page number (0 = invalid) */
    uint8_t  state;           /* INVALID, CLEAN, or DIRTY */
    uint8_t  pin_count;       /* Number of times pinned */
    uint16_t reserved;        /* Padding for alignment */
    uint64_t txn_id;          /* Phase 3C: Transaction ID (0 = none) */
    uint8_t  data[AMIDB_PAGE_SIZE];  /* Page data */

    /* LRU links */
    struct cache_entry *lru_prev;
    struct cache_entry *lru_next;
};

/* Page cache */
struct page_cache {
    struct amidb_pager *pager;    /* Associated pager */
    uint32_t capacity;             /* Maximum number of pages */
    uint32_t count;                /* Current number of cached pages */

    struct cache_entry *entries;   /* Array of cache entries */

    /* LRU list (most recent at head, least recent at tail) */
    struct cache_entry *lru_head;
    struct cache_entry *lru_tail;
};

/* Pin list for tracking pinned pages during an operation */
struct cache_pin_list {
    uint32_t pages[AMIDB_MAX_PINNED_PAGES];
    int count;
};

/*
 * Create a new page cache
 *
 * capacity: Maximum number of pages to cache (0 = use default)
 * pager: Associated pager for reading/writing pages
 *
 * Returns: Pointer to cache on success, NULL on error
 */
struct page_cache *cache_create(uint32_t capacity, struct amidb_pager *pager);

/*
 * Destroy a page cache
 *
 * Flushes all dirty pages before destroying.
 */
void cache_destroy(struct page_cache *cache);

/*
 * Get a page from cache
 *
 * If the page is not in cache, it will be loaded from disk.
 * The page is automatically pinned and must be unpinned after use.
 *
 * page_num: Page number to get
 * data: Output pointer to page data (valid until unpinned)
 *
 * Returns: 0 on success, -1 on error
 */
int cache_get_page(struct page_cache *cache, uint32_t page_num, uint8_t **data);

/*
 * Mark a page as dirty
 *
 * page_num: Page number to mark dirty
 *
 * Returns: 0 on success, -1 on error
 */
int cache_mark_dirty(struct page_cache *cache, uint32_t page_num);

/*
 * Pin a page to prevent eviction
 *
 * page_num: Page number to pin
 * pins: Pin list to track the pin (optional, can be NULL)
 *
 * Returns: 0 on success, -1 on error
 */
int cache_pin(struct page_cache *cache, uint32_t page_num, struct cache_pin_list *pins);

/*
 * Unpin a page
 *
 * page_num: Page number to unpin
 *
 * Returns: 0 on success, -1 on error
 */
int cache_unpin(struct page_cache *cache, uint32_t page_num);

/*
 * Unpin all pages in a pin list
 *
 * pins: Pin list to unpin
 */
void cache_unpin_all(struct page_cache *cache, struct cache_pin_list *pins);

/*
 * Flush all dirty pages to disk
 *
 * Returns: 0 on success, -1 on error
 */
int cache_flush(struct page_cache *cache);

/*
 * Get cache statistics
 *
 * cached: Output number of pages in cache
 * dirty: Output number of dirty pages
 * pinned: Output number of pinned pages
 */
void cache_get_stats(struct page_cache *cache, uint32_t *cached, uint32_t *dirty, uint32_t *pinned);

/*
 * Find a cache entry by page number (Phase 3C: for transaction support)
 *
 * page_num: Page number to find
 *
 * Returns: Pointer to cache entry if found, NULL if not in cache
 */
struct cache_entry *cache_find_entry(struct page_cache *cache, uint32_t page_num);

#endif /* AMIDB_CACHE_H */
