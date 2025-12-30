/*
 * pager.c - Page-based file I/O implementation
 */

#include "storage/pager.h"
#include "txn/wal.h"       /* Phase 3C: WAL support */
#include "os/file.h"
#include "os/mem.h"
#include "util/endian.h"
#include "util/crc32.h"
#include <string.h>
#include <stdio.h>

/* Helper: Set bit in bitmap */
static void bitmap_set(uint8_t *bitmap, uint32_t bit) {
    bitmap[bit / 8] |= (1 << (bit % 8));
}

/* Helper: Clear bit in bitmap */
static void bitmap_clear(uint8_t *bitmap, uint32_t bit) {
    bitmap[bit / 8] &= ~(1 << (bit % 8));
}

/* Helper: Test bit in bitmap */
static int bitmap_test(const uint8_t *bitmap, uint32_t bit) {
    return (bitmap[bit / 8] & (1 << (bit % 8))) != 0;
}

/* Helper: Initialize file header */
static void init_file_header(struct amidb_file_header *hdr) {
    uint32_t i;
    hdr->magic = AMIDB_MAGIC;
    hdr->version = AMIDB_VERSION;
    hdr->page_size = AMIDB_PAGE_SIZE;
    hdr->page_count = 1;  /* Just the header page initially */
    hdr->first_free_page = 0;
    hdr->root_page = 0;
    hdr->wal_offset = 0;
    hdr->flags = 0;          /* Phase 3C: DB flags */
    hdr->wal_head = 0;       /* Phase 3C: WAL position */
    hdr->wal_tail = 0;       /* Phase 3C: WAL tail */
    hdr->catalog_root = 0;   /* Phase 4: Catalog B+Tree */
    for (i = 0; i < 5; i++) {
        hdr->reserved[i] = 0;
    }
}

/* Helper: Serialize file header to bytes */
static void serialize_header(const struct amidb_file_header *hdr, uint8_t *buf) {
    put_u32(buf + 0, hdr->magic);
    put_u32(buf + 4, hdr->version);
    put_u32(buf + 8, hdr->page_size);
    put_u32(buf + 12, hdr->page_count);
    put_u32(buf + 16, hdr->first_free_page);
    put_u32(buf + 20, hdr->root_page);
    put_u32(buf + 24, hdr->wal_offset);
    put_u32(buf + 28, hdr->flags);        /* Phase 3C */
    put_u32(buf + 32, hdr->wal_head);     /* Phase 3C */
    put_u32(buf + 36, hdr->wal_tail);     /* Phase 3C */
    put_u32(buf + 40, hdr->catalog_root); /* Phase 4 */
    /* Reserved fields (5 × 4 = 20 bytes) */
    memset(buf + 44, 0, 20);
}

/* Helper: Deserialize file header from bytes */
static void deserialize_header(struct amidb_file_header *hdr, const uint8_t *buf) {
    uint32_t i;
    hdr->magic = get_u32(buf + 0);
    hdr->version = get_u32(buf + 4);
    hdr->page_size = get_u32(buf + 8);
    hdr->page_count = get_u32(buf + 12);
    hdr->first_free_page = get_u32(buf + 16);
    hdr->root_page = get_u32(buf + 20);
    hdr->wal_offset = get_u32(buf + 24);
    hdr->flags = get_u32(buf + 28);        /* Phase 3C */
    hdr->wal_head = get_u32(buf + 32);     /* Phase 3C */
    hdr->wal_tail = get_u32(buf + 36);     /* Phase 3C */
    hdr->catalog_root = get_u32(buf + 40); /* Phase 4 */
    for (i = 0; i < 5; i++) {
        hdr->reserved[i] = 0;
    }
}

