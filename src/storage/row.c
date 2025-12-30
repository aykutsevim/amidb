/*
 * row.c - Row serialization implementation
 */

#include "storage/row.h"
#include "os/mem.h"
#include <string.h>

/* Helper functions for little-endian encoding */
static inline void put_u16(uint8_t *buf, uint16_t val) {
    buf[0] = (uint8_t)(val & 0xFF);
    buf[1] = (uint8_t)((val >> 8) & 0xFF);
}

static inline uint16_t get_u16(const uint8_t *buf) {
    return (uint16_t)buf[0] | ((uint16_t)buf[1] << 8);
}

static inline void put_u32(uint8_t *buf, uint32_t val) {
    buf[0] = (uint8_t)(val & 0xFF);
    buf[1] = (uint8_t)((val >> 8) & 0xFF);
    buf[2] = (uint8_t)((val >> 16) & 0xFF);
    buf[3] = (uint8_t)((val >> 24) & 0xFF);
}

static inline uint32_t get_u32(const uint8_t *buf) {
    return (uint32_t)buf[0] |
           ((uint32_t)buf[1] << 8) |
           ((uint32_t)buf[2] << 16) |
           ((uint32_t)buf[3] << 24);
}

/*
 * Initialize a row
 */
void row_init(struct amidb_row *row) {
    uint32_t i;

    if (!row) {
        return;
    }

    row->column_count = 0;

    for (i = 0; i < AMIDB_MAX_COLUMNS; i++) {
        row->values[i].type = AMIDB_TYPE_NULL;
        row->values[i].u.blob.data = NULL;
        row->values[i].u.blob.size = 0;
    }
}

/*
 * Clear a row
 */
void row_clear(struct amidb_row *row) {
    uint32_t i;

    if (!row) {
        return;
    }

    /* Free any allocated memory */
    for (i = 0; i < row->column_count; i++) {
        if ((row->values[i].type == AMIDB_TYPE_TEXT ||
             row->values[i].type == AMIDB_TYPE_BLOB) &&
            row->values[i].u.blob.data != NULL) {
            mem_free(row->values[i].u.blob.data, row->values[i].u.blob.size);
            row->values[i].u.blob.data = NULL;
            row->values[i].u.blob.size = 0;
        }
    }

    row_init(row);
}

/*
 * Set an INTEGER value
 */
int row_set_int(struct amidb_row *row, uint32_t column_index, int32_t value) {
    if (!row || column_index >= AMIDB_MAX_COLUMNS) {
        return -1;
    }

    /* Clear any existing value */
    if (column_index < row->column_count &&
        (row->values[column_index].type == AMIDB_TYPE_TEXT ||
         row->values[column_index].type == AMIDB_TYPE_BLOB) &&
        row->values[column_index].u.blob.data != NULL) {
        mem_free(row->values[column_index].u.blob.data,
                 row->values[column_index].u.blob.size);
    }

    row->values[column_index].type = AMIDB_TYPE_INTEGER;
    row->values[column_index].u.i = value;

    if (column_index >= row->column_count) {
        row->column_count = column_index + 1;
    }

    return 0;
}

/*
 * Set a TEXT value
 */
int row_set_text(struct amidb_row *row, uint32_t column_index, const char *text, uint32_t length) {
    uint8_t *data = NULL;

    if (!row || !text || column_index >= AMIDB_MAX_COLUMNS) {
        return -1;
    }

    /* Calculate length if not provided */
    if (length == 0) {
        const char *p = text;
        while (*p) {
            p++;
            length++;
        }
    }

    /* Allocate memory for text (only if length > 0) */
    if (length > 0) {
        data = (uint8_t *)mem_alloc(length, 0);
        if (!data) {
            return -1;
        }

        memcpy(data, text, length);
    }

    /* Clear any existing value */
    if (column_index < row->column_count &&
        (row->values[column_index].type == AMIDB_TYPE_TEXT ||
         row->values[column_index].type == AMIDB_TYPE_BLOB) &&
        row->values[column_index].u.blob.data != NULL) {
        mem_free(row->values[column_index].u.blob.data,
                 row->values[column_index].u.blob.size);
    }

    row->values[column_index].type = AMIDB_TYPE_TEXT;
    row->values[column_index].u.blob.data = data;
    row->values[column_index].u.blob.size = length;

    if (column_index >= row->column_count) {
        row->column_count = column_index + 1;
    }

    return 0;
}

/*
 * Set a BLOB value
 */
int row_set_blob(struct amidb_row *row, uint32_t column_index, const uint8_t *data, uint32_t size) {
    uint8_t *blob_data = NULL;

    if (!row || column_index >= AMIDB_MAX_COLUMNS) {
        return -1;
    }

    /* Allow NULL data only if size is 0 */
    if (!data && size > 0) {
        return -1;
    }

    /* Allocate memory for blob (only if size > 0) */
    if (size > 0) {
        blob_data = (uint8_t *)mem_alloc(size, 0);
        if (!blob_data) {
            return -1;
        }

        memcpy(blob_data, data, size);
    }

    /* Clear any existing value */
    if (column_index < row->column_count &&
        (row->values[column_index].type == AMIDB_TYPE_TEXT ||
         row->values[column_index].type == AMIDB_TYPE_BLOB) &&
        row->values[column_index].u.blob.data != NULL) {
        mem_free(row->values[column_index].u.blob.data,
                 row->values[column_index].u.blob.size);
    }

    row->values[column_index].type = AMIDB_TYPE_BLOB;
    row->values[column_index].u.blob.data = blob_data;
    row->values[column_index].u.blob.size = size;

    if (column_index >= row->column_count) {
        row->column_count = column_index + 1;
    }

    return 0;
}

