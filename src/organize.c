#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
#include "organize.h"

#include "sha256.h"

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <io.h>
#include <direct.h>
#define ORG_OPEN _open
#define ORG_CLOSE _close
#define ORG_FDOPEN _fdopen
#define ORG_FSTAT _fstat
#define ORG_STAT _stat
#define ORG_MKDIR(path) _mkdir(path)
#define ORG_BINARY_FLAG _O_BINARY
#define ORG_READ_FLAG _O_RDONLY
#define ORG_WRITE_FLAGS (_O_WRONLY | _O_CREAT | _O_EXCL | _O_BINARY)
#define ORG_FILE_MODE (_S_IREAD | _S_IWRITE)
#define ORG_SEPARATOR '\\'
typedef struct _stat OrgStat;
#define ORG_IS_REGULAR(mode) (((mode) & _S_IFMT) == _S_IFREG)
#define ORG_IS_DIRECTORY(mode) (((mode) & _S_IFMT) == _S_IFDIR)
#else
#include <unistd.h>
#define ORG_OPEN open
#define ORG_CLOSE close
#define ORG_FDOPEN fdopen
#define ORG_FSTAT fstat
#define ORG_STAT stat
#define ORG_MKDIR(path) mkdir((path), 0700)
#define ORG_BINARY_FLAG 0
#define ORG_READ_FLAG O_RDONLY
#define ORG_WRITE_FLAGS (O_WRONLY | O_CREAT | O_EXCL)
#define ORG_FILE_MODE 0600
#define ORG_SEPARATOR '/'
typedef struct stat OrgStat;
#define ORG_IS_REGULAR(mode) S_ISREG(mode)
#define ORG_IS_DIRECTORY(mode) S_ISDIR(mode)
#endif

#define ORGANIZE_COPY_BYTES 16384U

typedef enum {
    PATH_MISSING = 0,
    PATH_REGULAR,
    PATH_DIRECTORY,
    PATH_OTHER,
    PATH_ERROR
} PathKind;

static const char *const type_directories[3] = {"pdf", "docx", "zip"};
static const char *const type_extensions[3] = {".pdf", ".docx", ".zip"};

static int result_is_empty(const OrganizeResult *result)
{
    return result->status == ORGANIZER_OK && result->is_duplicate == 0 &&
           result->stored_type == DOCUMENT_TYPE_UNKNOWN &&
           result->sha256_hex[0] == '\0' && result->bytes == 0 &&
           result->stored_path == NULL && result->detail[0] == '\0';
}

static void set_detail(OrganizeResult *result, OrganizerStatus status,
                       const char *detail)
{
    result->status = status;
    if (detail == NULL) { detail = organizer_status_message(status); }
    (void)snprintf(result->detail, sizeof(result->detail), "%s", detail);
}

const char *organizer_status_message(OrganizerStatus status)
{
    switch (status) {
    case ORGANIZER_OK: return "organization directories are ready";
    case ORGANIZER_NEW_FILE: return "new unique file stored";
    case ORGANIZER_DUPLICATE: return "identical file already stored";
    case ORGANIZER_ERROR_INVALID_ARGUMENT: return "invalid organizer argument";
    case ORGANIZER_ERROR_INVALID_TYPE: return "unsupported document type";
    case ORGANIZER_ERROR_OUT_OF_MEMORY: return "out of memory";
    case ORGANIZER_ERROR_FILESYSTEM: return "filesystem path is missing or unsafe";
    case ORGANIZER_ERROR_READ: return "could not read input or existing file";
    case ORGANIZER_ERROR_WRITE: return "could not write destination file";
    case ORGANIZER_ERROR_CLOSE: return "could not close a file cleanly";
    case ORGANIZER_ERROR_EMPTY: return "empty documents cannot be organized";
    case ORGANIZER_ERROR_TOO_LARGE: return "document exceeds the 20 MiB limit";
    case ORGANIZER_ERROR_CONFLICT: return "canonical digest path has conflicting contents";
    case ORGANIZER_ERROR_OVERFLOW: return "file size or path calculation overflow";
    case ORGANIZER_ERROR_CLEANUP: return "failed to remove an incomplete destination";
    case ORGANIZER_ERROR_VERIFY: return "stored copy did not match its source digest";
    default: return "unknown organizer error";
    }
}

