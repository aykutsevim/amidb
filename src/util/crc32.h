/*
 * crc32.h - CRC32 checksum for AmiDB
 *
 * Used for page integrity checking. Every page has a CRC32 checksum
 * to detect corruption.
 */

#ifndef AMIDB_CRC32_H
#define AMIDB_CRC32_H

#include <stdint.h>

/* Initialize CRC32 tables (call once at startup) */
void crc32_init(void);

/* Compute CRC32 of a buffer */
uint32_t crc32_compute(const uint8_t *data, uint32_t length);

/* Incremental CRC32 computation */
uint32_t crc32_update(uint32_t crc, const uint8_t *data, uint32_t length);

#endif /* AMIDB_CRC32_H */
