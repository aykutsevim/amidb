/*
 * cache.c - LRU page cache implementation
 */

#include "storage/cache.h"
#include "storage/pager.h"
#include "os/mem.h"
#include <string.h>

/* Forward declarations of internal functions */
static struct cache_entry *find_entry(struct page_cache *cache, uint32_t page_num);
static struct cache_entry *find_free_entry(struct page_cache *cache);
static struct cache_entry *evict_lru_page(struct page_cache *cache);
static void move_to_lru_head(struct page_cache *cache, struct cache_entry *entry);
static void remove_from_lru(struct page_cache *cache, struct cache_entry *entry);
static void add_to_lru_head(struct page_cache *cache, struct cache_entry *entry);

/*
 * Create a new page cache
 */
struct page_cache *cache_create(uint32_t capacity, struct amidb_pager *pager) {
    struct page_cache *cache;
    uint32_t i;

    if (!pager) {
        return NULL;
    }

    if (capacity == 0) {
        capacity = AMIDB_DEFAULT_CACHE_SIZE;
    }

    /* Allocate cache structure */
    cache = (struct page_cache *)mem_alloc(sizeof(struct page_cache), AMIDB_MEM_CLEAR);
    if (!cache) {
        return NULL;
    }

    /* Allocate cache entries */
    cache->entries = (struct cache_entry *)mem_alloc(
        capacity * sizeof(struct cache_entry),
        AMIDB_MEM_CLEAR
    );
    if (!cache->entries) {
        mem_free(cache, sizeof(struct page_cache));
        return NULL;
    }

    cache->pager = pager;
    cache->capacity = capacity;
    cache->count = 0;
    cache->lru_head = NULL;
    cache->lru_tail = NULL;

    /* Initialize all entries as invalid */
    for (i = 0; i < capacity; i++) {
        cache->entries[i].page_num = 0;
        cache->entries[i].state = CACHE_ENTRY_INVALID;
        cache->entries[i].pin_count = 0;
        cache->entries[i].txn_id = 0;   /* Phase 3C */
        cache->entries[i].lru_prev = NULL;
        cache->entries[i].lru_next = NULL;
    }

    return cache;
}

/*
 * Destroy a page cache
 */
void cache_destroy(struct page_cache *cache) {
    if (!cache) {
        return;
    }

    /* Flush all dirty pages */
    cache_flush(cache);

    /* Free entries */
    if (cache->entries) {
        mem_free(cache->entries, cache->capacity * sizeof(struct cache_entry));
    }

    /* Free cache structure */
    mem_free(cache, sizeof(struct page_cache));
}

/*
 * Find a cache entry by page number
 */
static struct cache_entry *find_entry(struct page_cache *cache, uint32_t page_num) {
    uint32_t i;

    for (i = 0; i < cache->capacity; i++) {
        if (cache->entries[i].state != CACHE_ENTRY_INVALID &&
            cache->entries[i].page_num == page_num) {
            return &cache->entries[i];
        }
    }

    return NULL;
}

/*
 * Find a free (invalid) cache entry
 */
static struct cache_entry *find_free_entry(struct page_cache *cache) {
    uint32_t i;

    for (i = 0; i < cache->capacity; i++) {
        if (cache->entries[i].state == CACHE_ENTRY_INVALID) {
            return &cache->entries[i];
        }
    }

    return NULL;
}

/*
 * Remove an entry from the LRU list
 */
static void remove_from_lru(struct page_cache *cache, struct cache_entry *entry) {
    if (entry->lru_prev) {
        entry->lru_prev->lru_next = entry->lru_next;
    } else {
        cache->lru_head = entry->lru_next;
    }

    if (entry->lru_next) {
        entry->lru_next->lru_prev = entry->lru_prev;
    } else {
        cache->lru_tail = entry->lru_prev;
    }

    entry->lru_prev = NULL;
    entry->lru_next = NULL;
}

/*
 * Add an entry to the head of the LRU list (most recently used)
 */
static void add_to_lru_head(struct page_cache *cache, struct cache_entry *entry) {
    entry->lru_prev = NULL;
    entry->lru_next = cache->lru_head;

    if (cache->lru_head) {
        cache->lru_head->lru_prev = entry;
    }

    cache->lru_head = entry;

    if (!cache->lru_tail) {
        cache->lru_tail = entry;
    }
}

/*
 * Move an entry to the head of the LRU list
 */
static void move_to_lru_head(struct page_cache *cache, struct cache_entry *entry) {
    /* If already at head, nothing to do */
    if (cache->lru_head == entry) {
        return;
    }

    /* Remove from current position */
    remove_from_lru(cache, entry);

    /* Add to head */
    add_to_lru_head(cache, entry);
}

/*
 * Evict the least recently used page
 */
static struct cache_entry *evict_lru_page(struct page_cache *cache) {
    struct cache_entry *victim;

    /* Start from tail (least recently used) */
    victim = cache->lru_tail;

    while (victim) {
        /* Phase 3C: Can't evict pinned pages OR pages in active transaction */
        if (victim->pin_count == 0 && victim->txn_id == 0) {
            /* Flush if dirty */
            if (victim->state == CACHE_ENTRY_DIRTY) {
                pager_write_page(cache->pager, victim->page_num, victim->data);
            }

            /* Remove from LRU list */
            remove_from_lru(cache, victim);

            /* Mark as invalid */
            victim->state = CACHE_ENTRY_INVALID;
            victim->page_num = 0;

            cache->count--;

            return victim;
        }

        /* Try previous entry */
        victim = victim->lru_prev;
    }

