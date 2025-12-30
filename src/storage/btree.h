/*
 * btree.h - B+Tree index implementation
 *
 * B+Tree structure for indexing database records.
 * Uses iterative traversal to avoid stack overflow (68000 has only 4KB stack).
 */

#ifndef AMIDB_BTREE_H
#define AMIDB_BTREE_H

#include <stdint.h>

/* Forward declarations */
struct amidb_pager;
struct page_cache;
struct txn_context;

/* B+Tree configuration */
#define BTREE_ORDER 64          /* Maximum keys per node (fits in 4KB page) */
#define BTREE_MIN_KEYS 32       /* Minimum keys per node (for splits) */
#define BTREE_MAX_HEIGHT 16     /* Maximum tree height */

/* B+Tree node types */
#define BTREE_NODE_INTERNAL 1
#define BTREE_NODE_LEAF     2

/* B+Tree key/value pair */
struct btree_entry {
    int32_t key;                /* Key value */
    uint32_t value;             /* Value (page number or record ID) */
};

/* B+Tree node structure (stored in a page) */
struct btree_node {
    uint8_t  node_type;         /* BTREE_NODE_INTERNAL or BTREE_NODE_LEAF */
    uint8_t  reserved[3];       /* Padding for alignment */
    uint32_t num_keys;          /* Number of keys in this node */
    uint32_t parent;            /* Parent page number (0 if root) */
    uint32_t next_leaf;         /* Next leaf page (for leaf nodes, 0 if none) */

    /* For internal nodes: keys[i] and children[i] */
    /* For leaf nodes: keys[i] and values[i] */
    int32_t  keys[BTREE_ORDER];
    uint32_t children[BTREE_ORDER + 1];  /* For internal nodes */
    uint32_t values[BTREE_ORDER];        /* For leaf nodes */
};

/* B+Tree cursor for iteration */
struct btree_cursor {
    struct amidb_pager *pager;
    struct page_cache *cache;

    uint32_t current_page;      /* Current page number */
    uint32_t current_index;     /* Current key index within page */

    /* Path from root to current position (for traversal) */
    struct {
        uint32_t page_num;
        uint32_t index;
    } path[BTREE_MAX_HEIGHT];
    uint32_t path_depth;

    /* Current key/value */
    int32_t key;
    uint32_t value;

    uint8_t valid;              /* 1 if cursor points to valid entry */
};

/* B+Tree handle */
struct btree {
    struct amidb_pager *pager;
    struct page_cache *cache;
    struct txn_context *txn;    /* Active transaction (NULL if none) */
    uint32_t root_page;         /* Root page number */
    uint32_t num_entries;       /* Total number of entries */
};

/*
 * Create a new B+Tree
 *
 * pager: Pager for page I/O
 * cache: Page cache
 * root_page_out: Output root page number
 *
 * Returns: B+Tree handle on success, NULL on error
 */
struct btree *btree_create(struct amidb_pager *pager, struct page_cache *cache,
                           uint32_t *root_page_out);

/*
 * Open an existing B+Tree
 *
 * pager: Pager for page I/O
 * cache: Page cache
 * root_page: Root page number
 *
 * Returns: B+Tree handle on success, NULL on error
 */
struct btree *btree_open(struct amidb_pager *pager, struct page_cache *cache,
                         uint32_t root_page);

/*
 * Close a B+Tree
 *
 * Flushes any cached changes and frees resources.
 */
void btree_close(struct btree *tree);

/*
 * Set transaction context for B+Tree
 *
 * Associates a transaction with this B+Tree so that all modifications
 * are tracked in the transaction for ACID guarantees.
 *
 * txn: Transaction context (or NULL to clear)
 */
void btree_set_transaction(struct btree *tree, struct txn_context *txn);

/*
 * Insert a key/value pair
 *
 * Phase 3B: Full split support - handles node splits and tree growth
 *
 * key: Key to insert
 * value: Value to associate with key
 *
 * Returns: 0 on success, -1 on error
 */
int btree_insert(struct btree *tree, int32_t key, uint32_t value);

/*
 * Search for a key
 *
 * key: Key to search for
 * value_out: Output value if found
 *
 * Returns: 0 if found, -1 if not found
 */
int btree_search(struct btree *tree, int32_t key, uint32_t *value_out);

/*
 * Delete a key
 *
 * Phase 3A: Only works if node stays valid after delete (no merge)
 * Phase 3B: Will implement merge logic
 *
 * key: Key to delete
 *
 * Returns: 0 on success, -1 if not found
 */
int btree_delete(struct btree *tree, int32_t key);

/*
 * Create a cursor positioned at the first entry
 *
 * Returns: 0 on success, -1 on error
 */
int btree_cursor_first(struct btree *tree, struct btree_cursor *cursor);

/*
 * Move cursor to next entry
 *
 * Returns: 0 on success, -1 if no more entries
 */
int btree_cursor_next(struct btree_cursor *cursor);

/*
 * Check if cursor is valid
 *
 * Returns: 1 if valid, 0 if not
 */
int btree_cursor_valid(struct btree_cursor *cursor);

/*
 * Get current key/value from cursor
 *
 * key_out: Output key
 * value_out: Output value
 *
 * Returns: 0 on success, -1 if cursor invalid
 */
int btree_cursor_get(struct btree_cursor *cursor, int32_t *key_out, uint32_t *value_out);

/*
 * Get tree statistics
 *
 * num_entries: Output total number of entries
 * height: Output tree height
 * num_nodes: Output total number of nodes
 */
void btree_get_stats(struct btree *tree, uint32_t *num_entries, uint32_t *height, uint32_t *num_nodes);

#endif /* AMIDB_BTREE_H */
