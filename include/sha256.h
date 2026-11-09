#ifndef DEEPDOWNLOAD_SHA256_H
#define DEEPDOWNLOAD_SHA256_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Keep file hashing within the same per-document bound as Phase 9. */
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

/*
 * Hash one file incrementally using a fixed-size buffer.
 *
 * path is borrowed. hex_out must point to at least 65 bytes and receives
 * exactly 64 lowercase hexadecimal digits plus NUL on success. bytes_read
 * is optional. Outputs are cleared on failure. Empty files are valid here;
 * the Phase 10 organizer rejects them as documents.
 */
Sha256Status sha256_file(const char *path, char hex_out[65],
                         size_t *bytes_read);

#ifdef __cplusplus
}
#endif

#endif
