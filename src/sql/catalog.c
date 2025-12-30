/*
 * catalog.c - Catalog implementation
 */

#include "sql/catalog.h"
#include "storage/btree.h"
#include "util/crc32.h"
#include "os/mem.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* Module-level buffers to avoid stack overflow (4KB limit) */
static struct table_schema g_catalog_schema_buffer;
static uint8_t g_catalog_page_buffer[AMIDB_PAGE_SIZE];

/* Debug logging */
static FILE *g_catalog_debug_log = NULL;

#define CATALOG_LOG(...) do { \
    if (!g_catalog_debug_log) g_catalog_debug_log = fopen("RAM:catalog_debug.txt", "w"); \
    if (g_catalog_debug_log) { fprintf(g_catalog_debug_log, __VA_ARGS__); fflush(g_catalog_debug_log); } \
} while(0)

/* Forward declarations */
static int serialize_schema(const struct table_schema *schema, uint8_t *buffer, uint32_t *size);
static int deserialize_schema(const uint8_t *buffer, struct table_schema *schema);

/*
 * Hash table name to int32_t key
 */
int32_t catalog_hash_name(const char *table_name) {
    uint32_t hash = crc32_compute((const uint8_t *)table_name, strlen(table_name));
    /* Convert to signed int32_t, ensure positive */
    return (int32_t)(hash & 0x7FFFFFFF);
}

/*
 * Initialize catalog
 */
int catalog_init(struct catalog *cat, struct amidb_pager *pager, struct page_cache *cache) {
    uint32_t catalog_root;

    cat->pager = pager;
    cat->cache = cache;
    cat->catalog_tree = NULL;

    /* Get catalog root from file header */
    catalog_root = pager_get_catalog_root(pager);

    if (catalog_root == 0) {
        /* New database - create catalog B+Tree */
        cat->catalog_tree = btree_create(pager, cache, &catalog_root);
        if (!cat->catalog_tree) {
            return -1;
        }

        /* Save catalog root to file header */
        pager_set_catalog_root(pager, catalog_root);
        cat->catalog_root = catalog_root;
    } else {
        /* Existing database - open catalog B+Tree */
        cat->catalog_tree = btree_open(pager, cache, catalog_root);
        if (!cat->catalog_tree) {
            return -1;
        }

        cat->catalog_root = catalog_root;
    }

    return 0;
}

/*
 * Close catalog
 */
void catalog_close(struct catalog *cat) {
    if (cat->catalog_tree) {
        btree_close(cat->catalog_tree);
        cat->catalog_tree = NULL;
    }
}

/*
 * Create table in catalog
 */