/* Open pager */
int pager_open(const char *path, int read_only, struct amidb_pager **pager_out) {
    struct amidb_pager *pager;
    void *file_handle;
    uint8_t *page_buf;
    int is_new_file = 0;
    int rc;
    uint32_t i;
    FILE *debug_log;

    /* Open debug log */
    debug_log = fopen("pager_debug.log", "w");
    if (debug_log) {
        fprintf(debug_log, "[DEBUG] pager_open: path=%s, read_only=%d\n", path, read_only);
        fflush(debug_log);
    }

    printf("[DEBUG] pager_open: path=%s, read_only=%d\n", path, read_only);
    fflush(stdout);

    /* Open file - use CREATE flag for write mode to handle both new and existing files */
    if (read_only) {
        file_handle = file_open(path, AMIDB_O_RDONLY);
    } else {
        file_handle = file_open(path, AMIDB_O_RDWR | AMIDB_O_CREATE);
    }
    if (debug_log) { fprintf(debug_log, "file_open: %p\n", file_handle); fflush(debug_log); }

    /* Check if this is a new file by trying to read the header */
    if (file_handle && !read_only) {
        uint8_t test_buf[64];
        int32_t read_result = file_read(file_handle, test_buf, 64);
        if (read_result == 64) {
            uint32_t magic = get_u32(test_buf);
            if (magic == AMIDB_MAGIC) {
                /* Valid existing database */
                is_new_file = 0;
                if (debug_log) { fprintf(debug_log, "Found valid magic, existing file\n"); fflush(debug_log); }
                /* Seek back to start */
                file_seek(file_handle, 0, AMIDB_SEEK_SET);
            } else {
                /* Invalid/corrupt file, treat as new */
                is_new_file = 1;
                if (debug_log) { fprintf(debug_log, "Invalid magic (0x%08x), treating as new file\n", magic); fflush(debug_log); }
                /* Seek back to start to overwrite */
                file_seek(file_handle, 0, AMIDB_SEEK_SET);
            }
        } else {
            /* Empty or too small, treat as new file */
            is_new_file = 1;
            if (debug_log) { fprintf(debug_log, "Read returned %d, treating as new file\n", read_result); fflush(debug_log); }
            file_seek(file_handle, 0, AMIDB_SEEK_SET);
        }
    }

    if (!file_handle) {
        if (debug_log) { fprintf(debug_log, "FAIL: file_handle is NULL\n"); fclose(debug_log); }
        return -1;  /* Failed to open/create */
    }

    if (debug_log) { fprintf(debug_log, "Determined: is_new_file=%d\n", is_new_file); fflush(debug_log); }

    /* Allocate pager structure */
    printf("[DEBUG] Allocating pager struct (%u bytes)...\n", (unsigned)sizeof(struct amidb_pager));
    pager = (struct amidb_pager *)mem_alloc(sizeof(struct amidb_pager), AMIDB_MEM_CLEAR);
    printf("[DEBUG] pager=%p\n", pager);
    if (!pager) {
        printf("[DEBUG] Returning -1: pager allocation failed\n");
        file_close(file_handle);
        return -1;
    }

    pager->file_handle = file_handle;
    pager->read_only = read_only;

    /* Copy file path */
    printf("[DEBUG] Allocating file_path (%u bytes)...\n", (unsigned)(strlen(path) + 1));
    pager->file_path = (char *)mem_alloc(strlen(path) + 1, 0);
    printf("[DEBUG] file_path=%p\n", pager->file_path);
    if (!pager->file_path) {
        printf("[DEBUG] Returning -1: file_path allocation failed\n");
        mem_free(pager, sizeof(struct amidb_pager));
        file_close(file_handle);
        return -1;
    }
    strcpy(pager->file_path, path);

    /* Allocate page buffer */
    printf("[DEBUG] Allocating page_buf (%u bytes)...\n", AMIDB_PAGE_SIZE);
    page_buf = (uint8_t *)mem_alloc(AMIDB_PAGE_SIZE, AMIDB_MEM_CLEAR);
    printf("[DEBUG] page_buf=%p\n", page_buf);
    if (!page_buf) {
        printf("[DEBUG] Returning -1: page_buf allocation failed\n");
        mem_free(pager->file_path, strlen(path) + 1);
        mem_free(pager, sizeof(struct amidb_pager));
        file_close(file_handle);
        return -1;
    }

    if (is_new_file) {
        /* Initialize new database file */
        printf("[DEBUG] Entering new file branch\n");
        init_file_header(&pager->header);

        /* Allocate bitmap (8192 bytes = 65536 bits) */
        pager->bitmap_size = AMIDB_MAX_PAGES / 8;
        printf("[DEBUG] Allocating bitmap (%u bytes)...\n", pager->bitmap_size);
        pager->bitmap = (uint8_t *)mem_alloc(pager->bitmap_size, AMIDB_MEM_CLEAR);
        printf("[DEBUG] bitmap=%p\n", pager->bitmap);
        if (!pager->bitmap) {
            printf("[DEBUG] Returning -1: bitmap allocation failed\n");
            mem_free(page_buf, AMIDB_PAGE_SIZE);
            mem_free(pager->file_path, strlen(path) + 1);
            mem_free(pager, sizeof(struct amidb_pager));
            file_close(file_handle);
            return -1;
        }

        /* Mark page 0 (header) as allocated */
        bitmap_set(pager->bitmap, 0);

        /* Write header page */
        printf("[DEBUG] Writing header page...\n");
        serialize_header(&pager->header, page_buf);
        memcpy(page_buf + 64, pager->bitmap, pager->bitmap_size);

        rc = file_write(file_handle, page_buf, AMIDB_PAGE_SIZE);
        printf("[DEBUG] file_write returned %d (expected %u)\n", rc, AMIDB_PAGE_SIZE);
        if (rc != AMIDB_PAGE_SIZE) {
            printf("[DEBUG] Returning -1: file_write failed\n");
            mem_free(pager->bitmap, pager->bitmap_size);
            mem_free(page_buf, AMIDB_PAGE_SIZE);
            mem_free(pager->file_path, strlen(path) + 1);
            mem_free(pager, sizeof(struct amidb_pager));
            file_close(file_handle);
            return -1;
        }

        printf("[DEBUG] Calling file_sync...\n");
        file_sync(file_handle);
    } else {
        /* Read existing header */
        rc = file_read(file_handle, page_buf, AMIDB_PAGE_SIZE);
        if (rc != AMIDB_PAGE_SIZE) {
            mem_free(page_buf, AMIDB_PAGE_SIZE);
            mem_free(pager->file_path, strlen(path) + 1);
            mem_free(pager, sizeof(struct amidb_pager));
            file_close(file_handle);
            return -1;
        }

        deserialize_header(&pager->header, page_buf);

        /* Verify magic number */
        if (pager->header.magic != AMIDB_MAGIC) {
            mem_free(page_buf, AMIDB_PAGE_SIZE);
            mem_free(pager->file_path, strlen(path) + 1);
            mem_free(pager, sizeof(struct amidb_pager));
            file_close(file_handle);
            return -1;  /* Invalid database file */
        }

        /* Load bitmap */
        pager->bitmap_size = AMIDB_MAX_PAGES / 8;
        pager->bitmap = (uint8_t *)mem_alloc(pager->bitmap_size, 0);
        if (!pager->bitmap) {
            mem_free(page_buf, AMIDB_PAGE_SIZE);
            mem_free(pager->file_path, strlen(path) + 1);
            mem_free(pager, sizeof(struct amidb_pager));
            file_close(file_handle);
            return -1;
        }
        memcpy(pager->bitmap, page_buf + 64, pager->bitmap_size);
    }

    /* Phase 3C: Initialize WAL and transaction pointers */
    pager->wal = NULL;
    pager->txn = NULL;

    /* Phase 3C: Ensure file is extended to include WAL region */
    if (!read_only) {
        int32_t current_size = file_size(file_handle);
        int32_t required_size = 35 * AMIDB_PAGE_SIZE;  /* Header + pages 1-34 */

        if (current_size < required_size) {
            /* Extend file to include WAL region */
            memset(page_buf, 0, AMIDB_PAGE_SIZE);
            file_seek(file_handle, 0, AMIDB_SEEK_END);

            while (current_size < required_size) {
                rc = file_write(file_handle, page_buf, AMIDB_PAGE_SIZE);
                if (rc != AMIDB_PAGE_SIZE) {
                    break;
                }
                current_size += AMIDB_PAGE_SIZE;
            }
            file_sync(file_handle);
        }
    }

    /* Phase 3C: Check for dirty flag and perform recovery if needed */
    if (!is_new_file && !read_only && (pager->header.flags & DB_FLAG_DIRTY)) {
        /* Database was not cleanly shut down - perform recovery */
        struct wal_context *wal;
        int rc_recovery;

        wal = wal_create(pager);
        if (wal) {
            wal->wal_head = pager->header.wal_head;
            wal->wal_tail = pager->header.wal_tail;
            rc_recovery = wal_recover(wal);
            wal_destroy(wal);

            if (rc_recovery != 0) {
                /* Recovery failed */
                mem_free(page_buf, AMIDB_PAGE_SIZE);
                mem_free(pager->bitmap, pager->bitmap_size);
                mem_free(pager->file_path, strlen(path) + 1);
                mem_free(pager, sizeof(struct amidb_pager));
                file_close(file_handle);
                return -1;
            }

            /* Recovery succeeded - clear dirty flag */
            pager->header.flags &= ~DB_FLAG_DIRTY;
            pager->header.wal_head = 0;
            pager->header.wal_tail = 0;

            /* Write cleared header to disk */
            serialize_header(&pager->header, page_buf);
            memcpy(page_buf + 64, pager->bitmap, pager->bitmap_size);
            file_seek(file_handle, 0, AMIDB_SEEK_SET);
            file_write(file_handle, page_buf, AMIDB_PAGE_SIZE);
            file_sync(file_handle);
        }
    }

    /* Phase 3C: Set dirty flag for write mode (will be cleared on clean shutdown) */
    /* Only set for new databases - existing clean databases stay clean until modified */
    if (!read_only && is_new_file) {
        pager->header.flags |= DB_FLAG_DIRTY;
        /* Write header with dirty flag set */
        serialize_header(&pager->header, page_buf);
        memcpy(page_buf + 64, pager->bitmap, pager->bitmap_size);
        file_seek(file_handle, 0, AMIDB_SEEK_SET);
        file_write(file_handle, page_buf, AMIDB_PAGE_SIZE);
        file_sync(file_handle);
    }

    mem_free(page_buf, AMIDB_PAGE_SIZE);
    *pager_out = pager;
    if (debug_log) { fprintf(debug_log, "SUCCESS: returning 0\n"); fclose(debug_log); }
    return 0;
}