/*
 * Set a NULL value
 */
int row_set_null(struct amidb_row *row, uint32_t column_index) {
    if (!row || column_index >= AMIDB_MAX_COLUMNS) {
        return -1;
    }

    /* Clear any existing value */
    if (column_index < row->column_count &&
        (row->values[column_index].type == AMIDB_TYPE_TEXT ||
         row->values[column_index].type == AMIDB_TYPE_BLOB) &&
        row->values[column_index].u.blob.data != NULL) {
        mem_free(row->values[column_index].u.blob.data,
                 row->values[column_index].u.blob.size);
    }

    row->values[column_index].type = AMIDB_TYPE_NULL;
    row->values[column_index].u.blob.data = NULL;
    row->values[column_index].u.blob.size = 0;

    if (column_index >= row->column_count) {
        row->column_count = column_index + 1;
    }

    return 0;
}

/*
 * Get column value
 */
const struct amidb_value *row_get_value(const struct amidb_row *row, uint32_t column_index) {
    if (!row || column_index >= row->column_count) {
        return NULL;
    }

    return &row->values[column_index];
}

/*
 * Get serialized size of a row
 */
uint32_t row_get_serialized_size(const struct amidb_row *row) {
    uint32_t size;
    uint32_t i;

    if (!row) {
        return 0;
    }

    /* 2 bytes for column count */
    size = 2;

    for (i = 0; i < row->column_count; i++) {
        /* 1 byte for type */
        size += 1;

        switch (row->values[i].type) {
            case AMIDB_TYPE_NULL:
                /* No additional data */
                break;

            case AMIDB_TYPE_INTEGER:
                /* 4 bytes for integer value */
                size += 4;
                break;

            case AMIDB_TYPE_TEXT:
            case AMIDB_TYPE_BLOB:
                /* 4 bytes for size + actual data */
                size += 4 + row->values[i].u.blob.size;
                break;

            default:
                /* Unknown type */
                break;
        }
    }

    return size;
}

/*
 * Serialize a row to binary format
 */
int row_serialize(const struct amidb_row *row, uint8_t *buffer, uint32_t buffer_size) {
    uint32_t offset = 0;
    uint32_t i;

    if (!row || !buffer) {
        return -1;
    }

    /* Check if buffer is large enough */
    if (buffer_size < row_get_serialized_size(row)) {
        return -1;
    }

    /* Write column count */
    put_u16(buffer + offset, (uint16_t)row->column_count);
    offset += 2;

    /* Write each column */
    for (i = 0; i < row->column_count; i++) {
        /* Write type */
        buffer[offset++] = row->values[i].type;

        switch (row->values[i].type) {
            case AMIDB_TYPE_NULL:
                /* No additional data */
                break;

            case AMIDB_TYPE_INTEGER:
                /* Write integer value */
                put_u32(buffer + offset, (uint32_t)row->values[i].u.i);
                offset += 4;
                break;

            case AMIDB_TYPE_TEXT:
            case AMIDB_TYPE_BLOB:
                /* Write size */
                put_u32(buffer + offset, row->values[i].u.blob.size);
                offset += 4;

                /* Write data */
                if (row->values[i].u.blob.size > 0 && row->values[i].u.blob.data) {
                    memcpy(buffer + offset, row->values[i].u.blob.data,
                           row->values[i].u.blob.size);
                    offset += row->values[i].u.blob.size;
                }
                break;

            default:
                /* Unknown type - error */
                return -1;
        }
    }

    return (int)offset;
}

/*
 * Deserialize a row from binary format
 */
int row_deserialize(struct amidb_row *row, const uint8_t *buffer, uint32_t buffer_size) {
    uint32_t offset = 0;
    uint32_t i;
    uint16_t column_count;
    uint8_t type;
    uint32_t size;
    int32_t int_val;

    if (!row || !buffer || buffer_size < 2) {
        return -1;
    }

    /* Clear existing row */
    row_clear(row);

    /* Read column count */
    column_count = get_u16(buffer + offset);
    offset += 2;

    if (column_count > AMIDB_MAX_COLUMNS) {
        return -1;
    }

    /* Read each column */
    for (i = 0; i < column_count; i++) {
        if (offset >= buffer_size) {
            row_clear(row);
            return -1;
        }

        /* Read type */
        type = buffer[offset++];

        switch (type) {
            case AMIDB_TYPE_NULL:
                if (row_set_null(row, i) != 0) {
                    row_clear(row);
                    return -1;
                }
                break;

            case AMIDB_TYPE_INTEGER:
                if (offset + 4 > buffer_size) {
                    row_clear(row);
                    return -1;
                }

                int_val = (int32_t)get_u32(buffer + offset);
                offset += 4;

                if (row_set_int(row, i, int_val) != 0) {
                    row_clear(row);
                    return -1;
                }
                break;

            case AMIDB_TYPE_TEXT:
            case AMIDB_TYPE_BLOB:
                if (offset + 4 > buffer_size) {
                    row_clear(row);
                    return -1;
                }

                size = get_u32(buffer + offset);
                offset += 4;

                if (offset + size > buffer_size) {
                    row_clear(row);
                    return -1;
                }

                if (type == AMIDB_TYPE_TEXT) {
                    if (row_set_text(row, i, (const char *)(buffer + offset), size) != 0) {
                        row_clear(row);
                        return -1;
                    }
                } else {
                    if (row_set_blob(row, i, buffer + offset, size) != 0) {
                        row_clear(row);
                        return -1;
                    }
                }

                offset += size;
                break;

            default:
                /* Unknown type */
                row_clear(row);
                return -1;
        }
    }

    return (int)offset;
}
