/*
 * row.h - Row serialization and deserialization
 *
 * Handles encoding/decoding of database rows to/from binary format.
 * Supports INTEGER, TEXT, BLOB, and NULL data types.
 */

#ifndef AMIDB_ROW_H
#define AMIDB_ROW_H

#include <stdint.h>

/* Data types */
#define AMIDB_TYPE_NULL    0
#define AMIDB_TYPE_INTEGER 1
#define AMIDB_TYPE_TEXT    2
#define AMIDB_TYPE_BLOB    3

/* Maximum number of columns per row */
#define AMIDB_MAX_COLUMNS 32

/* Column value */
struct amidb_value {
    uint8_t type;           /* AMIDB_TYPE_* */
    union {
        int32_t  i;         /* INTEGER value */
        struct {
            uint8_t *data;  /* TEXT or BLOB data */
            uint32_t size;  /* Size in bytes */
        } blob;
    } u;
};

/* Row structure */
struct amidb_row {
    uint32_t column_count;
    struct amidb_value values[AMIDB_MAX_COLUMNS];
};

/*
 * Initialize a row
 *
 * Sets column_count to 0 and initializes all values to NULL.
 */
void row_init(struct amidb_row *row);

/*
 * Clear a row
 *
 * Frees any allocated memory and resets to empty state.
 */
void row_clear(struct amidb_row *row);

/*
 * Set an INTEGER value
 *
 * column_index: Column index (0-based)
 * value: Integer value to set
 *
 * Returns: 0 on success, -1 on error
 */
int row_set_int(struct amidb_row *row, uint32_t column_index, int32_t value);

/*
 * Set a TEXT value
 *
 * column_index: Column index (0-based)
 * text: Text string (will be copied)
 * length: Length of text in bytes (or 0 to use strlen)
 *
 * Returns: 0 on success, -1 on error
 */
int row_set_text(struct amidb_row *row, uint32_t column_index, const char *text, uint32_t length);

/*
 * Set a BLOB value
 *
 * column_index: Column index (0-based)
 * data: Binary data (will be copied)
 * size: Size of data in bytes
 *
 * Returns: 0 on success, -1 on error
 */
int row_set_blob(struct amidb_row *row, uint32_t column_index, const uint8_t *data, uint32_t size);

/*
 * Set a NULL value
 *
 * column_index: Column index (0-based)
 *
 * Returns: 0 on success, -1 on error
 */
int row_set_null(struct amidb_row *row, uint32_t column_index);

/*
 * Get column value
 *
 * column_index: Column index (0-based)
 *
 * Returns: Pointer to value, or NULL on error
 */
const struct amidb_value *row_get_value(const struct amidb_row *row, uint32_t column_index);

/*
 * Serialize a row to binary format
 *
 * Format:
 *   [2 bytes] column_count (little-endian)
 *   For each column:
 *     [1 byte] type
 *     [4 bytes] value (for INTEGER) or size (for TEXT/BLOB)
 *     [n bytes] data (for TEXT/BLOB)
 *
 * row: Row to serialize
 * buffer: Output buffer
 * buffer_size: Size of output buffer
 *
 * Returns: Number of bytes written, or -1 on error
 */
int row_serialize(const struct amidb_row *row, uint8_t *buffer, uint32_t buffer_size);

/*
 * Deserialize a row from binary format
 *
 * row: Row to populate (must be initialized)
 * buffer: Input buffer
 * buffer_size: Size of input buffer
 *
 * Returns: Number of bytes read, or -1 on error
 */
int row_deserialize(struct amidb_row *row, const uint8_t *buffer, uint32_t buffer_size);

/*
 * Get serialized size of a row
 *
 * Returns: Size in bytes needed to serialize the row
 */
uint32_t row_get_serialized_size(const struct amidb_row *row);

#endif /* AMIDB_ROW_H */
