/*
 * error.c - Error handling implementation for AmiDB
 */

#include "api/error.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* Temporary forward declaration - will be properly defined in amidb.h later */
struct amidb {
    struct amidb_error_context error;
    /* ... other fields ... */
};

/* Set error with context */
void _amidb_set_error(struct amidb *db, int code, const char *file,
                      int line, const char *fmt, ...) {
    va_list args;

    if (!db) {
        return;
    }

    db->error.code = code;
    db->error.file = file;
    db->error.line = line;

    /* Format message */
    va_start(args, fmt);
    vsnprintf(db->error.message, sizeof(db->error.message), fmt, args);
    va_end(args);
}

/* Get error message */
const char *amidb_error_message(const struct amidb_error_context *ctx) {
    if (!ctx) {
        return "No error context";
    }
    return ctx->message;
}

/* Get error code name */
const char *amidb_error_name(int code) {
    switch (code) {
        case AMIDB_OK:       return "AMIDB_OK";
        case AMIDB_ERROR:    return "AMIDB_ERROR";
        case AMIDB_BUSY:     return "AMIDB_BUSY";
        case AMIDB_NOTFOUND: return "AMIDB_NOTFOUND";
        case AMIDB_EXISTS:   return "AMIDB_EXISTS";
        case AMIDB_CORRUPT:  return "AMIDB_CORRUPT";
        case AMIDB_NOMEM:    return "AMIDB_NOMEM";
        case AMIDB_IOERR:    return "AMIDB_IOERR";
        case AMIDB_FULL:     return "AMIDB_FULL";
        case AMIDB_SYNTAX:   return "AMIDB_SYNTAX";
        case AMIDB_DONE:     return "AMIDB_DONE";
        case AMIDB_ROW:      return "AMIDB_ROW";
        case AMIDB_OVERFLOW: return "AMIDB_OVERFLOW";
        default:             return "UNKNOWN";
    }
}
