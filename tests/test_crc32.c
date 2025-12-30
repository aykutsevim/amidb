/*
 * test_crc32.c - Unit tests for CRC32 checksums
 */

#include "test_harness.h"
#include "util/crc32.h"
#include <string.h>

/* Test CRC32 known value for "123456789" */
TEST(crc32_known_value) {
    uint8_t data[] = "123456789";
    uint32_t crc;

    TEST_BEGIN();

    crc32_init();
    crc = crc32_compute(data, 9);

    /* Known CRC32 of "123456789" is 0xCBF43926 */
    ASSERT_EQ(crc, 0xCBF43926UL);

    TEST_END();
    return 0;
}

/* Test CRC32 of empty string */
TEST(crc32_empty) {
    uint8_t data[] = "";
    uint32_t crc;

    TEST_BEGIN();

    crc32_init();
    crc = crc32_compute(data, 0);

    /* CRC32 of empty data should be 0 */
    ASSERT_EQ(crc, 0);

    TEST_END();
    return 0;
}

/* Test CRC32 incremental computation */
TEST(crc32_incremental) {
    uint8_t data[] = "123456789";
    uint32_t crc1, crc2, crc_inc;

    TEST_BEGIN();

    crc32_init();

    /* Compute in one pass */
    crc1 = crc32_compute(data, 9);

    /* Compute incrementally */
    crc_inc = crc32_update(0, data, 5);      /* "12345" */
    crc_inc = crc32_update(crc_inc, data + 5, 4);  /* "6789" */

    /* Should match */
    ASSERT_EQ(crc_inc, crc1);
    ASSERT_EQ(crc_inc, 0xCBF43926UL);

    TEST_END();
    return 0;
}

/* Test CRC32 different data */
TEST(crc32_different_data) {
    uint8_t data1[] = "Hello";
    uint8_t data2[] = "World";
    uint32_t crc1, crc2;

    TEST_BEGIN();

    crc32_init();
    crc1 = crc32_compute(data1, 5);
    crc2 = crc32_compute(data2, 5);

    /* Different data should have different CRCs */
    ASSERT_NEQ(crc1, crc2);

    TEST_END();
    return 0;
}

/* Test CRC32 collision detection */
TEST(crc32_one_bit_change) {
    uint8_t data1[] = "test";
    uint8_t data2[] = "Test";  /* Only first letter case changed */
    uint32_t crc1, crc2;

    TEST_BEGIN();

    crc32_init();
    crc1 = crc32_compute(data1, 4);
    crc2 = crc32_compute(data2, 4);

    /* Even one bit change should produce different CRC */
    ASSERT_NEQ(crc1, crc2);

    TEST_END();
    return 0;
}
