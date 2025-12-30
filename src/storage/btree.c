/*
 * btree.c - B+Tree implementation (Phase 3A: Basic operations)
 */

#include "storage/btree.h"
#include "storage/pager.h"
#include "storage/cache.h"
#include "txn/txn.h"
#include "os/mem.h"
#include <string.h>

/* Helper functions for endianness */
static inline void put_u32(uint8_t *buf, uint32_t val) {
    buf[0] = (uint8_t)(val & 0xFF);
    buf[1] = (uint8_t)((val >> 8) & 0xFF);
    buf[2] = (uint8_t)((val >> 16) & 0xFF);
    buf[3] = (uint8_t)((val >> 24) & 0xFF);
}

static inline uint32_t get_u32(const uint8_t *buf) {
    return (uint32_t)buf[0] |
           ((uint32_t)buf[1] << 8) |
           ((uint32_t)buf[2] << 16) |
           ((uint32_t)buf[3] << 24);
}

static inline void put_i32(uint8_t *buf, int32_t val) {
    put_u32(buf, (uint32_t)val);
}

static inline int32_t get_i32(const uint8_t *buf) {
    return (int32_t)get_u32(buf);
}

/* Forward declarations of internal functions */
static int serialize_node(const struct btree_node *node, uint8_t *buffer);
static int deserialize_node(struct btree_node *node, const uint8_t *buffer);
static int find_key_in_node(const struct btree_node *node, int32_t key);
static int find_leaf_page(struct btree *tree, int32_t key, uint32_t *leaf_page_out);

/* Phase 3B: Split/merge functions */
static int split_leaf_node(struct btree *tree, uint32_t leaf_page, int32_t *split_key_out, uint32_t *new_page_out);
static int split_internal_node(struct btree *tree, uint32_t internal_page, int32_t *split_key_out, uint32_t *new_page_out);
static int insert_into_parent(struct btree *tree, uint32_t left_page, int32_t key, uint32_t right_page);
static int allocate_node(struct btree *tree, uint8_t node_type, uint32_t *page_out);
static int rebalance_after_delete(struct btree *tree, uint32_t page_num);
static int borrow_from_sibling(struct btree *tree, uint32_t page_num, uint32_t parent_page, int child_index);
static int merge_with_sibling(struct btree *tree, uint32_t left_page, uint32_t right_page, uint32_t parent_page, int separator_index);

/* Phase 3C: Transaction integration helper */
static void btree_mark_page_dirty(struct btree *tree, uint32_t page_num);

/*
 * Mark a page as dirty and track it in the active transaction
 */
static void btree_mark_page_dirty(struct btree *tree, uint32_t page_num) {
    struct cache_entry *entry;

    /* Always mark dirty in cache */
    cache_mark_dirty(tree->cache, page_num);

    /* If there's an active transaction, integrate with it */
    if (tree->txn) {
        /* Add to transaction's dirty page list */
        txn_add_dirty_page(tree->txn, page_num);

        /* Set transaction ID on cache entry */
        entry = cache_find_entry(tree->cache, page_num);
        if (entry) {
            entry->txn_id = tree->txn->txn_id;
        }

        /* Page will be kept in cache by transaction system */
    }
}

/*
 * Serialize a B+Tree node to a page buffer
 */
static int serialize_node(const struct btree_node *node, uint8_t *buffer) {
    uint32_t i;
    uint32_t offset = 12;  /* Skip 12-byte page header */

    /* Clear buffer (but preserve page header at bytes 0-11) */
    memset(buffer + 12, 0, AMIDB_PAGE_SIZE - 12);

    /* Write node header */
    buffer[offset++] = node->node_type;
    offset += 3;  /* Skip reserved */
    put_u32(buffer + offset, node->num_keys); offset += 4;
    put_u32(buffer + offset, node->parent); offset += 4;
    put_u32(buffer + offset, node->next_leaf); offset += 4;

    /* Write keys */
    for (i = 0; i < BTREE_ORDER; i++) {
        put_i32(buffer + offset, node->keys[i]);
        offset += 4;
    }

    /* Write children (for internal nodes) or values (for leaf nodes) */
    if (node->node_type == BTREE_NODE_INTERNAL) {
        for (i = 0; i <= BTREE_ORDER; i++) {
            put_u32(buffer + offset, node->children[i]);
            offset += 4;
        }
    } else {
        for (i = 0; i < BTREE_ORDER; i++) {
            put_u32(buffer + offset, node->values[i]);
            offset += 4;
        }
    }

    return 0;
}

/*
 * Deserialize a B+Tree node from a page buffer
 */
static int deserialize_node(struct btree_node *node, const uint8_t *buffer) {
    uint32_t i;
    uint32_t offset = 12;  /* Skip 12-byte page header */

    /* Read node header */
    node->node_type = buffer[offset++];
    offset += 3;  /* Skip reserved */
    node->num_keys = get_u32(buffer + offset); offset += 4;
    node->parent = get_u32(buffer + offset); offset += 4;
    node->next_leaf = get_u32(buffer + offset); offset += 4;

    /* Read keys */
    for (i = 0; i < BTREE_ORDER; i++) {
        node->keys[i] = get_i32(buffer + offset);
        offset += 4;
    }

    /* Read children or values */
    if (node->node_type == BTREE_NODE_INTERNAL) {
        for (i = 0; i <= BTREE_ORDER; i++) {
            node->children[i] = get_u32(buffer + offset);
            offset += 4;
        }
        /* Clear values array */
        memset(node->values, 0, sizeof(node->values));
    } else {
        for (i = 0; i < BTREE_ORDER; i++) {
            node->values[i] = get_u32(buffer + offset);
            offset += 4;
        }
        /* Clear children array */
        memset(node->children, 0, sizeof(node->children));
    }

    return 0;
}