int catalog_create_table(struct catalog *cat, const struct sql_create_table *create_stmt) {
    struct table_schema *schema = &g_catalog_schema_buffer;
    struct btree *table_tree;
    uint8_t *schema_buffer = g_catalog_page_buffer;
    uint32_t schema_size;
    uint32_t schema_page;
    int32_t hash_key;
    uint32_t existing_page;
    int rc;
    int i;

    CATALOG_LOG("[CATALOG] Creating table '%s'\n", create_stmt->table_name); 

    /* Check if table already exists */
    hash_key = catalog_hash_name(create_stmt->table_name);
    rc = btree_search(cat->catalog_tree, hash_key, &existing_page);
    if (rc == 0) {
        CATALOG_LOG("[CATALOG] Table already exists\n"); 
        /* Table already exists */
        return -1;
    }
    CATALOG_LOG("[CATALOG] Table doesn't exist, proceeding...\n"); 

    /* Build table schema */
    memset(schema, 0, sizeof(*schema));
    strncpy(schema->name, create_stmt->table_name, sizeof(schema->name) - 1);
    schema->column_count = create_stmt->column_count;
    CATALOG_LOG("[CATALOG] Built schema: name='%s', column_count=%u\n",
                schema->name, schema->column_count);

    /* Copy column definitions */
    for (i = 0; i < create_stmt->column_count; i++) {
        schema->columns[i] = create_stmt->columns[i];
    }

    /* Find PRIMARY KEY index */
    schema->primary_key_index = -1;  /* Default: implicit rowid */
    for (i = 0; i < schema->column_count; i++) {
        if (schema->columns[i].is_primary_key) {
            schema->primary_key_index = i;
            break;
        }
    }

    /* Create B+Tree for table data */
    CATALOG_LOG("[CATALOG] Creating table B+Tree...\n"); 
    table_tree = btree_create(cat->pager, cat->cache, &schema->btree_root);
    if (!table_tree) {
        CATALOG_LOG("[CATALOG] ERROR: btree_create failed\n"); 
        return -1;
    }
    CATALOG_LOG("[CATALOG] B+Tree created, root=%u\n", schema->btree_root); 

    schema->next_rowid = 1;  /* Start auto-increment at 1 */
    schema->row_count = 0;

    btree_close(table_tree);

    /* Serialize schema */
    CATALOG_LOG("[CATALOG] Serializing schema...\n"); 
    if (serialize_schema(schema, schema_buffer, &schema_size) != 0) {
        CATALOG_LOG("[CATALOG] ERROR: serialize_schema failed\n"); 
        return -1;
    }

    /* Allocate page for schema */
    CATALOG_LOG("[CATALOG] Allocating schema page...\n"); 
    rc = pager_allocate_page(cat->pager, &schema_page);
    if (rc != 0) {
        CATALOG_LOG("[CATALOG] ERROR: pager_allocate_page failed\n"); 
        return -1;
    }
    CATALOG_LOG("[CATALOG] Schema page=%u\n", schema_page); 

    /* Write schema to page */
    CATALOG_LOG("[CATALOG] Writing schema to page %u...\n", schema_page);
    CATALOG_LOG("[CATALOG] Buffer BEFORE write, first 16 bytes: ");
    for (i = 0; i < 16; i++) {
        CATALOG_LOG("%02x ", schema_buffer[i]);
    }
    CATALOG_LOG("\n");
    rc = pager_write_page(cat->pager, schema_page, schema_buffer);
    if (rc != 0) {
        CATALOG_LOG("[CATALOG] ERROR: pager_write_page failed\n");
        return -1;
    }
    CATALOG_LOG("[CATALOG] pager_write_page completed\n");

    /* Insert into catalog B+Tree */
    CATALOG_LOG("[CATALOG] Inserting into catalog B+Tree...\n");
    CATALOG_LOG("[CATALOG] btree_insert(hash_key=%d, schema_page=%u)\n", hash_key, schema_page);
    rc = btree_insert(cat->catalog_tree, hash_key, schema_page);
    if (rc != 0) {
        CATALOG_LOG("[CATALOG] ERROR: btree_insert failed, rc=%d\n", rc);
        return -1;
    }

    CATALOG_LOG("[CATALOG] SUCCESS: Table '%s' created (hash=%d -> page=%u)\n",
                create_stmt->table_name, hash_key, schema_page); 
    return 0;
}

/*
 * Get table schema
 */
int catalog_get_table(struct catalog *cat, const char *table_name, struct table_schema *schema) {
    int32_t hash_key;
    uint32_t schema_page;
    uint8_t *schema_buffer = g_catalog_page_buffer;  /* Use global buffer */
    int rc;

    hash_key = catalog_hash_name(table_name);
    CATALOG_LOG("[GET_TABLE] Looking up table='%s', hash_key=%d\n", table_name, hash_key);

    /* Search catalog B+Tree */
    rc = btree_search(cat->catalog_tree, hash_key, &schema_page);
    if (rc != 0) {
        CATALOG_LOG("[GET_TABLE] Table not found in B+Tree\n");
        return -1;  /* Table not found */
    }

    CATALOG_LOG("[GET_TABLE] Found schema_page=%u\n", schema_page);

    /* Read schema page */
    rc = pager_read_page(cat->pager, schema_page, schema_buffer);
    if (rc != 0) {
        CATALOG_LOG("[GET_TABLE] Failed to read page %u\n", schema_page);
        return -1;
    }

    CATALOG_LOG("[GET_TABLE] Successfully read page %u\n", schema_page);
    CATALOG_LOG("[GET_TABLE] Buffer AFTER read, first 16 bytes: ");
    {
        uint32_t i;
        for (i = 0; i < 16; i++) {
            CATALOG_LOG("%02x ", schema_buffer[i]);
        }
        CATALOG_LOG("\n");
    }

    /* Deserialize schema */
    if (deserialize_schema(schema_buffer, schema) != 0) {
        return -1;
    }

    return 0;
}

