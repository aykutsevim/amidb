/*
 * test_row.c - Unit tests for row serialization
 */

#include "test_harness.h"
#include "storage/row.h"
#include "os/mem.h"
#include <string.h>
#include <stdio.h>

/* Test: Initialize and clear row */
TEST(row_init_clear) {
    struct amidb_row row;

    TEST_BEGIN();

    /* Initialize row */
    row_init(&row);
    ASSERT_EQ(row.column_count, 0);

    /* Set some values */
    row_set_int(&row, 0, 42);
    row_set_text(&row, 1, "hello", 5);
    ASSERT_EQ(row.column_count, 2);

    /* Clear row */
    row_clear(&row);
    ASSERT_EQ(row.column_count, 0);

    TEST_END();
    return 0;
}

/* Test: Set and get INTEGER values */
TEST(row_integer) {
    struct amidb_row row;
    const struct amidb_value *val;

    TEST_BEGIN();

    row_init(&row);

    /* Set integer values */
    row_set_int(&row, 0, 123);
    row_set_int(&row, 1, -456);
    row_set_int(&row, 2, 0);

    ASSERT_EQ(row.column_count, 3);

    /* Get and verify values */
    val = row_get_value(&row, 0);
    ASSERT_NOT_NULL(val);
    ASSERT_EQ(val->type, AMIDB_TYPE_INTEGER);
    ASSERT_EQ(val->u.i, 123);

    val = row_get_value(&row, 1);
    ASSERT_NOT_NULL(val);
    ASSERT_EQ(val->type, AMIDB_TYPE_INTEGER);
    ASSERT_EQ(val->u.i, -456);

    val = row_get_value(&row, 2);
    ASSERT_NOT_NULL(val);
    ASSERT_EQ(val->type, AMIDB_TYPE_INTEGER);
    ASSERT_EQ(val->u.i, 0);

    row_clear(&row);

    TEST_END();
    return 0;
}

/* Test: Set and get TEXT values */
TEST(row_text) {
    struct amidb_row row;
    const struct amidb_value *val;

    TEST_BEGIN();

    row_init(&row);

    /* Set text values */
    row_set_text(&row, 0, "Hello", 5);
    row_set_text(&row, 1, "World", 5);
    row_set_text(&row, 2, "", 0);  /* Empty string */

    ASSERT_EQ(row.column_count, 3);

    /* Get and verify values */
    val = row_get_value(&row, 0);
    ASSERT_NOT_NULL(val);
    ASSERT_EQ(val->type, AMIDB_TYPE_TEXT);
    ASSERT_EQ(val->u.blob.size, 5);
    ASSERT_EQ(memcmp(val->u.blob.data, "Hello", 5), 0);

    val = row_get_value(&row, 1);
    ASSERT_NOT_NULL(val);
    ASSERT_EQ(val->type, AMIDB_TYPE_TEXT);
    ASSERT_EQ(val->u.blob.size, 5);
    ASSERT_EQ(memcmp(val->u.blob.data, "World", 5), 0);

    val = row_get_value(&row, 2);
    ASSERT_NOT_NULL(val);
    ASSERT_EQ(val->type, AMIDB_TYPE_TEXT);
    ASSERT_EQ(val->u.blob.size, 0);

    row_clear(&row);

    TEST_END();
    return 0;
}

/* Test: Set and get BLOB values */
TEST(row_blob) {
    struct amidb_row row;
    const struct amidb_value *val;
    uint8_t blob_data[] = {0x01, 0x02, 0x03, 0x04, 0x05};

    TEST_BEGIN();

    row_init(&row);

    /* Set blob value */
    row_set_blob(&row, 0, blob_data, sizeof(blob_data));

    ASSERT_EQ(row.column_count, 1);

    /* Get and verify value */
    val = row_get_value(&row, 0);
    ASSERT_NOT_NULL(val);
    ASSERT_EQ(val->type, AMIDB_TYPE_BLOB);
    ASSERT_EQ(val->u.blob.size, sizeof(blob_data));
    ASSERT_EQ(memcmp(val->u.blob.data, blob_data, sizeof(blob_data)), 0);

    row_clear(&row);

    TEST_END();
    return 0;
}