/*
 * Binary search for key in node
 * Returns index where key is found or should be inserted
 */
static int find_key_in_node(const struct btree_node *node, int32_t key) {
    int32_t left = 0;
    int32_t right = (int32_t)node->num_keys - 1;
    int32_t mid;

    while (left <= right) {
        mid = left + (right - left) / 2;

        if (node->keys[mid] == key) {
            return mid;
        } else if (node->keys[mid] < key) {
            left = mid + 1;
        } else {
            right = mid - 1;
        }
    }

    return left;
}

/*
 * Find the leaf page that should contain the given key
 */
static int find_leaf_page(struct btree *tree, int32_t key, uint32_t *leaf_page_out) {
    uint32_t current_page;
    struct btree_node node;
    uint8_t *page_data;
    int index;

    current_page = tree->root_page;

    /* Traverse down to leaf */
    while (1) {
        /* Get page from cache */
        if (cache_get_page(tree->cache, current_page, &page_data) != 0) {
            return -1;
        }

        /* Deserialize node */
        deserialize_node(&node, page_data);

        /* Unpin page */
        cache_unpin(tree->cache, current_page);

        /* If leaf, we're done */
        if (node.node_type == BTREE_NODE_LEAF) {
            *leaf_page_out = current_page;
            return 0;
        }

        /* Internal node - find which child to follow */
        index = find_key_in_node(&node, key);

        /* For internal nodes: children[i] contains keys < keys[i] */
        /* children[num_keys] contains keys >= keys[num_keys-1] */
        if (index >= (int)node.num_keys) {
            current_page = node.children[node.num_keys];
        } else if (key < node.keys[index]) {
            current_page = node.children[index];
        } else {
            current_page = node.children[index + 1];
        }

        if (current_page == 0) {
            return -1;  /* Invalid child pointer */
        }
    }
}

/*
 * Create a new B+Tree
 */
struct btree *btree_create(struct amidb_pager *pager, struct page_cache *cache,
                           uint32_t *root_page_out) {
    struct btree *tree;
    struct btree_node root;
    uint32_t root_page;
    uint8_t *page_data;
    int rc;

    if (!pager || !cache || !root_page_out) {
        return NULL;
    }

    /* Allocate tree structure */
    tree = (struct btree *)mem_alloc(sizeof(struct btree), AMIDB_MEM_CLEAR);
    if (!tree) {
        return NULL;
    }

    /* Allocate root page */
    rc = pager_allocate_page(pager, &root_page);
    if (rc != 0) {
        mem_free(tree, sizeof(struct btree));
        return NULL;
    }

    /* Initialize root as empty leaf node */
    memset(&root, 0, sizeof(root));
    root.node_type = BTREE_NODE_LEAF;
    root.num_keys = 0;
    root.parent = 0;
    root.next_leaf = 0;

    /* Serialize and write root page */
    page_data = (uint8_t *)mem_alloc(AMIDB_PAGE_SIZE, AMIDB_MEM_CLEAR);
    if (!page_data) {
        mem_free(tree, sizeof(struct btree));
        return NULL;
    }

    serialize_node(&root, page_data);

    /* Set page type in page header (byte 4) */
    page_data[4] = PAGE_TYPE_BTREE;

    rc = pager_write_page(pager, root_page, page_data);
    mem_free(page_data, AMIDB_PAGE_SIZE);

    if (rc != 0) {
        mem_free(tree, sizeof(struct btree));
        return NULL;
    }

    pager_sync(pager);

    /* Initialize tree structure */
    tree->pager = pager;
    tree->cache = cache;
    tree->txn = NULL;  /* No transaction initially */
    tree->root_page = root_page;
    tree->num_entries = 0;

    *root_page_out = root_page;

    return tree;
}

/*
 * Open an existing B+Tree
 */
struct btree *btree_open(struct amidb_pager *pager, struct page_cache *cache,
                         uint32_t root_page) {
    struct btree *tree;

    if (!pager || !cache) {
        return NULL;
    }

    /* Allocate tree structure */
    tree = (struct btree *)mem_alloc(sizeof(struct btree), AMIDB_MEM_CLEAR);
    if (!tree) {
        return NULL;
    }

    tree->pager = pager;
    tree->cache = cache;
    tree->txn = NULL;  /* No transaction initially */
    tree->root_page = root_page;
    tree->num_entries = 0;  /* Will be computed on demand */

    return tree;
}

/*
 * Close a B+Tree
 */
void btree_close(struct btree *tree) {
    if (!tree) {
        return;
    }

    /* Phase 3C: Don't flush cache - btree doesn't own it */
    /* Cache will be flushed by cache_destroy() which respects txn_id */

    /* Free tree structure */
    mem_free(tree, sizeof(struct btree));
}

/*
 * Set transaction context for B+Tree (Phase 3C)
 */
void btree_set_transaction(struct btree *tree, struct txn_context *txn) {
    if (!tree) {
        return;
    }
    tree->txn = txn;
}