/*
 * Drop table
 */
int catalog_drop_table(struct catalog *cat, const char *table_name) {
    int32_t hash_key;
    uint32_t schema_page;
    int rc;

    hash_key = catalog_hash_name(table_name);

    /* Check if table exists */
    rc = btree_search(cat->catalog_tree, hash_key, &schema_page);
    if (rc != 0) {
        return -1;  /* Table not found */
    }

    /* Delete from catalog B+Tree */
    rc = btree_delete(cat->catalog_tree, hash_key);
    if (rc != 0) {
        return -1;
    }

    /* TODO: Free table's B+Tree pages and schema page */
    /* For now, just remove from catalog (pages become orphaned) */

    return 0;
}

/*
 * Update table schema
 */
int catalog_update_table(struct catalog *cat, const struct table_schema *schema) {
    int32_t hash_key;
    uint32_t schema_page;
    uint8_t *schema_buffer = g_catalog_page_buffer;  /* Use global buffer */
    uint32_t schema_size;
    int rc;

    hash_key = catalog_hash_name(schema->name);

    /* Get schema page number */
    rc = btree_search(cat->catalog_tree, hash_key, &schema_page);
    if (rc != 0) {
        return -1;  /* Table not found */
    }

    /* Serialize schema */
    if (serialize_schema(schema, schema_buffer, &schema_size) != 0) {
        return -1;
    }

    /* Write updated schema to page */
    rc = pager_write_page(cat->pager, schema_page, schema_buffer);
    if (rc != 0) {
        return -1;
    }

    return 0;
}

/*
 * List all tables
 */
int catalog_list_tables(struct catalog *cat, char table_names[][64], int max_tables) {
    struct btree_cursor cursor;
    struct table_schema *schema = &g_catalog_schema_buffer;  /* Use global buffer */
    uint8_t *schema_buffer = g_catalog_page_buffer;  /* Use global buffer */
    int32_t key;
    uint32_t value;
    int count = 0;
    int rc;

    /* Initialize cursor at beginning of catalog B+Tree */
    rc = btree_cursor_first(cat->catalog_tree, &cursor);
    if (rc != 0) {
        return 0;  /* Empty catalog */
    }

    /* Iterate over all entries */
    while (count < max_tables && btree_cursor_valid(&cursor)) {
        rc = btree_cursor_get(&cursor, &key, &value);
        if (rc != 0) {
            break;  /* End of tree */
        }

        /* Read schema page */
        rc = pager_read_page(cat->pager, value, schema_buffer);
        if (rc == 0) {
            if (deserialize_schema(schema_buffer, schema) == 0) {
                strncpy(table_names[count], schema->name, 63);
                table_names[count][63] = '\0';
                count++;
            }
        }

        /* Move to next entry */
        rc = btree_cursor_next(&cursor);
        if (rc != 0) {
            break;
        }
    }

    return count;
}

/* ========== Serialization ========== */

/*
 * Serialize table schema to buffer
 */
