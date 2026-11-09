#ifndef DEEPDOWNLOAD_DOWNLOAD_H
#define DEEPDOWNLOAD_DOWNLOAD_H

#include "document.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DOWNLOAD_MAX_BYTES ((size_t)20 * 1024 * 1024)
#define DOWNLOAD_MAX_FILENAME_BYTES ((size_t)128)
#define DOWNLOAD_MAX_TIMEOUT_SECONDS 3600L

typedef enum {
    DOWNLOAD_OK = 0,
    DOWNLOAD_ERROR_INVALID_ARGUMENT,
    DOWNLOAD_ERROR_UNSUPPORTED_TYPE,
    DOWNLOAD_ERROR_OUT_OF_MEMORY,
    DOWNLOAD_ERROR_INIT,
    DOWNLOAD_ERROR_TIMEOUT,
    DOWNLOAD_ERROR_NETWORK,
    DOWNLOAD_ERROR_REDIRECT,
    DOWNLOAD_ERROR_SCOPE,
    DOWNLOAD_ERROR_HTTP_STATUS,
    DOWNLOAD_ERROR_CONTENT_TYPE,
    DOWNLOAD_ERROR_TOO_LARGE,
    DOWNLOAD_ERROR_EMPTY,
    DOWNLOAD_ERROR_INCOMPLETE,
    DOWNLOAD_ERROR_EXISTS,
    DOWNLOAD_ERROR_WRITE,
    DOWNLOAD_ERROR_FILE,
    DOWNLOAD_ERROR_CLEANUP,
    DOWNLOAD_ERROR_OVERFLOW
} DownloadStatus;

typedef struct {
    const char *document_url;       /* Borrowed absolute HTTP(S) URL. */
    const char *seed_url;           /* Borrowed URL defining redirect scope. */
    int same_domain;                /* 1 pins exact seed host; both modes pin port. */
    long timeout_seconds;            /* 1..3600 seconds per transfer hop. */
    const char *output_directory;    /* Borrowed; must already exist. */
    const char *filename;            /* Borrowed safe basename, not a path. */
} DownloadConfig;

typedef struct {
    DownloadStatus status;
    DocumentType type;
    long http_status;                /* Last response code, or zero if none. */
    size_t bytes_written;             /* Successfully written bytes, even on failure. */
    char *final_url;                  /* Owned copy of last fetched effective URL. */
    char *file_path;                  /* Owned on success or cleanup failure only. */
    char detail[256];                 /* Short diagnostic; part of this structure. */
} DownloadResult;

/* Initialize only a zeroed/empty result; this function does not allocate. */
DownloadStatus download_result_init(DownloadResult *result);
/* NULL-safe and repeatable; frees final_url/file_path and resets all fields. */
void download_result_free(DownloadResult *result);
const char *download_status_message(DownloadStatus status); /* Static string. */

/*
 * Stream one recognized Phase 8 document to a new, exclusively-created file.
 * The destination directory must exist. The basename is restricted to ASCII
 * letters, digits, dot, dash and underscore; its extension must match the URL.
 * Existing files are never overwritten.
 *
 * The caller supplies an empty zero-initialized result and owns all inputs for
 * the duration of the call. On success, final_url and file_path are owned by
 * result and bytes_written is the complete file size. On ordinary failure,
 * a newly created partial file is removed and file_path is NULL; on a cleanup
 * error, file_path remains available to identify the file that could not be
 * removed. final_url/http_status/bytes_written are retained when known.
 * Always call download_result_free().
 */
DownloadStatus download_document(const DownloadConfig *config,
                                 DownloadResult *result);

#ifdef __cplusplus
}
#endif
#endif
