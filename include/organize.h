#ifndef DEEPDOWNLOAD_ORGANIZE_H
#define DEEPDOWNLOAD_ORGANIZE_H

#include "document.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ORGANIZE_MAX_FILE_BYTES ((size_t)20 * 1024 * 1024)

typedef enum {
    ORGANIZER_OK = 0,
    ORGANIZER_NEW_FILE,
    ORGANIZER_DUPLICATE,
    ORGANIZER_ERROR_INVALID_ARGUMENT,
    ORGANIZER_ERROR_INVALID_TYPE,
    ORGANIZER_ERROR_OUT_OF_MEMORY,
    ORGANIZER_ERROR_FILESYSTEM,
    ORGANIZER_ERROR_READ,
    ORGANIZER_ERROR_WRITE,
    ORGANIZER_ERROR_CLOSE,
    ORGANIZER_ERROR_EMPTY,
    ORGANIZER_ERROR_TOO_LARGE,
    ORGANIZER_ERROR_CONFLICT,
    ORGANIZER_ERROR_OVERFLOW,
    ORGANIZER_ERROR_CLEANUP,
    ORGANIZER_ERROR_VERIFY
} OrganizerStatus;

typedef struct {
    OrganizerStatus status;
    int is_duplicate;
    DocumentType stored_type;
    char sha256_hex[65];
    size_t bytes;
    char *stored_path; /* Owned by this result on success/cleanup failure. */
    char detail[256];
} OrganizeResult;

const char *organizer_status_message(OrganizerStatus status);

/* Initialize a zeroed result; no allocation is performed. */
OrganizerStatus organize_result_init(OrganizeResult *result);

/* NULL-safe and repeatable; frees stored_path and resets the result. */
void organize_result_free(OrganizeResult *result);

/*
 * Require an existing trusted output root and create/verify only its immediate
 * pdf/, docx/ and zip/ directories. Existing user files are never removed.
 * On POSIX, a symlink at the root or any of those child names is rejected.
 * Windows junction/reparse-point detection is not guaranteed.
 */
OrganizerStatus organizer_prepare_directories(const char *output_root);

/*
 * Hash and organize one completed Phase 9 file. The input file and output root
 * are borrowed and must remain valid for this call. The input must be a
 * nonempty regular file no larger than 20 MiB and the output root must already
 * exist. A unique file is exclusively created as
 * <output-root>/<type>/<64-lowercase-hex-digest>.<extension>.
 *
 * Identical bytes are deduplicated across all three type folders. The result
 * owns stored_path on NEW_FILE, DUPLICATE, or CLEANUP error. The organizer
 * never removes or modifies the caller's input file. Initialize result as
 * {0}; do not shallow-copy it after it owns memory.
 */
OrganizerStatus organize_file(const char *completed_file_path,
                             DocumentType type,
                             const char *output_root,
                             OrganizeResult *result);

#ifdef __cplusplus
}
#endif

#endif