static int serialize_schema(const struct table_schema *schema, uint8_t *buffer, uint32_t *size) {
    uint32_t offset = 12;  /* Start after 12-byte page header */
    uint32_t i;

    CATALOG_LOG("[SERIALIZE] Serializing schema: name='%s'\n", schema->name);

    /* Clear buffer */
    memset(buffer, 0, AMIDB_PAGE_SIZE);

    /* Table name (64 bytes) */
    memcpy(buffer + offset, schema->name, 64);
    CATALOG_LOG("[SERIALIZE] Copied name to buffer, first 16 bytes (hex): ");
    for (i = 0; i < 16; i++) {
        CATALOG_LOG("%02x ", buffer[i]);
    }
    CATALOG_LOG("...\n");
    offset += 64;

    /* Column count (4 bytes) */
    memcpy(buffer + offset, &schema->column_count, 4);
    offset += 4;

    /* Columns (32 * 68 bytes = 2176 bytes) */
    for (i = 0; i < 32; i++) {
        /* Column name (64 bytes) */
        memcpy(buffer + offset, schema->columns[i].name, 64);
        offset += 64;

        /* Column type (1 byte) */
        buffer[offset++] = schema->columns[i].type;

        /* Column flags (2 bytes) */
        buffer[offset++] = schema->columns[i].is_primary_key;
        buffer[offset++] = schema->columns[i].not_null;

        /* Padding (1 byte for alignment) */
        buffer[offset++] = 0;
    }

    /* Primary key index (4 bytes) */
    memcpy(buffer + offset, &schema->primary_key_index, 4);
    offset += 4;

    /* B+Tree root (4 bytes) */
    memcpy(buffer + offset, &schema->btree_root, 4);
    offset += 4;

    /* Next rowid (4 bytes) */
    memcpy(buffer + offset, &schema->next_rowid, 4);
    offset += 4;

    /* Row count (4 bytes) */
    memcpy(buffer + offset, &schema->row_count, 4);
    offset += 4;

    *size = offset;
    return 0;
}

/*
 * Deserialize table schema from buffer
 */
static int deserialize_schema(const uint8_t *buffer, struct table_schema *schema) {
    uint32_t offset = 12;  /* Start after 12-byte page header */
    uint32_t i;

    CATALOG_LOG("[DESERIALIZE] Starting deserialization...\n");
    CATALOG_LOG("[DESERIALIZE] First 16 bytes of DATA (offset 12+): ");
    for (i = 12; i < 28; i++) {
        CATALOG_LOG("%02x ", buffer[i]);
    }
    CATALOG_LOG("...\n");

    memset(schema, 0, sizeof(struct table_schema));

    /* Table name (64 bytes) */
    memcpy(schema->name, buffer + offset, 64);
    schema->name[63] = '\0';
    CATALOG_LOG("[DESERIALIZE] Table name from buffer: '%s'\n", schema->name);
    offset += 64;

    /* Column count (4 bytes) */
    memcpy(&schema->column_count, buffer + offset, 4);
    offset += 4;

    /* Columns (32 * 68 bytes) */
    for (i = 0; i < 32; i++) {
        /* Column name (64 bytes) */
        memcpy(schema->columns[i].name, buffer + offset, 64);
        schema->columns[i].name[63] = '\0';
        offset += 64;

        /* Column type (1 byte) */
        schema->columns[i].type = buffer[offset++];

        /* Column flags (2 bytes) */
        schema->columns[i].is_primary_key = buffer[offset++];
        schema->columns[i].not_null = buffer[offset++];

        /* Padding (1 byte) */
        offset++;
    }

    /* Primary key index (4 bytes) */
    memcpy(&schema->primary_key_index, buffer + offset, 4);
    offset += 4;

    /* B+Tree root (4 bytes) */
    memcpy(&schema->btree_root, buffer + offset, 4);
    offset += 4;

    /* Next rowid (4 bytes) */
    memcpy(&schema->next_rowid, buffer + offset, 4);
    offset += 4;

    /* Row count (4 bytes) */
    memcpy(&schema->row_count, buffer + offset, 4);
    offset += 4;

    return 0;
}
