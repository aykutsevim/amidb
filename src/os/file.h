/*
 * file.h - Portable file I/O interface for AmiDB
 *
 * Provides a thin abstraction over AmigaOS dos.library file operations.
 */

#ifndef AMIDB_FILE_H
#define AMIDB_FILE_H

#include <stdint.h>

/* Opaque file handle */
typedef void* amidb_file_t;

/* Seek origins */
#define AMIDB_SEEK_SET 0
#define AMIDB_SEEK_CUR 1
#define AMIDB_SEEK_END 2

/* Open modes */
#define AMIDB_O_RDONLY 0x01
#define AMIDB_O_RDWR   0x02
#define AMIDB_O_CREATE 0x04
#define AMIDB_O_TRUNC  0x08

/* Error codes */
#define AMIDB_FILE_OK      0
#define AMIDB_FILE_ERROR  -1

/* Open a file */
amidb_file_t file_open(const char *path, uint32_t mode);

/* Close a file */
void file_close(amidb_file_t file);

/* Read from file */
int32_t file_read(amidb_file_t file, void *buf, uint32_t length);

/* Write to file */
int32_t file_write(amidb_file_t file, const void *buf, uint32_t length);

/* Seek in file */
int32_t file_seek(amidb_file_t file, int32_t offset, int whence);

/* Get current file position */
int32_t file_tell(amidb_file_t file);

/* Sync file to disk (flush buffers) */
int file_sync(amidb_file_t file);

/* Get file size */
int32_t file_size(amidb_file_t file);

/* Truncate file to specified size */
int file_truncate(amidb_file_t file, uint32_t size);

/* Delete a file */
int file_delete(const char *path);

/* Check if file exists */
int file_exists(const char *path);

#endif /* AMIDB_FILE_H */
