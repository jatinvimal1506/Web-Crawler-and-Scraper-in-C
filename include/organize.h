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
    char *stored_path;
    char detail[256];
} OrganizeResult;

const char *organizer_status_message(OrganizerStatus status);
OrganizerStatus organize_result_init(OrganizeResult *result);
void organize_result_free(OrganizeResult *result);
OrganizerStatus organizer_prepare_directories(const char *output_root);
OrganizerStatus organize_file(const char *completed_file_path,
                             DocumentType type,
                             const char *output_root,
                             OrganizeResult *result);

#ifdef __cplusplus
}
#endif

#endif