/*
 * Insert a key/value pair (Phase 3B: with split support)
 */
int btree_insert(struct btree *tree, int32_t key, uint32_t value) {
    uint32_t leaf_page;
    struct btree_node node;
    uint8_t *page_data;
    int index;
    int i;
    int32_t split_key;
    uint32_t new_page;

    if (!tree) {
        return -1;
    }

    /* Find leaf page */
    if (find_leaf_page(tree, key, &leaf_page) != 0) {
        return -1;
    }

    /* Get leaf page from cache */
    if (cache_get_page(tree->cache, leaf_page, &page_data) != 0) {
        return -1;
    }

    /* Deserialize node */
    deserialize_node(&node, page_data);

    /* Check if node is full - split if necessary (Phase 3B) */
    if (node.num_keys >= BTREE_ORDER) {
        cache_unpin(tree->cache, leaf_page);

        /* Split the leaf node */
        if (split_leaf_node(tree, leaf_page, &split_key, &new_page) != 0) {
            return -1;
        }

        /* Insert split key into parent */
        if (insert_into_parent(tree, leaf_page, split_key, new_page) != 0) {
            return -1;
        }

        /* Re-find leaf page (key may now be in different leaf) */
        if (find_leaf_page(tree, key, &leaf_page) != 0) {
            return -1;
        }

        /* Get the correct leaf page */
        if (cache_get_page(tree->cache, leaf_page, &page_data) != 0) {
            return -1;
        }

        deserialize_node(&node, page_data);
    }

    /* Find insertion position */
    index = find_key_in_node(&node, key);

    /* Check if key already exists (UPDATE case) */
    if (index < (int)node.num_keys && node.keys[index] == key) {
        /* Update existing value */
        node.values[index] = value;
    } else {
        /* Insert new key/value */
        /* Shift keys and values to make room */
        for (i = (int)node.num_keys; i > index; i--) {
            node.keys[i] = node.keys[i - 1];
            node.values[i] = node.values[i - 1];
        }

        node.keys[index] = key;
        node.values[index] = value;
        node.num_keys++;
        tree->num_entries++;
    }

    /* Serialize and write back */
    serialize_node(&node, page_data);
    btree_mark_page_dirty(tree, leaf_page);
    cache_unpin(tree->cache, leaf_page);

    return 0;
}

/*
 * Search for a key
 */
int btree_search(struct btree *tree, int32_t key, uint32_t *value_out) {
    uint32_t leaf_page;
    struct btree_node node;
    uint8_t *page_data;
    int index;

    if (!tree || !value_out) {
        return -1;
    }

    /* Find leaf page */
    if (find_leaf_page(tree, key, &leaf_page) != 0) {
        return -1;
    }

    /* Get leaf page from cache */
    if (cache_get_page(tree->cache, leaf_page, &page_data) != 0) {
        return -1;
    }

    /* Deserialize node */
    deserialize_node(&node, page_data);

    /* Search for key */
    index = find_key_in_node(&node, key);

    cache_unpin(tree->cache, leaf_page);

    /* Check if key found */
    if (index < (int)node.num_keys && node.keys[index] == key) {
        *value_out = node.values[index];
        return 0;
    }

    return -1;  /* Not found */
}

/*
 * Delete a key (Phase 3B: with merge/borrow)
 */
int btree_delete(struct btree *tree, int32_t key) {
    uint32_t leaf_page;
    struct btree_node node;
    uint8_t *page_data;
    int index;
    int i;

    if (!tree) {
        return -1;
    }

    /* Find leaf page */
    if (find_leaf_page(tree, key, &leaf_page) != 0) {
        return -1;
    }

    /* Get leaf page from cache */
    if (cache_get_page(tree->cache, leaf_page, &page_data) != 0) {
        return -1;
    }

    /* Deserialize node */
    deserialize_node(&node, page_data);

    /* Find key */
    index = find_key_in_node(&node, key);

    /* Check if key exists */
    if (index >= (int)node.num_keys || node.keys[index] != key) {
        cache_unpin(tree->cache, leaf_page);
        return -1;  /* Not found */
    }

    /* Shift keys and values to remove the entry */
    for (i = index; i < (int)node.num_keys - 1; i++) {
        node.keys[i] = node.keys[i + 1];
        node.values[i] = node.values[i + 1];
    }

    node.num_keys--;
    tree->num_entries--;

    /* Serialize and write back */
    serialize_node(&node, page_data);
    btree_mark_page_dirty(tree, leaf_page);
    cache_unpin(tree->cache, leaf_page);

    /* Rebalance tree if needed (Phase 3B) */
    if (rebalance_after_delete(tree, leaf_page) != 0) {
        return -1;
    }

    return 0;
}

/*
 * Create cursor positioned at first entry
 */
int btree_cursor_first(struct btree *tree, struct btree_cursor *cursor) {
    uint32_t current_page;
    struct btree_node node;
    uint8_t *page_data;

    if (!tree || !cursor) {
        return -1;
    }

    memset(cursor, 0, sizeof(*cursor));
    cursor->pager = tree->pager;
    cursor->cache = tree->cache;

    /* Find leftmost leaf */
    current_page = tree->root_page;

    while (1) {
        /* Get page */
        if (cache_get_page(tree->cache, current_page, &page_data) != 0) {
            return -1;
        }

        deserialize_node(&node, page_data);
        cache_unpin(tree->cache, current_page);

        if (node.node_type == BTREE_NODE_LEAF) {
            /* Found leftmost leaf */
            cursor->current_page = current_page;
            cursor->current_index = 0;

            if (node.num_keys > 0) {
                cursor->key = node.keys[0];
                cursor->value = node.values[0];
                cursor->valid = 1;
            } else {
                cursor->valid = 0;
            }

            return 0;
        }

        /* Follow leftmost child */
        current_page = node.children[0];

        if (current_page == 0) {
            return -1;
        }
    }
}

