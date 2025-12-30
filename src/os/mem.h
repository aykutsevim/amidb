/*
 * mem.h - Memory allocation interface for AmiDB
 *
 * Provides memory allocation with leak tracking for debugging.
 */

#ifndef AMIDB_MEM_H
#define AMIDB_MEM_H

#include <stdint.h>

/* Memory allocation flags */
#define AMIDB_MEM_CLEAR 0x01  /* Clear memory to zero */

/* Allocate memory */
void *mem_alloc(uint32_t size, uint32_t flags);

/* Free memory */
void mem_free(void *ptr, uint32_t size);

/* Reallocate memory (not recommended - prefer fixed allocations) */
void *mem_realloc(void *ptr, uint32_t old_size, uint32_t new_size, uint32_t flags);

/* Get total allocated bytes (for leak detection) */
uint32_t mem_get_allocated(void);

/* Get total freed bytes (for leak detection) */
uint32_t mem_get_freed(void);

/* Reset allocation counters */
void mem_reset_counters(void);

#endif /* AMIDB_MEM_H */
