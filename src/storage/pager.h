/*
 * pager.h - Page-based file I/O for AmiDB
 *
 * Manages fixed-size pages (4096 bytes) with CRC32 checksums.
 * Handles page allocation using a bitmap in the file header.
 */

#ifndef AMIDB_PAGER_H
#define AMIDB_PAGER_H

#include <stdint.h>

/* Page size: 4096 bytes (standard for most systems) */
#define AMIDB_PAGE_SIZE 4096

/* File format magic number: "AmiD" in ASCII */
#define AMIDB_MAGIC 0x416D6944

/* File format version */
#define AMIDB_VERSION 1

/* Maximum number of pages (limited by bitmap size) */
#define AMIDB_MAX_PAGES 4096   /* 16 MB max file size (512 byte bitmap - Amiga-friendly) */

/* Page types */
#define PAGE_TYPE_FREE      0
#define PAGE_TYPE_HEADER    1
#define PAGE_TYPE_BTREE     2
#define PAGE_TYPE_OVERFLOW  3
#define PAGE_TYPE_FREELIST  4
#define PAGE_TYPE_WAL       5

/* Database flags */
#define DB_FLAG_DIRTY       0x0001  /* Unclean shutdown, needs recovery */

/* File header structure (stored in page 0) */
struct amidb_file_header {
    uint32_t magic;              /* Magic number: 0x416D6944 */
    uint32_t version;            /* File format version */
    uint32_t page_size;          /* Page size (always 4096) */
    uint32_t page_count;         /* Total pages allocated */
    uint32_t first_free_page;    /* First page in free list (0 = none) */
    uint32_t root_page;          /* Root page of main B+tree */
    uint32_t wal_offset;         /* Offset to WAL region */
    uint32_t flags;              /* Database flags (DB_FLAG_*) */
    uint32_t wal_head;           /* Current WAL write position */
    uint32_t wal_tail;           /* Oldest unprocessed WAL entry */
    uint32_t catalog_root;       /* Root page of catalog B+Tree (Phase 4) */
    uint32_t reserved[5];        /* Reserved for future use */
    /* Followed by page allocation bitmap */
};

/* Page header structure (at start of each page) */
struct amidb_page_header {
    uint32_t page_num;           /* Page number (for verification) */
    uint8_t  page_type;          /* Page type (see PAGE_TYPE_*) */
    uint8_t  reserved[3];        /* Alignment padding */
    uint32_t checksum;           /* CRC32 of page data (excluding this header) */
};

/* Forward declarations for WAL and transaction support */
struct wal_context;
struct txn_context;

/* Pager handle */
struct amidb_pager {
    void *file_handle;           /* OS file handle */
    char *file_path;             /* Database file path */
    struct amidb_file_header header;
    uint8_t *bitmap;             /* Page allocation bitmap */
    uint32_t bitmap_size;        /* Size of bitmap in bytes */
    int read_only;               /* Read-only mode flag */

    /* Phase 3C: WAL and transaction support */
    struct wal_context *wal;     /* Write-ahead log (NULL if disabled) */
    struct txn_context *txn;     /* Active transaction (NULL if none) */
};

/* Open/close pager */
int pager_open(const char *path, int read_only, struct amidb_pager **pager_out);
void pager_close(struct amidb_pager *pager);

/* Page allocation */
int pager_allocate_page(struct amidb_pager *pager, uint32_t *page_num_out);
int pager_free_page(struct amidb_pager *pager, uint32_t page_num);

/* Page I/O */
int pager_read_page(struct amidb_pager *pager, uint32_t page_num, uint8_t *page_data);
int pager_write_page(struct amidb_pager *pager, uint32_t page_num, const uint8_t *page_data);

/* Sync to disk */
int pager_sync(struct amidb_pager *pager);

/* Get page count */
uint32_t pager_get_page_count(struct amidb_pager *pager);

/* Write file header (Phase 3C: for persisting WAL state) */
int pager_write_header(struct amidb_pager *pager);

/* Catalog root accessors (Phase 4) */
uint32_t pager_get_catalog_root(struct amidb_pager *pager);
void pager_set_catalog_root(struct amidb_pager *pager, uint32_t catalog_root);

#endif /* AMIDB_PAGER_H */