/*
 * Move cursor to next entry
 */
int btree_cursor_next(struct btree_cursor *cursor) {
    struct btree_node node;
    uint8_t *page_data;

    if (!cursor || !cursor->valid) {
        return -1;
    }

    /* Get current page */
    if (cache_get_page(cursor->cache, cursor->current_page, &page_data) != 0) {
        return -1;
    }

    deserialize_node(&node, page_data);
    cache_unpin(cursor->cache, cursor->current_page);

    /* Move to next entry in current page */
    cursor->current_index++;

    if (cursor->current_index < node.num_keys) {
        /* Still within current page */
        cursor->key = node.keys[cursor->current_index];
        cursor->value = node.values[cursor->current_index];
        return 0;
    }

    /* Move to next leaf page */
    if (node.next_leaf != 0) {
        cursor->current_page = node.next_leaf;
        cursor->current_index = 0;

        /* Get next page */
        if (cache_get_page(cursor->cache, cursor->current_page, &page_data) != 0) {
            cursor->valid = 0;
            return -1;
        }

        deserialize_node(&node, page_data);
        cache_unpin(cursor->cache, cursor->current_page);

        if (node.num_keys > 0) {
            cursor->key = node.keys[0];
            cursor->value = node.values[0];
            return 0;
        }
    }

    /* No more entries */
    cursor->valid = 0;
    return -1;
}

/*
 * Check if cursor is valid
 */
int btree_cursor_valid(struct btree_cursor *cursor) {
    if (!cursor) {
        return 0;
    }
    return cursor->valid;
}

/*
 * Get current key/value from cursor
 */
int btree_cursor_get(struct btree_cursor *cursor, int32_t *key_out, uint32_t *value_out) {
    if (!cursor || !cursor->valid) {
        return -1;
    }

    if (key_out) {
        *key_out = cursor->key;
    }

    if (value_out) {
        *value_out = cursor->value;
    }

    return 0;
}

/*
 * Allocate a new B+Tree node
 */
static int allocate_node(struct btree *tree, uint8_t node_type, uint32_t *page_out) {
    struct btree_node node;
    uint32_t new_page;
    uint8_t *page_data;
    int rc;

    /* Allocate new page */
    rc = pager_allocate_page(tree->pager, &new_page);
    if (rc != 0) {
        return -1;
    }

    /* Initialize node */
    memset(&node, 0, sizeof(node));
    node.node_type = node_type;
    node.num_keys = 0;
    node.parent = 0;
    node.next_leaf = 0;

    /* Allocate page buffer */
    page_data = (uint8_t *)mem_alloc(AMIDB_PAGE_SIZE, AMIDB_MEM_CLEAR);
    if (!page_data) {
        pager_free_page(tree->pager, new_page);
        return -1;
    }

    /* Serialize and write */
    serialize_node(&node, page_data);
    page_data[4] = PAGE_TYPE_BTREE;
    rc = pager_write_page(tree->pager, new_page, page_data);
    mem_free(page_data, AMIDB_PAGE_SIZE);

    if (rc != 0) {
        pager_free_page(tree->pager, new_page);
        return -1;
    }

    *page_out = new_page;
    return 0;
}

/*
 * Split a leaf node (Phase 3B)
 * Returns the middle key that should go to parent
 */
static int split_leaf_node(struct btree *tree, uint32_t leaf_page, int32_t *split_key_out, uint32_t *new_page_out) {
    struct btree_node old_node, new_node;
    uint8_t *old_data, *new_data;
    uint32_t new_page;
    uint32_t i, split_index;
    int rc;

    /* Get old leaf node */
    if (cache_get_page(tree->cache, leaf_page, &old_data) != 0) {
        return -1;
    }
    deserialize_node(&old_node, old_data);

    /* Allocate new leaf node */
    if (allocate_node(tree, BTREE_NODE_LEAF, &new_page) != 0) {
        cache_unpin(tree->cache, leaf_page);
        return -1;
    }

    /* Get new node */
    if (cache_get_page(tree->cache, new_page, &new_data) != 0) {
        cache_unpin(tree->cache, leaf_page);
        pager_free_page(tree->pager, new_page);
        return -1;
    }
    deserialize_node(&new_node, new_data);

    /* Split point: move half the keys to new node */
    split_index = BTREE_ORDER / 2;

    /* Copy second half to new node */
    new_node.num_keys = 0;
    for (i = split_index; i < old_node.num_keys; i++) {
        new_node.keys[new_node.num_keys] = old_node.keys[i];
        new_node.values[new_node.num_keys] = old_node.values[i];
        new_node.num_keys++;
    }

    /* Update old node */
    old_node.num_keys = split_index;

    /* Link leaves together */
    new_node.next_leaf = old_node.next_leaf;
    old_node.next_leaf = new_page;

    /* Both nodes have same parent (will be updated by insert_into_parent) */
    new_node.parent = old_node.parent;

    /* Write both nodes back */
    serialize_node(&old_node, old_data);
    btree_mark_page_dirty(tree, leaf_page);
    cache_unpin(tree->cache, leaf_page);

    serialize_node(&new_node, new_data);
    btree_mark_page_dirty(tree, new_page);
    cache_unpin(tree->cache, new_page);

    /* Return the first key of new node as split key */
    *split_key_out = new_node.keys[0];
    *new_page_out = new_page;

    return 0;
}