    /* All pages are pinned! */
    return NULL;
}

/*
 * Get a page from cache
 */
int cache_get_page(struct page_cache *cache, uint32_t page_num, uint8_t **data) {
    struct cache_entry *entry;
    int rc;

    if (!cache || !data) {
        return -1;
    }

    /* Check if already in cache */
    entry = find_entry(cache, page_num);

    if (entry) {
        /* Found in cache - move to head of LRU */
        move_to_lru_head(cache, entry);

        /* Pin the page */
        entry->pin_count++;

        *data = entry->data;
        return 0;
    }

    /* Not in cache - need to load it */

    /* Try to find a free entry */
    entry = find_free_entry(cache);

    if (!entry) {
        /* No free entry - need to evict */
        entry = evict_lru_page(cache);

        if (!entry) {
            /* All pages are pinned - can't evict */
            return -1;
        }
    }

    /* Load page from disk */
    rc = pager_read_page(cache->pager, page_num, entry->data);
    if (rc != 0) {
        return -1;
    }

    /* Initialize entry */
    entry->page_num = page_num;
    entry->state = CACHE_ENTRY_CLEAN;
    entry->pin_count = 1;  /* Automatically pinned */

    /* Add to head of LRU */
    add_to_lru_head(cache, entry);

    cache->count++;

    *data = entry->data;
    return 0;
}

/*
 * Mark a page as dirty
 */
int cache_mark_dirty(struct page_cache *cache, uint32_t page_num) {
    struct cache_entry *entry;

    if (!cache) {
        return -1;
    }

    entry = find_entry(cache, page_num);
    if (!entry) {
        return -1;
    }

    entry->state = CACHE_ENTRY_DIRTY;
    return 0;
}

/*
 * Pin a page
 */
int cache_pin(struct page_cache *cache, uint32_t page_num, struct cache_pin_list *pins) {
    struct cache_entry *entry;

    if (!cache) {
        return -1;
    }

    entry = find_entry(cache, page_num);
    if (!entry) {
        return -1;
    }

    entry->pin_count++;

    /* Add to pin list if provided */
    if (pins && pins->count < AMIDB_MAX_PINNED_PAGES) {
        pins->pages[pins->count] = page_num;
        pins->count++;
    }

    return 0;
}

/*
 * Unpin a page
 */
int cache_unpin(struct page_cache *cache, uint32_t page_num) {
    struct cache_entry *entry;

    if (!cache) {
        return -1;
    }

    entry = find_entry(cache, page_num);
    if (!entry) {
        return -1;
    }

    if (entry->pin_count > 0) {
        entry->pin_count--;
    }

    return 0;
}

/*
 * Unpin all pages in a pin list
 */
void cache_unpin_all(struct page_cache *cache, struct cache_pin_list *pins) {
    int i;

    if (!cache || !pins) {
        return;
    }

    for (i = 0; i < pins->count; i++) {
        cache_unpin(cache, pins->pages[i]);
    }

    pins->count = 0;
}

/*
 * Flush all dirty pages to disk
 */
int cache_flush(struct page_cache *cache) {
    uint32_t i;
    int rc;

    if (!cache) {
        return -1;
    }

    for (i = 0; i < cache->capacity; i++) {
        if (cache->entries[i].state == CACHE_ENTRY_DIRTY) {
            /* Phase 3C: Skip pages belonging to uncommitted transactions */
            if (cache->entries[i].txn_id != 0) {
                continue;  /* Don't flush uncommitted transaction pages */
            }

            rc = pager_write_page(cache->pager,
                                  cache->entries[i].page_num,
                                  cache->entries[i].data);
            if (rc != 0) {
                return -1;
            }

            cache->entries[i].state = CACHE_ENTRY_CLEAN;
        }
    }

    /* Sync pager to disk */
    pager_sync(cache->pager);

    return 0;
}

/*
 * Get cache statistics
 */
void cache_get_stats(struct page_cache *cache, uint32_t *cached, uint32_t *dirty, uint32_t *pinned) {
    uint32_t i;
    uint32_t dirty_count = 0;
    uint32_t pinned_count = 0;

    if (!cache) {
        if (cached) *cached = 0;
        if (dirty) *dirty = 0;
        if (pinned) *pinned = 0;
        return;
    }

    for (i = 0; i < cache->capacity; i++) {
        if (cache->entries[i].state != CACHE_ENTRY_INVALID) {
            if (cache->entries[i].state == CACHE_ENTRY_DIRTY) {
                dirty_count++;
            }
            if (cache->entries[i].pin_count > 0) {
                pinned_count++;
            }
        }
    }

    if (cached) *cached = cache->count;
    if (dirty) *dirty = dirty_count;
    if (pinned) *pinned = pinned_count;
}

/*
 * Find a cache entry by page number (Phase 3C: for transaction support)
 */
struct cache_entry *cache_find_entry(struct page_cache *cache, uint32_t page_num) {
    return find_entry(cache, page_num);
}
