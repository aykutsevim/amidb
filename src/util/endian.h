/*
 * endian.h - Endian conversion utilities for AmiDB
 *
 * 68000 is big-endian, but database uses little-endian storage
 * for cross-platform compatibility.
 */

#ifndef AMIDB_ENDIAN_H
#define AMIDB_ENDIAN_H

#include <stdint.h>

/* Write 16-bit integer to buffer (little-endian) */
static inline void put_u16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)(v);
    p[1] = (uint8_t)(v >> 8);
}

/* Read 16-bit integer from buffer (little-endian) */
static inline uint16_t get_u16(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

/* Write 32-bit integer to buffer (little-endian) */
static inline void put_u32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v);
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

/* Read 32-bit integer from buffer (little-endian) */
static inline uint32_t get_u32(const uint8_t *p) {
    return (uint32_t)p[0]
         | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16)
         | ((uint32_t)p[3] << 24);
}

/* Write 64-bit integer to buffer (little-endian) */
static inline void put_u64(uint8_t *p, uint64_t v) {
    p[0] = (uint8_t)(v);
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
    p[4] = (uint8_t)(v >> 32);
    p[5] = (uint8_t)(v >> 40);
    p[6] = (uint8_t)(v >> 48);
    p[7] = (uint8_t)(v >> 56);
}

/* Read 64-bit integer from buffer (little-endian) */
static inline uint64_t get_u64(const uint8_t *p) {
    return (uint64_t)p[0]
         | ((uint64_t)p[1] << 8)
         | ((uint64_t)p[2] << 16)
         | ((uint64_t)p[3] << 24)
         | ((uint64_t)p[4] << 32)
         | ((uint64_t)p[5] << 40)
         | ((uint64_t)p[6] << 48)
         | ((uint64_t)p[7] << 56);
}

#endif /* AMIDB_ENDIAN_H */
