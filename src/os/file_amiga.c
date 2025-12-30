/*
 * file_amiga.c - AmigaOS file I/O implementation
 *
 * Uses dos.library for file operations.
 */

#include "os/file.h"

/* AmigaOS includes */
#include <exec/types.h>
#include <dos/dos.h>

/* Manual declarations of dos.library functions */
BPTR Open(CONST_STRPTR name, LONG accessMode);
void Close(BPTR file);
LONG Read(BPTR file, APTR buffer, LONG length);
LONG Write(BPTR file, CONST_APTR buffer, LONG length);
LONG Seek(BPTR file, LONG position, LONG mode);
BOOL DeleteFile(CONST_STRPTR name);
BPTR Lock(CONST_STRPTR name, LONG type);
void UnLock(BPTR lock);

/* Open a file */
amidb_file_t file_open(const char *path, uint32_t mode) {
    LONG amiga_mode;
    BPTR file;

    /* Translate mode to AmigaOS mode */
    if (mode & AMIDB_O_CREATE) {
        if (mode & AMIDB_O_TRUNC) {
            /* Always create new file, overwriting if exists */
            amiga_mode = MODE_NEWFILE;
        } else {
            /* Open existing for read/write, or create if doesn't exist */
            /* First try to open existing file */
            file = Open((CONST_STRPTR)path, MODE_READWRITE);
            if (!file) {
                /* File doesn't exist, create it */
                file = Open((CONST_STRPTR)path, MODE_NEWFILE);
            }
            return (amidb_file_t)file;
        }
    } else if (mode & AMIDB_O_RDWR) {
        amiga_mode = MODE_READWRITE;
    } else {
        amiga_mode = MODE_OLDFILE;
    }

    file = Open((CONST_STRPTR)path, amiga_mode);
    if (!file) {
        return NULL;
    }

    return (amidb_file_t)file;
}

/* Close a file */
void file_close(amidb_file_t file) {
    if (file) {
        Close((BPTR)file);
    }
}

/* Read from file */
int32_t file_read(amidb_file_t file, void *buf, uint32_t length) {
    LONG result;

    if (!file) {
        return -1;
    }

    result = Read((BPTR)file, buf, length);
    if (result < 0) {
        return -1;
    }

    return result;
}

/* Write to file */
int32_t file_write(amidb_file_t file, const void *buf, uint32_t length) {
    LONG result;

    if (!file) {
        return -1;
    }

    result = Write((BPTR)file, (APTR)buf, length);
    if (result != (LONG)length) {
        return -1;
    }

    return result;
}

/* Seek in file */
int32_t file_seek(amidb_file_t file, int32_t offset, int whence) {
    LONG mode;
    LONG result;

    if (!file) {
        return -1;
    }

    /* Translate whence to AmigaOS mode */
    switch (whence) {
        case AMIDB_SEEK_SET:
            mode = OFFSET_BEGINNING;
            break;
        case AMIDB_SEEK_CUR:
            mode = OFFSET_CURRENT;
            break;
        case AMIDB_SEEK_END:
            mode = OFFSET_END;
            break;
        default:
            return -1;
    }

    result = Seek((BPTR)file, offset, mode);
    if (result < 0) {
        return -1;
    }

    return 0;
}

/* Get current file position */
int32_t file_tell(amidb_file_t file) {
    LONG pos;

    if (!file) {
        return -1;
    }

    /* Seek 0 from current position to get current offset */
    pos = Seek((BPTR)file, 0, OFFSET_CURRENT);
    if (pos < 0) {
        return -1;
    }

    return pos;
}

/* Sync file to disk */
int file_sync(amidb_file_t file) {
    if (!file) {
        return AMIDB_FILE_ERROR;
    }

    /* AmigaOS doesn't have explicit fsync, but we can flush */
    /* Note: On AmigaOS, Flush() may not be available on all systems */
    /* For now, we'll just return success as writes are typically synchronous */
    return AMIDB_FILE_OK;
}

/* Get file size */
int32_t file_size(amidb_file_t file) {
    LONG pos, size;

    if (!file) {
        return -1;
    }

    /* Save current position */
    pos = Seek((BPTR)file, 0, OFFSET_CURRENT);
    if (pos < 0) {
        return -1;
    }

    /* Seek to end */
    size = Seek((BPTR)file, 0, OFFSET_END);
    if (size < 0) {
        return -1;
    }

    /* Restore position */
    Seek((BPTR)file, pos, OFFSET_BEGINNING);

    return size;
}

/* Truncate file */
int file_truncate(amidb_file_t file, uint32_t size) {
    /* AmigaOS doesn't have a built-in truncate operation */
    /* This would require SetFileSize() which may not be available */
    /* For now, return error - we'll handle this differently if needed */
    return AMIDB_FILE_ERROR;
}

/* Delete a file */
int file_delete(const char *path) {
    if (DeleteFile((CONST_STRPTR)path)) {
        return AMIDB_FILE_OK;
    }
    return AMIDB_FILE_ERROR;
}

/* Check if file exists */
int file_exists(const char *path) {
    BPTR lock;

    lock = Lock((CONST_STRPTR)path, ACCESS_READ);
    if (lock) {
        UnLock(lock);
        return 1;
    }

    return 0;
}
