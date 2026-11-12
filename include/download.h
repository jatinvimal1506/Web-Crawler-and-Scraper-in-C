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
    const char *document_url;
    const char *seed_url;
    int same_domain;
    long timeout_seconds;
    const char *output_directory;
    const char *filename;
} DownloadConfig;

typedef struct {
    DownloadStatus status;
    DocumentType type;
    long http_status;
    size_t bytes_written;
    char *final_url;
    char *file_path;
    char detail[256];
} DownloadResult;


DownloadStatus download_result_init(DownloadResult *result);

void download_result_free(DownloadResult *result);
const char *download_status_message(DownloadStatus status);















DownloadStatus download_document(const DownloadConfig *config,
                                 DownloadResult *result);

#ifdef __cplusplus
}
#endif
#endif
