/*
 * mem_amiga.c - AmigaOS memory allocation implementation
 *
 * Uses exec.library AllocMem/FreeMem with leak tracking.
 */

#include "os/mem.h"

/* AmigaOS includes */
#include <exec/types.h>
#include <exec/memory.h>
#include <stdint.h>

/* Manual declarations of exec.library functions */
APTR AllocMem(ULONG byteSize, ULONG requirements);
void FreeMem(APTR memoryBlock, ULONG byteSize);

/* Global counters for leak detection - use standard types for tests */
uint32_t g_alloc_bytes = 0;
uint32_t g_free_bytes = 0;

/* Allocate memory */
void *mem_alloc(uint32_t size, uint32_t flags) {
    ULONG amiga_flags = MEMF_ANY;
    void *ptr;

    if (!size) {
        return NULL;
    }

    /* Translate flags */
    if (flags & AMIDB_MEM_CLEAR) {
        amiga_flags |= MEMF_CLEAR;
    }

    ptr = AllocMem(size, amiga_flags);
    if (ptr) {
        g_alloc_bytes += size;
    }

    return ptr;
}

/* Free memory */
void mem_free(void *ptr, uint32_t size) {
    if (!ptr || !size) {
        return;
    }

    FreeMem(ptr, size);
    g_free_bytes += size;
}

/* Reallocate memory */
void *mem_realloc(void *ptr, uint32_t old_size, uint32_t new_size, uint32_t flags) {
    void *new_ptr;
    ULONG copy_size;

    if (!new_size) {
        if (ptr) {
            mem_free(ptr, old_size);
        }
        return NULL;
    }

    if (!ptr) {
        return mem_alloc(new_size, flags);
    }

    new_ptr = mem_alloc(new_size, flags);
    if (!new_ptr) {
        return NULL;
    }

    /* Copy old data */
    copy_size = (old_size < new_size) ? old_size : new_size;
    if (copy_size > 0) {
        UBYTE *src = (UBYTE *)ptr;
        UBYTE *dst = (UBYTE *)new_ptr;
        ULONG i;

        for (i = 0; i < copy_size; i++) {
            dst[i] = src[i];
        }
    }

    mem_free(ptr, old_size);

    return new_ptr;
}

/* Get total allocated bytes */
uint32_t mem_get_allocated(void) {
    return g_alloc_bytes;
}

/* Get total freed bytes */
uint32_t mem_get_freed(void) {
    return g_free_bytes;
}

/* Reset allocation counters */
void mem_reset_counters(void) {
    g_alloc_bytes = 0;
    g_free_bytes = 0;
}
