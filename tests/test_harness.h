/*
 * test_harness.h - Minimal test framework for AmiDB on AmigaOS
 *
 * Provides simple unit testing macros with memory leak detection.
 */

#ifndef TEST_HARNESS_H
#define TEST_HARNESS_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>

/* Global log file handle */
extern FILE *g_test_log;

/* Helper to print to both stdout and log file */
static inline void test_printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);

    if (g_test_log) {
        va_start(args, fmt);
        vfprintf(g_test_log, fmt, args);
        va_end(args);
        fflush(g_test_log);
    }
}

/* External counters for leak detection */
extern uint32_t g_alloc_bytes;
extern uint32_t g_free_bytes;

/* Test definition macro */
#define TEST(name) \
    int test_##name(void)

/* Assertion macros */
#define ASSERT(cond) \
    do { \
        if (!(cond)) { \
            test_printf("  FAIL: %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            return 1; \
        } \
    } while(0)

#define ASSERT_EQ(a, b) \
    do { \
        if ((a) != (b)) { \
            test_printf("  FAIL: %s:%d: Expected %d, got %d\n", \
                   __FILE__, __LINE__, (int)(b), (int)(a)); \
            return 1; \
        } \
    } while(0)

#define ASSERT_NEQ(a, b) \
    do { \
        if ((a) == (b)) { \
            test_printf("  FAIL: %s:%d: Values should not be equal: %d\n", \
                   __FILE__, __LINE__, (int)(a)); \
            return 1; \
        } \
    } while(0)

#define ASSERT_STR_EQ(a, b) \
    do { \
        if (strcmp((a), (b)) != 0) { \
            test_printf("  FAIL: %s:%d: Expected \"%s\", got \"%s\"\n", \
                   __FILE__, __LINE__, (b), (a)); \
            return 1; \
        } \
    } while(0)

#define ASSERT_NULL(ptr) \
    do { \
        if ((ptr) != NULL) { \
            test_printf("  FAIL: %s:%d: Expected NULL, got %p\n", \
                   __FILE__, __LINE__, (ptr)); \
            return 1; \
        } \
    } while(0)

#define ASSERT_NOT_NULL(ptr) \
    do { \
        if ((ptr) == NULL) { \
            test_printf("  FAIL: %s:%d: Expected non-NULL\n", \
                   __FILE__, __LINE__); \
            return 1; \
        } \
    } while(0)

#define ASSERT_GT(a, b) \
    do { \
        if ((a) <= (b)) { \
            test_printf("  FAIL: %s:%d: Expected %d > %d\n", \
                   __FILE__, __LINE__, (int)(a), (int)(b)); \
            return 1; \
        } \
    } while(0)

#define ASSERT_LT(a, b) \
    do { \
        if ((a) >= (b)) { \
            test_printf("  FAIL: %s:%d: Expected %d < %d\n", \
                   __FILE__, __LINE__, (int)(a), (int)(b)); \
            return 1; \
        } \
    } while(0)

/* Test runner macro */
#define RUN_TEST(name) \
    do { \
        test_printf("Running %s... ", #name); \
        fflush(stdout); \
        if (test_##name() == 0) { \
            test_printf("OK\n"); \
            passed++; \
        } else { \
            failed++; \
        } \
    } while(0)

/* Memory leak detection macros */
#define TEST_BEGIN() \
    do { \
        g_alloc_bytes = 0; \
        g_free_bytes = 0; \
    } while(0)

#define TEST_END() \
    do { \
        if (g_alloc_bytes != g_free_bytes) { \
            test_printf("  MEMORY LEAK: %u bytes allocated, %u freed (leak: %d bytes)\n", \
                   g_alloc_bytes, g_free_bytes, \
                   (int)(g_alloc_bytes - g_free_bytes)); \
            return 1; \
        } \
    } while(0)

/* Test section macro for organizing tests */
#define TEST_SECTION(name) \
    test_printf("\n--- %s ---\n", name)

#endif /* TEST_HARNESS_H */
