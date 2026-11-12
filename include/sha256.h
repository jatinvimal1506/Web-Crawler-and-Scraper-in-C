#ifndef DEEPDOWNLOAD_SHA256_H
#define DEEPDOWNLOAD_SHA256_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif


#define SHA256_MAX_FILE_BYTES ((size_t)20 * 1024 * 1024)

typedef enum {
    SHA256_OK = 0,
    SHA256_ERROR_INVALID_ARGUMENT,
    SHA256_ERROR_OPEN,
    SHA256_ERROR_READ,
    SHA256_ERROR_CLOSE,
    SHA256_ERROR_OVERFLOW,
    SHA256_ERROR_SIZE_LIMIT
} Sha256Status;

const char *sha256_status_message(Sha256Status status);
Sha256Status sha256_file(const char *path, char hex_out[65],size_t *bytes_read);
// calculates a SHA-256 hash of the downloaded file.
// A SHA-256 hash is a fingerprint generated from the file's content.
#ifdef __cplusplus
}
#endif

#endif