/* Close pager */
void pager_close(struct amidb_pager *pager) {
    uint32_t path_len;

    if (!pager) {
        return;
    }

    /* Phase 3C: Clear dirty flag on clean shutdown */
    /* Only do this if there's no uncommitted WAL data (for crash simulation tests) */
    if (pager->file_handle && !pager->read_only && pager->header.wal_head == 0) {
        pager->header.flags &= ~DB_FLAG_DIRTY;
        pager_write_header(pager);
        file_sync(pager->file_handle);
    }

    if (pager->file_handle) {
        file_close(pager->file_handle);
    }

    if (pager->bitmap) {
        mem_free(pager->bitmap, pager->bitmap_size);
    }

    if (pager->file_path) {
        path_len = strlen(pager->file_path) + 1;
        mem_free(pager->file_path, path_len);
    }

    mem_free(pager, sizeof(struct amidb_pager));
}

/* Allocate a new page */
int pager_allocate_page(struct amidb_pager *pager, uint32_t *page_num_out) {
    uint32_t i;
    uint8_t *page_buf;
    int rc;

    if (pager->read_only) {
        return -1;
    }

    /* Find first free page in bitmap */
    for (i = 1; i < AMIDB_MAX_PAGES; i++) {
        if (!bitmap_test(pager->bitmap, i)) {
            /* Found free page */
            bitmap_set(pager->bitmap, i);

            if (i >= pager->header.page_count) {
                pager->header.page_count = i + 1;
            }

            /* Update header on disk */
            page_buf = (uint8_t *)mem_alloc(AMIDB_PAGE_SIZE, AMIDB_MEM_CLEAR);
            if (!page_buf) {
                return -1;
            }

            serialize_header(&pager->header, page_buf);
            memcpy(page_buf + 64, pager->bitmap, pager->bitmap_size);

            file_seek(pager->file_handle, 0, AMIDB_SEEK_SET);
            rc = file_write(pager->file_handle, page_buf, AMIDB_PAGE_SIZE);

            if (rc != AMIDB_PAGE_SIZE) {
                mem_free(page_buf, AMIDB_PAGE_SIZE);
                return -1;
            }

            /* Phase 3C: Initialize the new page on disk with valid header */
            memset(page_buf, 0, AMIDB_PAGE_SIZE);
            put_u32(page_buf + 0, i);           /* page_num */
            page_buf[4] = PAGE_TYPE_FREE;       /* page_type */
            /* Compute checksum for empty page */
            crc32_init();
            put_u32(page_buf + 8, crc32_compute(page_buf + 12, AMIDB_PAGE_SIZE - 12));

            /* Write initialized page to disk */
            file_seek(pager->file_handle, i * AMIDB_PAGE_SIZE, AMIDB_SEEK_SET);
            file_write(pager->file_handle, page_buf, AMIDB_PAGE_SIZE);

            mem_free(page_buf, AMIDB_PAGE_SIZE);

            *page_num_out = i;
            return 0;
        }
    }

    return -1;  /* No free pages */
}

