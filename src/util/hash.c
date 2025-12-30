/*
 * hash.c - String hashing implementation for AmiDB
 *
 * Uses DJB2 hash algorithm (simple and effective for strings)
 */

#include "util/hash.h"
#include <ctype.h>

/* DJB2 hash algorithm */
uint32_t hash_string(const char *str) {
    uint32_t hash = 5381;
    int c;

    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    }

    return hash;
}

/* Case-insensitive DJB2 hash */
uint32_t hash_string_ci(const char *str) {
    uint32_t hash = 5381;
    int c;

    while ((c = *str++)) {
        c = tolower(c);
        hash = ((hash << 5) + hash) + c;
    }

    return hash;
}

/* Hash a buffer */
uint32_t hash_buffer(const uint8_t *data, uint32_t length) {
    uint32_t hash = 5381;
    uint32_t i;

    for (i = 0; i < length; i++) {
        hash = ((hash << 5) + hash) + data[i];
    }

    return hash;
}