/*
 * Insert a key into parent after split (Phase 3B)
 */
static int insert_into_parent(struct btree *tree, uint32_t left_page, int32_t key, uint32_t right_page) {
    struct btree_node left_node, parent_node, new_root;
    uint8_t *left_data, *parent_data;
    uint32_t parent_page, new_root_page;
    int index, i;
    int32_t split_key;
    uint32_t new_page;

    /* Get left node to find its parent */
    if (cache_get_page(tree->cache, left_page, &left_data) != 0) {
        return -1;
    }
    deserialize_node(&left_node, left_data);
    parent_page = left_node.parent;
    cache_unpin(tree->cache, left_page);

    /* If no parent, create new root */
    if (parent_page == 0) {
        /* Allocate new root */
        if (allocate_node(tree, BTREE_NODE_INTERNAL, &new_root_page) != 0) {
            return -1;
        }

        /* Get new root */
        if (cache_get_page(tree->cache, new_root_page, &parent_data) != 0) {
            pager_free_page(tree->pager, new_root_page);
            return -1;
        }
        deserialize_node(&new_root, parent_data);

        /* Setup new root with two children */
        new_root.num_keys = 1;
        new_root.keys[0] = key;
        new_root.children[0] = left_page;
        new_root.children[1] = right_page;
        new_root.parent = 0;

        /* Write new root */
        serialize_node(&new_root, parent_data);
        btree_mark_page_dirty(tree, new_root_page);
        cache_unpin(tree->cache, new_root_page);

        /* Update children's parent pointers */
        if (cache_get_page(tree->cache, left_page, &left_data) != 0) {
            return -1;
        }
        deserialize_node(&left_node, left_data);
        left_node.parent = new_root_page;
        serialize_node(&left_node, left_data);
        btree_mark_page_dirty(tree, left_page);
        cache_unpin(tree->cache, left_page);

        if (cache_get_page(tree->cache, right_page, &left_data) != 0) {
            return -1;
        }
        deserialize_node(&left_node, left_data);
        left_node.parent = new_root_page;
        serialize_node(&left_node, left_data);
        btree_mark_page_dirty(tree, right_page);
        cache_unpin(tree->cache, right_page);

        /* Update tree root */
        tree->root_page = new_root_page;
        pager_sync(tree->pager);

        return 0;
    }

    /* Insert into existing parent */
    if (cache_get_page(tree->cache, parent_page, &parent_data) != 0) {
        return -1;
    }
    deserialize_node(&parent_node, parent_data);

    /* Check if parent is full */
    if (parent_node.num_keys >= BTREE_ORDER) {
        cache_unpin(tree->cache, parent_page);

        /* Split parent first */
        if (split_internal_node(tree, parent_page, &split_key, &new_page) != 0) {
            return -1;
        }

        /* Recursively insert into parent's parent */
        if (insert_into_parent(tree, parent_page, split_key, new_page) != 0) {
            return -1;
        }

        /* Re-fetch parent (may have moved due to split) */
        if (cache_get_page(tree->cache, left_page, &left_data) != 0) {
            return -1;
        }
        deserialize_node(&left_node, left_data);
        parent_page = left_node.parent;
        cache_unpin(tree->cache, left_page);

        if (cache_get_page(tree->cache, parent_page, &parent_data) != 0) {
            return -1;
        }
        deserialize_node(&parent_node, parent_data);
    }

    /* Find insertion position */
    index = find_key_in_node(&parent_node, key);

    /* Shift keys and children */
    for (i = (int)parent_node.num_keys; i > index; i--) {
        parent_node.keys[i] = parent_node.keys[i - 1];
        parent_node.children[i + 1] = parent_node.children[i];
    }

    /* Insert new key and child */
    parent_node.keys[index] = key;
    parent_node.children[index + 1] = right_page;
    parent_node.num_keys++;

    /* Write parent back */
    serialize_node(&parent_node, parent_data);
    btree_mark_page_dirty(tree, parent_page);
    cache_unpin(tree->cache, parent_page);

    /* Update right child's parent pointer */
    if (cache_get_page(tree->cache, right_page, &left_data) != 0) {
        return -1;
    }
    deserialize_node(&left_node, left_data);
    left_node.parent = parent_page;
    serialize_node(&left_node, left_data);
    btree_mark_page_dirty(tree, right_page);
    cache_unpin(tree->cache, right_page);

    return 0;
}

/*
 * Split an internal node (Phase 3B)
 */