/* Free a page */
int pager_free_page(struct amidb_pager *pager, uint32_t page_num) {
    uint8_t *page_buf;
    int rc;

    if (pager->read_only || page_num == 0 || page_num >= AMIDB_MAX_PAGES) {
        return -1;
    }

    if (!bitmap_test(pager->bitmap, page_num)) {
        return -1;  /* Page not allocated */
    }

    bitmap_clear(pager->bitmap, page_num);

    /* Update header on disk */
    page_buf = (uint8_t *)mem_alloc(AMIDB_PAGE_SIZE, AMIDB_MEM_CLEAR);
    if (!page_buf) {
        return -1;
    }

    serialize_header(&pager->header, page_buf);
    memcpy(page_buf + 64, pager->bitmap, pager->bitmap_size);

    file_seek(pager->file_handle, 0, AMIDB_SEEK_SET);
    rc = file_write(pager->file_handle, page_buf, AMIDB_PAGE_SIZE);
    mem_free(page_buf, AMIDB_PAGE_SIZE);

    return (rc == AMIDB_PAGE_SIZE) ? 0 : -1;
}

/* Read a page */
int pager_read_page(struct amidb_pager *pager, uint32_t page_num, uint8_t *page_data) {
    uint32_t offset;
    int rc;
    uint32_t stored_checksum, computed_checksum;
    struct amidb_page_header hdr;

    if (page_num >= pager->header.page_count) {
        return -1;
    }

    offset = page_num * AMIDB_PAGE_SIZE;
    file_seek(pager->file_handle, offset, AMIDB_SEEK_SET);
    rc = file_read(pager->file_handle, page_data, AMIDB_PAGE_SIZE);

    if (rc != AMIDB_PAGE_SIZE) {
        return -1;
    }

    /* Verify page header and checksum */
    hdr.page_num = get_u32(page_data + 0);
    hdr.page_type = page_data[4];
    stored_checksum = get_u32(page_data + 8);

    if (hdr.page_num != page_num) {
        return -1;  /* Page number mismatch */
    }

    /* Verify checksum (skip header bytes) */
    crc32_init();
    computed_checksum = crc32_compute(page_data + 12, AMIDB_PAGE_SIZE - 12);

    if (stored_checksum != computed_checksum) {
        return -1;  /* Checksum mismatch - corruption detected */
    }

    return 0;
}