/* Test: Set and get NULL values */
TEST(row_null) {
    struct amidb_row row;
    const struct amidb_value *val;

    TEST_BEGIN();

    row_init(&row);

    /* Set NULL values */
    row_set_null(&row, 0);
    row_set_null(&row, 1);

    ASSERT_EQ(row.column_count, 2);

    /* Get and verify values */
    val = row_get_value(&row, 0);
    ASSERT_NOT_NULL(val);
    ASSERT_EQ(val->type, AMIDB_TYPE_NULL);

    val = row_get_value(&row, 1);
    ASSERT_NOT_NULL(val);
    ASSERT_EQ(val->type, AMIDB_TYPE_NULL);

    row_clear(&row);

    TEST_END();
    return 0;
}

/* Test: Mixed types in a row */
TEST(row_mixed_types) {
    struct amidb_row row;
    const struct amidb_value *val;
    uint8_t blob_data[] = {0xAA, 0xBB, 0xCC};

    TEST_BEGIN();

    row_init(&row);

    /* Set mixed types */
    row_set_int(&row, 0, 42);
    row_set_text(&row, 1, "test", 4);
    row_set_null(&row, 2);
    row_set_blob(&row, 3, blob_data, sizeof(blob_data));

    ASSERT_EQ(row.column_count, 4);

    /* Verify all values */
    val = row_get_value(&row, 0);
    ASSERT_EQ(val->type, AMIDB_TYPE_INTEGER);
    ASSERT_EQ(val->u.i, 42);

    val = row_get_value(&row, 1);
    ASSERT_EQ(val->type, AMIDB_TYPE_TEXT);
    ASSERT_EQ(val->u.blob.size, 4);

    val = row_get_value(&row, 2);
    ASSERT_EQ(val->type, AMIDB_TYPE_NULL);

    val = row_get_value(&row, 3);
    ASSERT_EQ(val->type, AMIDB_TYPE_BLOB);
    ASSERT_EQ(val->u.blob.size, 3);

    row_clear(&row);

    TEST_END();
    return 0;
}

/* Test: Serialize and deserialize integer row */
TEST(row_serialize_integer) {
    struct amidb_row row1, row2;
    uint8_t buffer[256];
    int bytes_written, bytes_read;
    const struct amidb_value *val;

    TEST_BEGIN();

    row_init(&row1);
    row_init(&row2);

    /* Create row with integers */
    row_set_int(&row1, 0, 123);
    row_set_int(&row1, 1, -456);
    row_set_int(&row1, 2, 789);

    /* Serialize */
    bytes_written = row_serialize(&row1, buffer, sizeof(buffer));
    test_printf("  Serialized %d bytes\n", bytes_written);
    ASSERT_EQ(bytes_written > 0, 1);

    /* Deserialize */
    bytes_read = row_deserialize(&row2, buffer, bytes_written);
    ASSERT_EQ(bytes_read, bytes_written);
    ASSERT_EQ(row2.column_count, 3);

    /* Verify deserialized values */
    val = row_get_value(&row2, 0);
    ASSERT_EQ(val->type, AMIDB_TYPE_INTEGER);
    ASSERT_EQ(val->u.i, 123);

    val = row_get_value(&row2, 1);
    ASSERT_EQ(val->type, AMIDB_TYPE_INTEGER);
    ASSERT_EQ(val->u.i, -456);

    val = row_get_value(&row2, 2);
    ASSERT_EQ(val->type, AMIDB_TYPE_INTEGER);
    ASSERT_EQ(val->u.i, 789);

    row_clear(&row1);
    row_clear(&row2);

    TEST_END();
    return 0;
}