static int split_internal_node(struct btree *tree, uint32_t internal_page, int32_t *split_key_out, uint32_t *new_page_out) {
    struct btree_node old_node, new_node, child_node;
    uint8_t *old_data, *new_data, *child_data;
    uint32_t new_page;
    uint32_t i, split_index;

    /* Get old internal node */
    if (cache_get_page(tree->cache, internal_page, &old_data) != 0) {
        return -1;
    }
    deserialize_node(&old_node, old_data);

    /* Allocate new internal node */
    if (allocate_node(tree, BTREE_NODE_INTERNAL, &new_page) != 0) {
        cache_unpin(tree->cache, internal_page);
        return -1;
    }

    /* Get new node */
    if (cache_get_page(tree->cache, new_page, &new_data) != 0) {
        cache_unpin(tree->cache, internal_page);
        pager_free_page(tree->pager, new_page);
        return -1;
    }
    deserialize_node(&new_node, new_data);

    /* Split point: middle key goes up to parent */
    split_index = BTREE_ORDER / 2;

    /* Copy second half to new node (excluding middle key) */
    new_node.num_keys = 0;
    for (i = split_index + 1; i < old_node.num_keys; i++) {
        new_node.keys[new_node.num_keys] = old_node.keys[i];
        new_node.children[new_node.num_keys] = old_node.children[i];
        new_node.num_keys++;
    }
    new_node.children[new_node.num_keys] = old_node.children[old_node.num_keys];

    /* Update children's parent pointers */
    for (i = 0; i <= new_node.num_keys; i++) {
        if (cache_get_page(tree->cache, new_node.children[i], &child_data) != 0) {
            continue;
        }
        deserialize_node(&child_node, child_data);
        child_node.parent = new_page;
        serialize_node(&child_node, child_data);
        btree_mark_page_dirty(tree, new_node.children[i]);
        cache_unpin(tree->cache, new_node.children[i]);
    }

    /* Middle key goes to parent */
    *split_key_out = old_node.keys[split_index];

    /* Update old node */
    old_node.num_keys = split_index;

    /* Both nodes have same parent */
    new_node.parent = old_node.parent;

    /* Write both nodes back */
    serialize_node(&old_node, old_data);
    btree_mark_page_dirty(tree, internal_page);
    cache_unpin(tree->cache, internal_page);

    serialize_node(&new_node, new_data);
    btree_mark_page_dirty(tree, new_page);
    cache_unpin(tree->cache, new_page);

    *new_page_out = new_page;
    return 0;
}

/*
 * Borrow a key from a sibling (Phase 3B)
 * Called when a node has too few keys but sibling has extra
 */
static int borrow_from_sibling(struct btree *tree, uint32_t page_num, uint32_t parent_page, int child_index) {
    struct btree_node node, parent, sibling;
    uint8_t *node_data, *parent_data, *sibling_data;
    uint32_t sibling_page;
    int i;

    /* Get parent */
    if (cache_get_page(tree->cache, parent_page, &parent_data) != 0) {
        return -1;
    }
    deserialize_node(&parent, parent_data);

    /* Try to borrow from right sibling first */
    if (child_index < (int)parent.num_keys) {
        sibling_page = parent.children[child_index + 1];

        if (cache_get_page(tree->cache, sibling_page, &sibling_data) != 0) {
            cache_unpin(tree->cache, parent_page);
            return -1;
        }
        deserialize_node(&sibling, sibling_data);

        /* Can borrow if sibling has more than minimum */
        if (sibling.num_keys > BTREE_MIN_KEYS) {
            /* Get current node */
            if (cache_get_page(tree->cache, page_num, &node_data) != 0) {
                cache_unpin(tree->cache, sibling_page);
                cache_unpin(tree->cache, parent_page);
                return -1;
            }
            deserialize_node(&node, node_data);

            /* Borrow first key from right sibling */
            if (node.node_type == BTREE_NODE_LEAF) {
                /* Leaf: copy key/value */
                node.keys[node.num_keys] = sibling.keys[0];
                node.values[node.num_keys] = sibling.values[0];
                node.num_keys++;

                /* Remove from sibling */
                for (i = 0; i < (int)sibling.num_keys - 1; i++) {
                    sibling.keys[i] = sibling.keys[i + 1];
                    sibling.values[i] = sibling.values[i + 1];
                }
                sibling.num_keys--;

                /* Update parent separator */
                parent.keys[child_index] = sibling.keys[0];
            } else {
                /* Internal: borrow child pointer too */
                node.keys[node.num_keys] = parent.keys[child_index];
                node.children[node.num_keys + 1] = sibling.children[0];
                node.num_keys++;

                parent.keys[child_index] = sibling.keys[0];

                /* Remove from sibling */
                for (i = 0; i < (int)sibling.num_keys - 1; i++) {
                    sibling.keys[i] = sibling.keys[i + 1];
                    sibling.children[i] = sibling.children[i + 1];
                }
                sibling.children[sibling.num_keys - 1] = sibling.children[sibling.num_keys];
                sibling.num_keys--;
            }

            /* Write all nodes back */
            serialize_node(&node, node_data);
            btree_mark_page_dirty(tree, page_num);
            cache_unpin(tree->cache, page_num);

            serialize_node(&sibling, sibling_data);
            btree_mark_page_dirty(tree, sibling_page);
            cache_unpin(tree->cache, sibling_page);

            serialize_node(&parent, parent_data);
            btree_mark_page_dirty(tree, parent_page);
            cache_unpin(tree->cache, parent_page);

            return 0;  /* Successfully borrowed */
        }

        cache_unpin(tree->cache, sibling_page);
    }

    /* Try left sibling */
    if (child_index > 0) {
        sibling_page = parent.children[child_index - 1];

        if (cache_get_page(tree->cache, sibling_page, &sibling_data) != 0) {
            cache_unpin(tree->cache, parent_page);
            return -1;
        }
        deserialize_node(&sibling, sibling_data);

        /* Can borrow if sibling has more than minimum */
        if (sibling.num_keys > BTREE_MIN_KEYS) {
            /* Get current node */
            if (cache_get_page(tree->cache, page_num, &node_data) != 0) {
                cache_unpin(tree->cache, sibling_page);
                cache_unpin(tree->cache, parent_page);
                return -1;
            }
            deserialize_node(&node, node_data);

            /* Borrow last key from left sibling */
            if (node.node_type == BTREE_NODE_LEAF) {
                /* Shift current node right */
                for (i = (int)node.num_keys; i > 0; i--) {
                    node.keys[i] = node.keys[i - 1];
                    node.values[i] = node.values[i - 1];
                }

                /* Copy from sibling */
                node.keys[0] = sibling.keys[sibling.num_keys - 1];
                node.values[0] = sibling.values[sibling.num_keys - 1];
                node.num_keys++;
                sibling.num_keys--;

                /* Update parent separator */
                parent.keys[child_index - 1] = node.keys[0];
            } else {
                /* Internal node */
                for (i = (int)node.num_keys; i > 0; i--) {
                    node.keys[i] = node.keys[i - 1];
                    node.children[i + 1] = node.children[i];
                }
                node.children[1] = node.children[0];

                node.keys[0] = parent.keys[child_index - 1];
                node.children[0] = sibling.children[sibling.num_keys];
                node.num_keys++;

                parent.keys[child_index - 1] = sibling.keys[sibling.num_keys - 1];
                sibling.num_keys--;
            }

            /* Write all nodes back */
            serialize_node(&node, node_data);
            btree_mark_page_dirty(tree, page_num);
            cache_unpin(tree->cache, page_num);

            serialize_node(&sibling, sibling_data);
            btree_mark_page_dirty(tree, sibling_page);
            cache_unpin(tree->cache, sibling_page);

            serialize_node(&parent, parent_data);
            btree_mark_page_dirty(tree, parent_page);
            cache_unpin(tree->cache, parent_page);

            return 0;  /* Successfully borrowed */
        }

        cache_unpin(tree->cache, sibling_page);
    }

    cache_unpin(tree->cache, parent_page);
    return -1;  /* Cannot borrow */
}