/* Write a page */
int pager_write_page(struct amidb_pager *pager, uint32_t page_num, const uint8_t *page_data) {
    uint32_t offset;
    int rc;
    uint8_t *write_buf;
    uint32_t checksum;

    if (pager->read_only || page_num >= AMIDB_MAX_PAGES) {
        return -1;
    }

    /* Allocate write buffer */
    write_buf = (uint8_t *)mem_alloc(AMIDB_PAGE_SIZE, 0);
    if (!write_buf) {
        return -1;
    }

    memcpy(write_buf, page_data, AMIDB_PAGE_SIZE);

    /* Set page header */
    put_u32(write_buf + 0, page_num);
    /* page_type already set by caller in write_buf[4] */

    /* Compute and store checksum (excluding header) */
    crc32_init();
    checksum = crc32_compute(write_buf + 12, AMIDB_PAGE_SIZE - 12);
    put_u32(write_buf + 8, checksum);

    offset = page_num * AMIDB_PAGE_SIZE;
    file_seek(pager->file_handle, offset, AMIDB_SEEK_SET);
    rc = file_write(pager->file_handle, write_buf, AMIDB_PAGE_SIZE);

    mem_free(write_buf, AMIDB_PAGE_SIZE);

    return (rc == AMIDB_PAGE_SIZE) ? 0 : -1;
}

/* Sync to disk */
int pager_sync(struct amidb_pager *pager) {
    if (pager->read_only) {
        return 0;
    }
    return file_sync(pager->file_handle);
}

/* Get page count */
uint32_t pager_get_page_count(struct amidb_pager *pager) {
    return pager->header.page_count;
}

/*
 * Get catalog root page number (Phase 4)
 */
uint32_t pager_get_catalog_root(struct amidb_pager *pager) {
    return pager->header.catalog_root;
}

/*
 * Set catalog root page number (Phase 4)
 */
void pager_set_catalog_root(struct amidb_pager *pager, uint32_t catalog_root) {
    pager->header.catalog_root = catalog_root;
    pager_write_header(pager);  /* Persist to disk */
}

/* Write file header (Phase 3C: for persisting WAL state) */
int pager_write_header(struct amidb_pager *pager) {
    uint8_t *page_buf;
    int rc;

    if (!pager || pager->read_only) {
        return -1;
    }

    /* Allocate buffer for header page */
    page_buf = (uint8_t *)mem_alloc(AMIDB_PAGE_SIZE, AMIDB_MEM_CLEAR);
    if (!page_buf) {
        return -1;
    }

    /* Serialize header and bitmap */
    serialize_header(&pager->header, page_buf);
    memcpy(page_buf + 64, pager->bitmap, pager->bitmap_size);

    /* Write to disk */
    file_seek(pager->file_handle, 0, AMIDB_SEEK_SET);
    rc = file_write(pager->file_handle, page_buf, AMIDB_PAGE_SIZE);

    mem_free(page_buf, AMIDB_PAGE_SIZE);

    return (rc == AMIDB_PAGE_SIZE) ? 0 : -1;
}
