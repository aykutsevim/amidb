/*
 * catalog.h - Database catalog (schema storage)
 *
 * The catalog stores table schemas persistently using a B+Tree.
 * - Catalog B+Tree: hash32(table_name) → schema_page_number
 * - Schema pages contain serialized table_schema structures
 * - Root page number stored in file header (pager->header.catalog_root)
 */

#ifndef AMIDB_SQL_CATALOG_H
#define AMIDB_SQL_CATALOG_H

#include "sql/parser.h"
#include "storage/pager.h"
#include "storage/cache.h"
#include "storage/btree.h"
#include <stdint.h>

/* Table schema (persistent metadata) */
struct table_schema {
    char name[64];              /* Table name */
    uint32_t column_count;      /* Number of columns */
    struct sql_column_def columns[32];  /* Column definitions */
    int8_t primary_key_index;   /* Index of PRIMARY KEY column (-1 if implicit rowid) */
    uint32_t btree_root;        /* Root page of table's data B+Tree */
    uint32_t next_rowid;        /* Next auto-increment rowid (for implicit rowid tables) */
    uint32_t row_count;         /* Approximate row count (for stats) */
};

/* Catalog manager */
struct catalog {
    struct amidb_pager *pager;
    struct page_cache *cache;
    struct btree *catalog_tree;     /* B+Tree: hash(table_name) → schema_page */
    uint32_t catalog_root;          /* Root page of catalog B+Tree */
};

/* Catalog API */

/*
 * Initialize catalog system
 * Creates or opens the catalog B+Tree
 * Returns 0 on success, -1 on error
 */
int catalog_init(struct catalog *cat, struct amidb_pager *pager, struct page_cache *cache);

/*
 * Close catalog
 */
void catalog_close(struct catalog *cat);

/*
 * Create a new table in the catalog
 * Allocates a B+Tree for the table's data
 * Returns 0 on success, -1 on error (e.g., duplicate name)
 */
int catalog_create_table(struct catalog *cat, const struct sql_create_table *create_stmt);

/*
 * Get table schema by name
 * Returns 0 on success, -1 if table not found
 */
int catalog_get_table(struct catalog *cat, const char *table_name, struct table_schema *schema);

/*
 * Drop table from catalog
 * Removes schema and frees the table's B+Tree
 * Returns 0 on success, -1 if table not found
 */
int catalog_drop_table(struct catalog *cat, const char *table_name);

/*
 * Update table schema (e.g., increment next_rowid, update row_count)
 * Returns 0 on success, -1 on error
 */
int catalog_update_table(struct catalog *cat, const struct table_schema *schema);

/*
 * List all table names
 * Fills table_names array with up to max_tables names
 * Returns number of tables found, -1 on error
 */
int catalog_list_tables(struct catalog *cat, char table_names[][64], int max_tables);

/*
 * Hash table name to int32_t key for B+Tree
 */
int32_t catalog_hash_name(const char *table_name);

#endif /* AMIDB_SQL_CATALOG_H */