/*
 * Merge with a sibling (Phase 3B)
 * Called when both node and sibling have minimum keys
 */
static int merge_with_sibling(struct btree *tree, uint32_t left_page, uint32_t right_page, uint32_t parent_page, int separator_index) {
    struct btree_node left, right, parent;
    uint8_t *left_data, *right_data, *parent_data;
    uint32_t i;

    /* Get all three nodes */
    if (cache_get_page(tree->cache, left_page, &left_data) != 0) {
        return -1;
    }
    deserialize_node(&left, left_data);

    if (cache_get_page(tree->cache, right_page, &right_data) != 0) {
        cache_unpin(tree->cache, left_page);
        return -1;
    }
    deserialize_node(&right, right_data);

    if (cache_get_page(tree->cache, parent_page, &parent_data) != 0) {
        cache_unpin(tree->cache, left_page);
        cache_unpin(tree->cache, right_page);
        return -1;
    }
    deserialize_node(&parent, parent_data);

    /* Merge right into left */
    if (left.node_type == BTREE_NODE_LEAF) {
        /* Leaf nodes: just copy keys/values */
        for (i = 0; i < right.num_keys; i++) {
            left.keys[left.num_keys] = right.keys[i];
            left.values[left.num_keys] = right.values[i];
            left.num_keys++;
        }

        /* Update leaf chain */
        left.next_leaf = right.next_leaf;
    } else {
        /* Internal nodes: include separator from parent */
        left.keys[left.num_keys] = parent.keys[separator_index];
        left.children[left.num_keys + 1] = right.children[0];
        left.num_keys++;

        /* Copy remaining keys from right */
        for (i = 0; i < right.num_keys; i++) {
            left.keys[left.num_keys] = right.keys[i];
            left.children[left.num_keys + 1] = right.children[i + 1];
            left.num_keys++;
        }
    }

    /* Remove separator from parent */
    for (i = separator_index; i < parent.num_keys - 1; i++) {
        parent.keys[i] = parent.keys[i + 1];
        parent.children[i + 1] = parent.children[i + 2];
    }
    parent.num_keys--;

    /* Write updated nodes */
    serialize_node(&left, left_data);
    btree_mark_page_dirty(tree, left_page);
    cache_unpin(tree->cache, left_page);

    cache_unpin(tree->cache, right_page);
    pager_free_page(tree->pager, right_page);  /* Free the right node */

    serialize_node(&parent, parent_data);
    btree_mark_page_dirty(tree, parent_page);
    cache_unpin(tree->cache, parent_page);

    return 0;
}

/*
 * Rebalance tree after deletion (Phase 3B)
 */