OrganizerStatus organize_result_init(OrganizeResult *result)
{
    if (result == NULL || !result_is_empty(result)) {
        return ORGANIZER_ERROR_INVALID_ARGUMENT;
    }
    *result = (OrganizeResult){0};
    return ORGANIZER_OK;
}

void organize_result_free(OrganizeResult *result)
{
    if (result == NULL) { return; }
    free(result->stored_path);
    *result = (OrganizeResult){0};
}

static PathKind inspect_path(const char *path, OrgStat *info)
{
#ifdef _WIN32
    if (ORG_STAT(path, info) != 0) {
#else
    if (lstat(path, info) != 0) {
#endif
        if (errno == ENOENT || errno == ENOTDIR) { return PATH_MISSING; }
        return PATH_ERROR;
    }
    if (ORG_IS_REGULAR(info->st_mode)) { return PATH_REGULAR; }
    if (ORG_IS_DIRECTORY(info->st_mode)) { return PATH_DIRECTORY; }
    return PATH_OTHER;
}

static int path_has_separator(const char *path, size_t length)
{
    if (length == 0) { return 0; }
    if (path[length - 1] == ORG_SEPARATOR) { return 1; }
#ifdef _WIN32
    if (path[length - 1] == '/') { return 1; }
#endif
    return 0;
}

static OrganizerStatus join_path(const char *directory, const char *name,
                                 char **output)
{
    size_t directory_length, name_length, separator, total;
    char *path;

    if (directory == NULL || name == NULL || output == NULL) {
        return ORGANIZER_ERROR_INVALID_ARGUMENT;
    }
    *output = NULL;
    directory_length = strlen(directory);
    name_length = strlen(name);
    separator = path_has_separator(directory, directory_length) ? 0U : 1U;
    if (name_length > SIZE_MAX - separator - 1U ||
        directory_length > SIZE_MAX - name_length - separator - 1U) {
        return ORGANIZER_ERROR_OVERFLOW;
    }
    total = directory_length + separator + name_length + 1U;
    path = malloc(total);
    if (path == NULL) { return ORGANIZER_ERROR_OUT_OF_MEMORY; }
    memcpy(path, directory, directory_length);
    if (separator != 0) { path[directory_length] = ORG_SEPARATOR; }
    memcpy(path + directory_length + separator, name, name_length + 1U);
    *output = path;
    return ORGANIZER_OK;
}

static OrganizerStatus ensure_directory(const char *path)
{
    OrgStat info;
    PathKind kind = inspect_path(path, &info);
    if (kind == PATH_DIRECTORY) { return ORGANIZER_OK; }
    if (kind != PATH_MISSING) { return ORGANIZER_ERROR_FILESYSTEM; }

    if (ORG_MKDIR(path) == 0) {
        kind = inspect_path(path, &info);
        return kind == PATH_DIRECTORY ? ORGANIZER_OK
                                      : ORGANIZER_ERROR_FILESYSTEM;
    }
    if (errno == EEXIST) {
        kind = inspect_path(path, &info);
        if (kind == PATH_DIRECTORY) { return ORGANIZER_OK; }
    }
    return ORGANIZER_ERROR_FILESYSTEM;
}

OrganizerStatus organizer_prepare_directories(const char *output_root)
{
    OrgStat root_info;
    char *child_path = NULL;
    OrganizerStatus status;
    size_t index;

    if (output_root == NULL || output_root[0] == '\0') {
        return ORGANIZER_ERROR_INVALID_ARGUMENT;
    }
    if (inspect_path(output_root, &root_info) != PATH_DIRECTORY) {
        return ORGANIZER_ERROR_FILESYSTEM;
    }

    for (index = 0; index < 3U; ++index) {
        status = join_path(output_root, type_directories[index], &child_path);
        if (status != ORGANIZER_OK) { return status; }
        status = ensure_directory(child_path);
        free(child_path);
        child_path = NULL;
        if (status != ORGANIZER_OK) { return status; }
    }
    return ORGANIZER_OK;
}

static int type_index(DocumentType type)
{
    switch (type) {
    case DOCUMENT_TYPE_PDF: return 0;
    case DOCUMENT_TYPE_DOCX: return 1;
    case DOCUMENT_TYPE_ZIP: return 2;
    default: return -1;
    }
}

static int directory_index_type(size_t index)
{
    return index == 0U ? DOCUMENT_TYPE_PDF
         : index == 1U ? DOCUMENT_TYPE_DOCX
         : DOCUMENT_TYPE_ZIP;
}

static int open_flags_read(void)
{
    int flags = ORG_READ_FLAG | ORG_BINARY_FLAG;
#ifndef _WIN32
#ifdef O_NOFOLLOW
    flags |= O_NOFOLLOW;
#endif
#ifdef O_NONBLOCK
    flags |= O_NONBLOCK;
#endif
#endif
    return flags;
}

static FILE *open_regular_file(const char *path, OrganizerStatus *status)
{
    OrgStat path_info, opened_info;
    PathKind kind = inspect_path(path, &path_info);
    int descriptor, flags;
    FILE *file;

    if (kind != PATH_REGULAR) {
        *status = ORGANIZER_ERROR_FILESYSTEM;
        return NULL;
    }
    flags = open_flags_read();
    descriptor = ORG_OPEN(path, flags, ORG_FILE_MODE);
    if (descriptor < 0) {
        *status = errno == EEXIST ? ORGANIZER_ERROR_CONFLICT
                                  : ORGANIZER_ERROR_FILESYSTEM;
        return NULL;
    }
    if (ORG_FSTAT(descriptor, &opened_info) != 0 ||
        !ORG_IS_REGULAR(opened_info.st_mode)) {
        (void)ORG_CLOSE(descriptor);
        *status = ORGANIZER_ERROR_FILESYSTEM;
        return NULL;
    }
    file = ORG_FDOPEN(descriptor, "rb");
    if (file == NULL) {
        (void)ORG_CLOSE(descriptor);
        *status = ORGANIZER_ERROR_FILESYSTEM;
        return NULL;
    }
    *status = ORGANIZER_OK;
    return file;
}

static OrganizerStatus compare_files(const char *left_path,
                                     const char *right_path,
                                     size_t expected_size, int *equal)
{
    unsigned char left_buffer[ORGANIZE_COPY_BYTES];
    unsigned char right_buffer[ORGANIZE_COPY_BYTES];
    OrganizerStatus status = ORGANIZER_OK;
    FILE *left = NULL, *right = NULL;
    OrgStat left_info, right_info;
    int close_left_failed = 0, close_right_failed = 0;

    *equal = 0;
    if (inspect_path(left_path, &left_info) != PATH_REGULAR ||
        inspect_path(right_path, &right_info) != PATH_REGULAR) {
        return ORGANIZER_ERROR_FILESYSTEM;
    }
    if (left_info.st_size < 0 || right_info.st_size < 0) {
        return ORGANIZER_ERROR_OVERFLOW;
    }
    if ((uintmax_t)left_info.st_size > (uintmax_t)SIZE_MAX ||
        (uintmax_t)right_info.st_size > (uintmax_t)SIZE_MAX) {
        return ORGANIZER_ERROR_OVERFLOW;
    }
    if ((size_t)left_info.st_size != expected_size ||
        (size_t)right_info.st_size != expected_size) {
        return ORGANIZER_OK;
    }

    left = open_regular_file(left_path, &status);
    if (left == NULL) { return status; }
    right = open_regular_file(right_path, &status);
    if (right == NULL) {
        if (fclose(left) != 0 && status == ORGANIZER_OK) {
            status = ORGANIZER_ERROR_CLOSE;
        }
        return status;
    }

    for (;;) {
        size_t left_count = fread(left_buffer, 1, sizeof(left_buffer), left);
        size_t right_count = fread(right_buffer, 1, sizeof(right_buffer), right);
        if (left_count != right_count) {
            status = (ferror(left) || ferror(right))
                ? ORGANIZER_ERROR_READ : ORGANIZER_OK;
            break;
        }
        if (left_count == 0) {
            if (ferror(left) || ferror(right) ||
                !feof(left) || !feof(right)) {
                status = ORGANIZER_ERROR_READ;
            } else {
                *equal = 1;
            }
            break;
        }
        if (memcmp(left_buffer, right_buffer, left_count) != 0) {
            break;
        }
    }

    if (fclose(left) != 0) { close_left_failed = 1; }
    if (fclose(right) != 0) { close_right_failed = 1; }
    if ((close_left_failed || close_right_failed) && status == ORGANIZER_OK) {
        status = ORGANIZER_ERROR_CLOSE;
        *equal = 0;
    }
    return status;
}

static OrganizerStatus map_sha_status(Sha256Status status)
{
    switch (status) {
    case SHA256_OK: return ORGANIZER_OK;
    case SHA256_ERROR_INVALID_ARGUMENT: return ORGANIZER_ERROR_INVALID_ARGUMENT;
    case SHA256_ERROR_OVERFLOW: return ORGANIZER_ERROR_OVERFLOW;
    case SHA256_ERROR_SIZE_LIMIT: return ORGANIZER_ERROR_TOO_LARGE;
    case SHA256_ERROR_CLOSE: return ORGANIZER_ERROR_CLOSE;
    case SHA256_ERROR_OPEN:
    case SHA256_ERROR_READ:
    default: return ORGANIZER_ERROR_READ;
    }
}

static void cleanup_candidate_paths(char *paths[3])
{
    size_t index;
    for (index = 0; index < 3U; ++index) { free(paths[index]); }
}

static OrganizerStatus make_digest_name(const char digest[65], size_t index,
                                        char name[70])
{
    int written;
    written = snprintf(name, 70U, "%s%s", digest, type_extensions[index]);
    if (written < 0 || written >= 70) { return ORGANIZER_ERROR_OVERFLOW; }
    return ORGANIZER_OK;
}

OrganizerStatus organize_file(const char *completed_file_path,
                             DocumentType type,
                             const char *output_root,
                             OrganizeResult *result)
{
    char digest[65] = {0};
    size_t bytes = 0, index;
    char *candidate_paths[3] = {NULL, NULL, NULL};
    int selected_index;
    OrganizerStatus status;
    Sha256Status hash_status;
    OrgStat input_info;
    char name[70];

    if (result == NULL || !result_is_empty(result)) {
        return ORGANIZER_ERROR_INVALID_ARGUMENT;
    }
    if (completed_file_path == NULL || completed_file_path[0] == '\0' ||
        output_root == NULL || output_root[0] == '\0') {
        set_detail(result, ORGANIZER_ERROR_INVALID_ARGUMENT, NULL);
        return ORGANIZER_ERROR_INVALID_ARGUMENT;
    }
    selected_index = type_index(type);
    if (selected_index < 0) {
        set_detail(result, ORGANIZER_ERROR_INVALID_TYPE, NULL);
        return ORGANIZER_ERROR_INVALID_TYPE;
    }

    status = organizer_prepare_directories(output_root);
    if (status != ORGANIZER_OK) {
        set_detail(result, status, NULL);
        return status;
    }
    if (inspect_path(completed_file_path, &input_info) != PATH_REGULAR) {
        set_detail(result, ORGANIZER_ERROR_FILESYSTEM, NULL);
        return ORGANIZER_ERROR_FILESYSTEM;
    }
    if (input_info.st_size < 0) {
        set_detail(result, ORGANIZER_ERROR_OVERFLOW, NULL);
        return ORGANIZER_ERROR_OVERFLOW;
    }
    if ((uintmax_t)input_info.st_size > (uintmax_t)SIZE_MAX) {
        set_detail(result, ORGANIZER_ERROR_OVERFLOW, NULL);
        return ORGANIZER_ERROR_OVERFLOW;
    }
    if ((uintmax_t)input_info.st_size > (uintmax_t)ORGANIZE_MAX_FILE_BYTES) {
        set_detail(result, ORGANIZER_ERROR_TOO_LARGE, NULL);
        return ORGANIZER_ERROR_TOO_LARGE;
    }
    if (input_info.st_size == 0) {
        set_detail(result, ORGANIZER_ERROR_EMPTY, NULL);
        return ORGANIZER_ERROR_EMPTY;
    }

    hash_status = sha256_file(completed_file_path, digest, &bytes);
    status = map_sha_status(hash_status);
    if (status != ORGANIZER_OK) {
        set_detail(result, status, sha256_status_message(hash_status));
        return status;
    }
    if (bytes == 0) {
        set_detail(result, ORGANIZER_ERROR_EMPTY, NULL);
        return ORGANIZER_ERROR_EMPTY;
    }
    if (bytes > ORGANIZE_MAX_FILE_BYTES) {
        set_detail(result, ORGANIZER_ERROR_TOO_LARGE, NULL);
        return ORGANIZER_ERROR_TOO_LARGE;
    }

    for (index = 0; index < 3U; ++index) {
        status = make_digest_name(digest, index, name);
        if (status == ORGANIZER_OK) {
            status = join_path(output_root, type_directories[index],
                               &candidate_paths[index]);
        }
        if (status == ORGANIZER_OK) {
            char *file_path = NULL;
            status = join_path(candidate_paths[index], name, &file_path);
            free(candidate_paths[index]);
            candidate_paths[index] = file_path;
        }
        if (status != ORGANIZER_OK) {
            cleanup_candidate_paths(candidate_paths);
            set_detail(result, status, NULL);
            return status;
        }
    }

    for (index = 0; index < 3U; ++index) {
        OrgStat candidate_info;
        PathKind kind = inspect_path(candidate_paths[index], &candidate_info);
        if (kind == PATH_MISSING) { continue; }
        if (kind != PATH_REGULAR) {
            cleanup_candidate_paths(candidate_paths);
            set_detail(result, ORGANIZER_ERROR_CONFLICT, NULL);
            return ORGANIZER_ERROR_CONFLICT;
        }
        {
            int equal = 0;
            status = compare_files(completed_file_path, candidate_paths[index],
                                   bytes, &equal);
            if (status != ORGANIZER_OK) {
                cleanup_candidate_paths(candidate_paths);
                set_detail(result, status, NULL);
                return status;
            }
            if (!equal) {
                cleanup_candidate_paths(candidate_paths);
                set_detail(result, ORGANIZER_ERROR_CONFLICT, NULL);
                return ORGANIZER_ERROR_CONFLICT;
            }
        }
        result->status = ORGANIZER_DUPLICATE;
        result->is_duplicate = 1;
        result->stored_type = (DocumentType)directory_index_type(index);
        memcpy(result->sha256_hex, digest, sizeof(result->sha256_hex));
        result->bytes = bytes;
        result->stored_path = candidate_paths[index];
        candidate_paths[index] = NULL;
        set_detail(result, ORGANIZER_DUPLICATE, NULL);
        cleanup_candidate_paths(candidate_paths);
        return ORGANIZER_DUPLICATE;
    }


    {
        int descriptor;
        FILE *destination;
        int created = 0;
#ifdef _WIN32
        descriptor = ORG_OPEN(candidate_paths[selected_index],
                              ORG_WRITE_FLAGS, ORG_FILE_MODE);
#else
        {
            int flags = ORG_WRITE_FLAGS;
#ifdef O_NOFOLLOW
            flags |= O_NOFOLLOW;
#endif
            descriptor = ORG_OPEN(candidate_paths[selected_index],
                                  flags, ORG_FILE_MODE);
        }
#endif
        if (descriptor < 0) {
            status = errno == EEXIST ? ORGANIZER_ERROR_CONFLICT
                                     : ORGANIZER_ERROR_FILESYSTEM;
        } else {
            created = 1;
            destination = ORG_FDOPEN(descriptor, "wb");
            if (destination == NULL) {
                (void)ORG_CLOSE(descriptor);
                status = ORGANIZER_ERROR_FILESYSTEM;
            } else {
                unsigned char buffer[ORGANIZE_COPY_BYTES];
                FILE *source = open_regular_file(completed_file_path, &status);
                size_t copied = 0;
                int source_close_failed = 0;
                int destination_close_failed = 0;
                if (source != NULL) {
                    for (;;) {
                        size_t amount = fread(buffer, 1, sizeof(buffer), source);
                        if (amount != 0) {
                            if (amount > ORGANIZE_MAX_FILE_BYTES - copied) {
                                status = ORGANIZER_ERROR_TOO_LARGE;
                                break;
                            }
                            if (fwrite(buffer, 1, amount, destination) != amount ||
                                ferror(destination)) {
                                status = ORGANIZER_ERROR_WRITE;
                                break;
                            }
                            copied += amount;
                        }
                        if (amount == 0) {
                            if (ferror(source) || !feof(source)) {
                                status = ORGANIZER_ERROR_READ;
                            }
                            break;
                        }
                    }
                    if (copied != bytes && status == ORGANIZER_OK) {
                        status = ORGANIZER_ERROR_VERIFY;
                    }
                    if (fclose(source) != 0) { source_close_failed = 1; }
                    if (source_close_failed && status == ORGANIZER_OK) {
                        status = ORGANIZER_ERROR_CLOSE;
                    }
                }
                if (fclose(destination) != 0) { destination_close_failed = 1; }
                if (destination_close_failed && status == ORGANIZER_OK) {
                    status = ORGANIZER_ERROR_CLOSE;
                }
            }
        }

        if (status == ORGANIZER_OK) {
            char copied_digest[65] = {0};
            size_t copied_bytes = 0;
            hash_status = sha256_file(candidate_paths[selected_index],
                                      copied_digest, &copied_bytes);
            status = map_sha_status(hash_status);
            if (status == ORGANIZER_OK &&
                (copied_bytes != bytes || strcmp(copied_digest, digest) != 0)) {
                status = ORGANIZER_ERROR_VERIFY;
            }
        }
        if (status != ORGANIZER_OK) {
            if (created && remove(candidate_paths[selected_index]) != 0) {
                result->stored_path = candidate_paths[selected_index];
                candidate_paths[selected_index] = NULL;
                status = ORGANIZER_ERROR_CLEANUP;
                set_detail(result, status,
                           "failed to remove the incomplete canonical file");
            } else {
                set_detail(result, status, NULL);
            }
            cleanup_candidate_paths(candidate_paths);
            return status;
        }
    }

    result->status = ORGANIZER_NEW_FILE;
    result->is_duplicate = 0;
    result->stored_type = type;
    memcpy(result->sha256_hex, digest, sizeof(result->sha256_hex));
    result->bytes = bytes;
    result->stored_path = candidate_paths[selected_index];
    candidate_paths[selected_index] = NULL;
    set_detail(result, ORGANIZER_NEW_FILE, NULL);
    cleanup_candidate_paths(candidate_paths);
    return ORGANIZER_NEW_FILE;
}
