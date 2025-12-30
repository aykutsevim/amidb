/*
 * crc32.c - CRC32 implementation for AmiDB
 */

#include "util/crc32.h"

/* CRC32 lookup table */
static uint32_t crc32_table[256];
static int crc32_table_initialized = 0;

/* Initialize CRC32 lookup table */
void crc32_init(void) {
    uint32_t i, j, c;

    if (crc32_table_initialized) {
        return;
    }

    for (i = 0; i < 256; i++) {
        c = i;
        for (j = 0; j < 8; j++) {
            if (c & 1) {
                c = 0xEDB88320UL ^ (c >> 1);
            } else {
                c = c >> 1;
            }
        }
        crc32_table[i] = c;
    }

    crc32_table_initialized = 1;
}

/* Update CRC32 with new data */
uint32_t crc32_update(uint32_t crc, const uint8_t *data, uint32_t length) {
    uint32_t i;

    /* Ensure table is initialized */
    if (!crc32_table_initialized) {
        crc32_init();
    }

    crc = crc ^ 0xFFFFFFFFUL;

    for (i = 0; i < length; i++) {
        crc = crc32_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }

    return crc ^ 0xFFFFFFFFUL;
}

/* Compute CRC32 of a buffer */
uint32_t crc32_compute(const uint8_t *data, uint32_t length) {
    return crc32_update(0, data, length);
}
