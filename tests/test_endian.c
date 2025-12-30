/*
 * test_endian.c - Unit tests for endian conversion
 */

#include "test_harness.h"
#include "util/endian.h"
#include <string.h>

/* Test 16-bit endian conversion roundtrip */
TEST(endian_u16_roundtrip) {
    uint8_t buf[2];
    uint16_t value = 0x1234;
    uint16_t result;

    TEST_BEGIN();

    put_u16(buf, value);
    result = get_u16(buf);

    ASSERT_EQ(result, value);

    TEST_END();
    return 0;
}

/* Test 16-bit little-endian byte order */
TEST(endian_u16_byte_order) {
    uint8_t buf[2];
    uint16_t value = 0x1234;

    TEST_BEGIN();

    put_u16(buf, value);

    /* Verify little-endian: 0x1234 -> [0x34, 0x12] */
    ASSERT_EQ(buf[0], 0x34);
    ASSERT_EQ(buf[1], 0x12);

    TEST_END();
    return 0;
}

/* Test 32-bit endian conversion roundtrip */
TEST(endian_u32_roundtrip) {
    uint8_t buf[4];
    uint32_t value = 0x12345678UL;
    uint32_t result;

    TEST_BEGIN();

    put_u32(buf, value);
    result = get_u32(buf);

    ASSERT_EQ(result, value);

    TEST_END();
    return 0;
}

/* Test 32-bit little-endian byte order */
TEST(endian_u32_byte_order) {
    uint8_t buf[4];
    uint32_t value = 0x12345678UL;

    TEST_BEGIN();

    put_u32(buf, value);

    /* Verify little-endian: 0x12345678 -> [0x78, 0x56, 0x34, 0x12] */
    ASSERT_EQ(buf[0], 0x78);
    ASSERT_EQ(buf[1], 0x56);
    ASSERT_EQ(buf[2], 0x34);
    ASSERT_EQ(buf[3], 0x12);

    TEST_END();
    return 0;
}

/* Test 32-bit zero value */
TEST(endian_u32_zero) {
    uint8_t buf[4];
    uint32_t value = 0;
    uint32_t result;

    TEST_BEGIN();

    put_u32(buf, value);
    result = get_u32(buf);

    ASSERT_EQ(result, 0);
    ASSERT_EQ(buf[0], 0);
    ASSERT_EQ(buf[1], 0);
    ASSERT_EQ(buf[2], 0);
    ASSERT_EQ(buf[3], 0);

    TEST_END();
    return 0;
}

/* Test 32-bit max value */
TEST(endian_u32_max) {
    uint8_t buf[4];
    uint32_t value = 0xFFFFFFFFUL;
    uint32_t result;

    TEST_BEGIN();

    put_u32(buf, value);
    result = get_u32(buf);

    ASSERT_EQ(result, value);
    ASSERT_EQ(buf[0], 0xFF);
    ASSERT_EQ(buf[1], 0xFF);
    ASSERT_EQ(buf[2], 0xFF);
    ASSERT_EQ(buf[3], 0xFF);

    TEST_END();
    return 0;
}
