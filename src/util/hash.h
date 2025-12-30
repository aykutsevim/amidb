/*
 * hash.h - String hashing for AmiDB
 *
 * Used for catalog lookups and symbol tables.
 */

#ifndef AMIDB_HASH_H
#define AMIDB_HASH_H

#include <stdint.h>

/* Compute hash of a string (case-sensitive) */
uint32_t hash_string(const char *str);

/* Compute hash of a string (case-insensitive) */
uint32_t hash_string_ci(const char *str);

/* Compute hash of a buffer */
uint32_t hash_buffer(const uint8_t *data, uint32_t length);

#endif /* AMIDB_HASH_H */