static int rebalance_after_delete(struct btree *tree, uint32_t page_num) {
    struct btree_node node, parent;
    uint8_t *node_data, *parent_data;
    uint32_t parent_page;
    int child_index, i;

    /* Get the node */
    if (cache_get_page(tree->cache, page_num, &node_data) != 0) {
        return -1;
    }
    deserialize_node(&node, node_data);

    /* If node has enough keys, no rebalancing needed */
    if (node.num_keys >= BTREE_MIN_KEYS) {
        cache_unpin(tree->cache, page_num);
        return 0;
    }

    /* If this is root and has at least 1 key, it's okay */
    parent_page = node.parent;
    if (parent_page == 0) {
        /* Root node - special case */
        if (node.num_keys == 0 && node.node_type == BTREE_NODE_INTERNAL) {
            /* Root is empty internal node - make its only child the new root */
            tree->root_page = node.children[0];

            /* Update new root's parent pointer */
            cache_unpin(tree->cache, page_num);
            if (cache_get_page(tree->cache, tree->root_page, &node_data) != 0) {
                return -1;
            }
            deserialize_node(&node, node_data);
            node.parent = 0;
            serialize_node(&node, node_data);
            btree_mark_page_dirty(tree, tree->root_page);
            cache_unpin(tree->cache, tree->root_page);

            pager_free_page(tree->pager, page_num);
        } else {
            cache_unpin(tree->cache, page_num);
        }
        return 0;
    }

    cache_unpin(tree->cache, page_num);

    /* Find child index in parent */
    if (cache_get_page(tree->cache, parent_page, &parent_data) != 0) {
        return -1;
    }
    deserialize_node(&parent, parent_data);

    child_index = -1;
    for (i = 0; i <= (int)parent.num_keys; i++) {
        if (parent.children[i] == page_num) {
            child_index = i;
            break;
        }
    }
    cache_unpin(tree->cache, parent_page);

    if (child_index < 0) {
        return -1;  /* Not found in parent */
    }

    /* Try to borrow from sibling */
    if (borrow_from_sibling(tree, page_num, parent_page, child_index) == 0) {
        return 0;  /* Successfully borrowed */
    }

    /* Cannot borrow - must merge */
    if (child_index > 0) {
        /* Merge with left sibling */
        uint32_t left_page = parent.children[child_index - 1];
        if (merge_with_sibling(tree, left_page, page_num, parent_page, child_index - 1) == 0) {
            /* Recursively rebalance parent */
            return rebalance_after_delete(tree, parent_page);
        }
    } else {
        /* Merge with right sibling */
        uint32_t right_page = parent.children[child_index + 1];
        if (merge_with_sibling(tree, page_num, right_page, parent_page, child_index) == 0) {
            /* Recursively rebalance parent */
            return rebalance_after_delete(tree, parent_page);
        }
    }

    return 0;
}

/*
 * Get tree statistics (Phase 3B: actual tree traversal)
 */
void btree_get_stats(struct btree *tree, uint32_t *num_entries, uint32_t *height, uint32_t *num_nodes) {
    struct btree_node node;
    uint8_t *page_data;
    uint32_t current_page;
    uint32_t tree_height = 0;
    uint32_t node_count = 0;
    uint32_t leaf_count = 0;
    uint32_t next_leaf;

    if (!tree) {
        if (num_entries) *num_entries = 0;
        if (height) *height = 0;
        if (num_nodes) *num_nodes = 0;
        return;
    }

    /* Return number of entries (already tracked) */
    if (num_entries) {
        *num_entries = tree->num_entries;
    }

    /* Calculate height by traversing from root to leftmost leaf */
    if (height) {
        current_page = tree->root_page;
        tree_height = 0;

        while (1) {
            if (cache_get_page(tree->cache, current_page, &page_data) != 0) {
                break;
            }

            deserialize_node(&node, page_data);
            cache_unpin(tree->cache, current_page);

            tree_height++;

            if (node.node_type == BTREE_NODE_LEAF) {
                break;  /* Reached leaf level */
            }

            /* Follow leftmost child */
            current_page = node.children[0];
            if (current_page == 0) {
                break;
            }
        }

        *height = tree_height;
    }

    /* Count nodes by traversing all leaf nodes and walking up */
    if (num_nodes) {
        /* Count leaf nodes by following the leaf chain */
        leaf_count = 0;
        current_page = tree->root_page;

        /* First, find leftmost leaf */
        while (1) {
            if (cache_get_page(tree->cache, current_page, &page_data) != 0) {
                break;
            }

            deserialize_node(&node, page_data);
            cache_unpin(tree->cache, current_page);

            if (node.node_type == BTREE_NODE_LEAF) {
                break;
            }

            current_page = node.children[0];
            if (current_page == 0) {
                break;
            }
        }

        /* Now traverse all leaves */
        next_leaf = current_page;
        while (next_leaf != 0) {
            leaf_count++;

            if (cache_get_page(tree->cache, next_leaf, &page_data) != 0) {
                break;
            }

            deserialize_node(&node, page_data);
            current_page = next_leaf;  /* Save current page for unpinning */
            next_leaf = node.next_leaf;
            cache_unpin(tree->cache, current_page);
        }

        /* For simplicity, estimate total nodes based on height and leaf count */
        /* This is an approximation: leaf_count + internal nodes */
        /* For a more accurate count, we'd need to traverse internal nodes too */
        if (tree_height == 1) {
            node_count = leaf_count;  /* Only leaves */
        } else {
            /* Rough estimate: leaves + ~(leaves / branching factor) internal nodes */
            node_count = leaf_count + (leaf_count / 32);  /* Approximation */
        }

        *num_nodes = node_count;
    }
}
