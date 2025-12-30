/*
 * error.h - Error handling and context for AmiDB
 *
 * Provides detailed error information with file/line tracking
 * for debugging without a debugger on AmigaOS.
 */

#ifndef AMIDB_ERROR_H
#define AMIDB_ERROR_H

#include <exec/types.h>

/* Error codes */
#define AMIDB_OK          0
#define AMIDB_ERROR      -1
#define AMIDB_BUSY       -2
#define AMIDB_NOTFOUND   -3
#define AMIDB_EXISTS     -4
#define AMIDB_CORRUPT    -5
#define AMIDB_NOMEM      -6
#define AMIDB_IOERR      -7
#define AMIDB_FULL       -8
#define AMIDB_SYNTAX     -9
#define AMIDB_DONE       -10
#define AMIDB_ROW        -11
#define AMIDB_OVERFLOW   -12

/* Error context structure */
struct amidb_error_context {
    int code;                  /* AMIDB_xxx error code */
    char message[256];         /* Human-readable message */
    const char *file;          /* Source file where error occurred */
    int line;                  /* Line number */
};

/* Forward declaration of amidb structure */
struct amidb;

/* Set error with context (use SET_ERROR macro instead) */
void _amidb_set_error(struct amidb *db, int code, const char *file,
                      int line, const char *fmt, ...);

/* Set error with context macro */
#define SET_ERROR(db, code, ...) \
    _amidb_set_error(db, code, __FILE__, __LINE__, __VA_ARGS__)

/* Get error message */
const char *amidb_error_message(const struct amidb_error_context *ctx);

/* Get error code name (for debugging) */
const char *amidb_error_name(int code);

#endif /* AMIDB_ERROR_H */