/* Test: Serialize and deserialize text row */
TEST(row_serialize_text) {
    struct amidb_row row1, row2;
    uint8_t buffer[256];
    int bytes_written, bytes_read;
    const struct amidb_value *val;

    TEST_BEGIN();

    row_init(&row1);
    row_init(&row2);

    /* Create row with text */
    row_set_text(&row1, 0, "Hello", 5);
    row_set_text(&row1, 1, "World", 5);

    /* Serialize */
    bytes_written = row_serialize(&row1, buffer, sizeof(buffer));
    test_printf("  Serialized %d bytes\n", bytes_written);
    ASSERT_EQ(bytes_written > 0, 1);

    /* Deserialize */
    bytes_read = row_deserialize(&row2, buffer, bytes_written);
    ASSERT_EQ(bytes_read, bytes_written);
    ASSERT_EQ(row2.column_count, 2);

    /* Verify deserialized values */
    val = row_get_value(&row2, 0);
    ASSERT_EQ(val->type, AMIDB_TYPE_TEXT);
    ASSERT_EQ(val->u.blob.size, 5);
    ASSERT_EQ(memcmp(val->u.blob.data, "Hello", 5), 0);

    val = row_get_value(&row2, 1);
    ASSERT_EQ(val->type, AMIDB_TYPE_TEXT);
    ASSERT_EQ(val->u.blob.size, 5);
    ASSERT_EQ(memcmp(val->u.blob.data, "World", 5), 0);

    row_clear(&row1);
    row_clear(&row2);

    TEST_END();
    return 0;
}

/* Test: Serialize and deserialize mixed row */
TEST(row_serialize_mixed) {
    struct amidb_row row1, row2;
    uint8_t buffer[256];
    uint8_t blob_data[] = {0xDE, 0xAD, 0xBE, 0xEF};
    int bytes_written, bytes_read;
    const struct amidb_value *val;
    uint32_t expected_size;

    TEST_BEGIN();

    row_init(&row1);
    row_init(&row2);

    /* Create row with mixed types */
    row_set_int(&row1, 0, 42);
    row_set_text(&row1, 1, "test", 4);
    row_set_null(&row1, 2);
    row_set_blob(&row1, 3, blob_data, sizeof(blob_data));

    /* Check serialized size */
    expected_size = row_get_serialized_size(&row1);
    test_printf("  Expected size: %u bytes\n", expected_size);

    /* Serialize */
    bytes_written = row_serialize(&row1, buffer, sizeof(buffer));
    test_printf("  Serialized %d bytes\n", bytes_written);
    ASSERT_EQ(bytes_written, (int)expected_size);

    /* Deserialize */
    bytes_read = row_deserialize(&row2, buffer, bytes_written);
    ASSERT_EQ(bytes_read, bytes_written);
    ASSERT_EQ(row2.column_count, 4);

    /* Verify all values */
    val = row_get_value(&row2, 0);
    ASSERT_EQ(val->type, AMIDB_TYPE_INTEGER);
    ASSERT_EQ(val->u.i, 42);

    val = row_get_value(&row2, 1);
    ASSERT_EQ(val->type, AMIDB_TYPE_TEXT);
    ASSERT_EQ(val->u.blob.size, 4);
    ASSERT_EQ(memcmp(val->u.blob.data, "test", 4), 0);

    val = row_get_value(&row2, 2);
    ASSERT_EQ(val->type, AMIDB_TYPE_NULL);

    val = row_get_value(&row2, 3);
    ASSERT_EQ(val->type, AMIDB_TYPE_BLOB);
    ASSERT_EQ(val->u.blob.size, 4);
    ASSERT_EQ(memcmp(val->u.blob.data, blob_data, 4), 0);

    row_clear(&row1);
    row_clear(&row2);

    TEST_END();
    return 0;
}

/* Test: Roundtrip with empty row */
TEST(row_serialize_empty) {
    struct amidb_row row1, row2;
    uint8_t buffer[256];
    int bytes_written, bytes_read;

    TEST_BEGIN();

    row_init(&row1);
    row_init(&row2);

    /* Serialize empty row */
    bytes_written = row_serialize(&row1, buffer, sizeof(buffer));
    test_printf("  Serialized empty row: %d bytes\n", bytes_written);
    ASSERT_EQ(bytes_written, 2);  /* Just column count */

    /* Deserialize */
    bytes_read = row_deserialize(&row2, buffer, bytes_written);
    ASSERT_EQ(bytes_read, bytes_written);
    ASSERT_EQ(row2.column_count, 0);

    row_clear(&row1);
    row_clear(&row2);

    TEST_END();
    return 0;
}
