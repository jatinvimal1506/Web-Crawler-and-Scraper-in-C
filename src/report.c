#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
#include "report.h"

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <io.h>
#define REPORT_OPEN _open
#define REPORT_CLOSE _close
#define REPORT_FDOPEN _fdopen
#define REPORT_BINARY_FLAG _O_BINARY
#define REPORT_CREATE_FLAGS (_O_WRONLY | _O_CREAT | _O_EXCL | _O_BINARY)
#define REPORT_FILE_MODE (_S_IREAD | _S_IWRITE)
#define REPORT_SEPARATOR '\\'
#define REPORT_IS_DIRECTORY(mode) (((mode) & _S_IFMT) == _S_IFDIR)
typedef struct _stat ReportStat;
#else
#include <unistd.h>
#define REPORT_OPEN open
#define REPORT_CLOSE close
#define REPORT_FDOPEN fdopen
#define REPORT_BINARY_FLAG 0
#define REPORT_CREATE_FLAGS (O_WRONLY | O_CREAT | O_EXCL)
#define REPORT_FILE_MODE 0600
#define REPORT_SEPARATOR '/'
#define REPORT_IS_DIRECTORY(mode) S_ISDIR(mode)
typedef struct stat ReportStat;
#endif

#define REPORT_HEADER \
    "document_url,document_type,source_page_url,source_depth,status," \
    "sha256,size_bytes,stored_path,error\r\n"

static size_t bounded_length(const char *text, size_t maximum)
{
    size_t length = 0;
    if (text == NULL) { return maximum + 1U; }
    while (length <= maximum && text[length] != '\0') { ++length; }
    return length;
}

static int ascii_lower(int value)
{
    if (value >= 'A' && value <= 'Z') { return value + ('a' - 'A'); }
    return value;
}

static int valid_keyword(const char *keyword)
{
    size_t length = bounded_length(keyword, REPORT_MAX_KEYWORD_BYTES);
    size_t index;
    if (length == 0 || length > REPORT_MAX_KEYWORD_BYTES) { return 0; }
    for (index = 0; index < length; ++index) {
        unsigned char value = (unsigned char)keyword[index];
        if (value < 0x20U || value > 0x7eU) { return 0; }
    }
    return 1;
}

static ReportStatus validate_filter(const ReportFilter *filter)
{
    size_t index;
    if (filter == NULL || filter->keyword_count > REPORT_MAX_KEYWORDS ||
        (filter->keyword_count != 0 && filter->keywords == NULL)) {
        return REPORT_ERROR_INVALID_FILTER;
    }
    for (index = 0; index < filter->keyword_count; ++index) {
        if (!valid_keyword(filter->keywords[index])) {
            return REPORT_ERROR_INVALID_FILTER;
        }
    }
    return REPORT_OK;
}

ReportStatus report_validate_filter(const ReportFilter *filter)
{
    return validate_filter(filter);
}

static int valid_type(DocumentType type)
{
    return type == DOCUMENT_TYPE_PDF || type == DOCUMENT_TYPE_DOCX ||
           type == DOCUMENT_TYPE_ZIP;
}

static int valid_row_status(ReportRowStatus status)
{
    return status == REPORT_ROW_DOWNLOADED_NEW ||
           status == REPORT_ROW_DUPLICATE ||
           status == REPORT_ROW_DOWNLOAD_FAILED ||
           status == REPORT_ROW_SKIPPED_LIMIT;
}

static int valid_digest(const char *digest)
{
    size_t length = bounded_length(digest, 64U);
    size_t index;
    if (length == 0) { return 1; }
    if (length != 64U) { return 0; }
    for (index = 0; index < length; ++index) {
        unsigned char value = (unsigned char)digest[index];
        if (!((value >= '0' && value <= '9') ||
              (value >= 'a' && value <= 'f'))) {
            return 0;
        }
    }
    return 1;
}

static ReportStatus validate_row_metadata(const ReportRow *row)
{
    if (row == NULL || row->document_url == NULL ||
        row->source_page_url == NULL) {
        return REPORT_ERROR_INVALID_ROW;
    }
    if (bounded_length(row->document_url, REPORT_MAX_FIELD_BYTES) == 0 ||
        bounded_length(row->document_url, REPORT_MAX_FIELD_BYTES) >
            REPORT_MAX_FIELD_BYTES ||
        bounded_length(row->source_page_url, REPORT_MAX_FIELD_BYTES) == 0 ||
        bounded_length(row->source_page_url, REPORT_MAX_FIELD_BYTES) >
            REPORT_MAX_FIELD_BYTES) {
        return REPORT_ERROR_INVALID_ROW;
    }
    return REPORT_OK;
}

static ReportStatus validate_row(const ReportRow *row)
{
    size_t digest_length, path_length;
    ReportStatus status = validate_row_metadata(row);
    if (status != REPORT_OK) { return status; }
    if (row->sha256_hex == NULL || row->stored_path == NULL ||
        row->error_detail == NULL ||
        bounded_length(row->sha256_hex, REPORT_MAX_FIELD_BYTES) >
            REPORT_MAX_FIELD_BYTES ||
        bounded_length(row->stored_path, REPORT_MAX_FIELD_BYTES) >
            REPORT_MAX_FIELD_BYTES ||
        bounded_length(row->error_detail, REPORT_MAX_FIELD_BYTES) >
            REPORT_MAX_FIELD_BYTES ||
        !valid_type(row->document_type) || !valid_row_status(row->outcome) ||
        !valid_digest(row->sha256_hex)) {
        return REPORT_ERROR_INVALID_ROW;
    }
    digest_length = bounded_length(row->sha256_hex, REPORT_MAX_FIELD_BYTES);
    path_length = bounded_length(row->stored_path, REPORT_MAX_FIELD_BYTES);
    if (row->outcome == REPORT_ROW_DOWNLOADED_NEW ||
        row->outcome == REPORT_ROW_DUPLICATE) {
        if (digest_length != 64U || path_length == 0U ||
            row->size_bytes == 0U) {
            return REPORT_ERROR_INVALID_ROW;
        }
    } else if (digest_length != 0U || path_length != 0U) {
        return REPORT_ERROR_INVALID_ROW;
    }
    return REPORT_OK;
}

static int contains_ascii_case_insensitive(const char *text, size_t text_length,
                                           const char *keyword,
                                           size_t keyword_length)
{
    size_t prefix[REPORT_MAX_KEYWORD_BYTES];
    size_t index, matched = 0;

    if (keyword_length == 0U || keyword_length > REPORT_MAX_KEYWORD_BYTES ||
        keyword_length > text_length) {
        return 0;
    }





    prefix[0] = 0U;
    for (index = 1U; index < keyword_length; ++index) {
        while (matched > 0U &&
               ascii_lower((unsigned char)keyword[index]) !=
                   ascii_lower((unsigned char)keyword[matched])) {
            matched = prefix[matched - 1U];
        }
        if (ascii_lower((unsigned char)keyword[index]) ==
            ascii_lower((unsigned char)keyword[matched])) {
            ++matched;
        }
        prefix[index] = matched;
    }

    matched = 0U;
    for (index = 0U; index < text_length; ++index) {
        while (matched > 0U &&
               ascii_lower((unsigned char)text[index]) !=
                   ascii_lower((unsigned char)keyword[matched])) {
            matched = prefix[matched - 1U];
        }
        if (ascii_lower((unsigned char)text[index]) ==
            ascii_lower((unsigned char)keyword[matched])) {
            ++matched;
        }
        if (matched == keyword_length) { return 1; }
    }
    return 0;
}

const char *report_status_message(ReportStatus status)
{
    switch (status) {
    case REPORT_OK: return "report operation completed";
    case REPORT_ERROR_INVALID_ARGUMENT: return "invalid report argument";
    case REPORT_ERROR_INVALID_FILTER: return "invalid keyword filter";
    case REPORT_ERROR_INVALID_ROW: return "invalid report row";
    case REPORT_ERROR_OUT_OF_MEMORY: return "out of memory";
    case REPORT_ERROR_FILESYSTEM: return "report directory or file error";
    case REPORT_ERROR_EXISTS: return "report file already exists";
    case REPORT_ERROR_WRITE: return "could not write CSV report";
    case REPORT_ERROR_CLOSE: return "could not close CSV report cleanly";
    case REPORT_ERROR_TOO_MANY_ROWS: return "report exceeds the 1000-row limit";
    case REPORT_ERROR_OVERFLOW: return "report path or count overflow";
    case REPORT_ERROR_CLEANUP: return "could not remove incomplete CSV report";
    default: return "unknown report error";
    }
}

const char *report_row_status_name(ReportRowStatus status)
{
    switch (status) {
    case REPORT_ROW_DOWNLOADED_NEW: return "DOWNLOADED_NEW";
    case REPORT_ROW_DUPLICATE: return "DUPLICATE";
    case REPORT_ROW_DOWNLOAD_FAILED: return "DOWNLOAD_FAILED";
    case REPORT_ROW_SKIPPED_LIMIT: return "SKIPPED_LIMIT";
    default: return "INVALID";
    }
}

static int result_is_empty(const ReportResult *result)
{
    return result->status == REPORT_OK && result->rows_written == 0 &&
           result->rows_excluded == 0 && result->output_path == NULL &&
           result->detail[0] == '\0';
}

static void set_result_status(ReportResult *result, ReportStatus status,
                              const char *detail)
{
    result->status = status;
    if (detail == NULL) { detail = report_status_message(status); }
    (void)snprintf(result->detail, sizeof(result->detail), "%s", detail);
}

ReportStatus report_result_init(ReportResult *result)
{
    if (result == NULL) { return REPORT_ERROR_INVALID_ARGUMENT; }

    *result = (ReportResult){0};
    return REPORT_OK;
}

void report_result_free(ReportResult *result)
{
    if (result == NULL) { return; }
    free(result->output_path);
    *result = (ReportResult){0};
}

ReportStatus report_row_matches(const ReportRow *row,
                                const ReportFilter *filter,
                                int *matches)
{
    ReportStatus status;
    size_t document_length, source_length, index;
    if (matches != NULL) { *matches = 0; }
    if (matches == NULL) { return REPORT_ERROR_INVALID_ARGUMENT; }
    status = validate_row_metadata(row);
    if (status != REPORT_OK) { return status; }
    status = validate_filter(filter);
    if (status != REPORT_OK) { return status; }

    if (filter->keyword_count == 0) {
        *matches = 1;
        return REPORT_OK;
    }
    document_length = bounded_length(row->document_url, REPORT_MAX_FIELD_BYTES);
    source_length = bounded_length(row->source_page_url,
                                   REPORT_MAX_FIELD_BYTES);
    for (index = 0; index < filter->keyword_count; ++index) {
        size_t keyword_length = bounded_length(
            filter->keywords[index], REPORT_MAX_KEYWORD_BYTES);
        if (contains_ascii_case_insensitive(row->document_url,
                                            document_length,
                                            filter->keywords[index],
                                            keyword_length) ||
            contains_ascii_case_insensitive(row->source_page_url,
                                            source_length,
                                            filter->keywords[index],
                                            keyword_length)) {
            *matches = 1;
            break;
        }
    }
    return REPORT_OK;
}

static int path_is_directory(const char *path)
{
    ReportStat info;
#ifdef _WIN32
    if (_stat(path, &info) != 0) { return 0; }
#else
    if (lstat(path, &info) != 0) { return 0; }
#endif
    return REPORT_IS_DIRECTORY(info.st_mode);
}

static int valid_csv_basename(const char *filename)
{
    size_t length, index, suffix_start;
    if (filename == NULL) { return 0; }
    length = bounded_length(filename, REPORT_MAX_FILENAME_BYTES);
    if (length < 5U || length > REPORT_MAX_FILENAME_BYTES ||
        filename[0] == '.') {
        return 0;
    }
    suffix_start = length - 4U;
    if (ascii_lower((unsigned char)filename[suffix_start]) != '.' ||
        ascii_lower((unsigned char)filename[suffix_start + 1U]) != 'c' ||
        ascii_lower((unsigned char)filename[suffix_start + 2U]) != 's' ||
        ascii_lower((unsigned char)filename[suffix_start + 3U]) != 'v') {
        return 0;
    }
    for (index = 0; index < length; ++index) {
        unsigned char value = (unsigned char)filename[index];
        if (!((value >= 'a' && value <= 'z') ||
              (value >= 'A' && value <= 'Z') ||
              (value >= '0' && value <= '9') || value == '.' ||
              value == '_' || value == '-')) {
            return 0;
        }
    }

    {
        size_t stem_length = 0;
        while (stem_length < length && filename[stem_length] != '.') {
            ++stem_length;
        }
        if ((stem_length == 3U &&
             (ascii_lower((unsigned char)filename[0]) == 'c' &&
              ascii_lower((unsigned char)filename[1]) == 'o' &&
              ascii_lower((unsigned char)filename[2]) == 'n')) ||
            (stem_length == 3U &&
             (ascii_lower((unsigned char)filename[0]) == 'p' &&
              ascii_lower((unsigned char)filename[1]) == 'r' &&
              ascii_lower((unsigned char)filename[2]) == 'n')) ||
            (stem_length == 3U &&
             (ascii_lower((unsigned char)filename[0]) == 'a' &&
              ascii_lower((unsigned char)filename[1]) == 'u' &&
              ascii_lower((unsigned char)filename[2]) == 'x')) ||
            (stem_length == 3U &&
             (ascii_lower((unsigned char)filename[0]) == 'n' &&
              ascii_lower((unsigned char)filename[1]) == 'u' &&
              ascii_lower((unsigned char)filename[2]) == 'l')) ||
            (stem_length == 4U &&
             ascii_lower((unsigned char)filename[0]) == 'c' &&
             ascii_lower((unsigned char)filename[1]) == 'o' &&
             ascii_lower((unsigned char)filename[2]) == 'm' &&
             ascii_lower((unsigned char)filename[3]) >= '1' &&
             ascii_lower((unsigned char)filename[3]) <= '9') ||
            (stem_length == 4U &&
             ascii_lower((unsigned char)filename[0]) == 'l' &&
             ascii_lower((unsigned char)filename[1]) == 'p' &&
             ascii_lower((unsigned char)filename[2]) == 't' &&
             ascii_lower((unsigned char)filename[3]) >= '1' &&
             ascii_lower((unsigned char)filename[3]) <= '9')) {
            return 0;
        }
    }
    return 1;
}

ReportStatus report_validate_csv_basename(const char *safe_csv_basename)
{
    return valid_csv_basename(safe_csv_basename)
        ? REPORT_OK : REPORT_ERROR_INVALID_ARGUMENT;
}

static ReportStatus make_output_path(const char *directory,
                                    const char *filename, char **output)
{
    size_t directory_length = strlen(directory);
    size_t filename_length = strlen(filename);
    size_t separator = directory_length != 0 &&
                       directory[directory_length - 1U] == REPORT_SEPARATOR
        ? 0U : 1U;
    size_t total;
    char *path;

#ifdef _WIN32
    if (directory_length != 0 && directory[directory_length - 1U] == '/') {
        separator = 0U;
    }
#endif
    *output = NULL;
    if (filename_length > SIZE_MAX - separator - 1U ||
        directory_length > SIZE_MAX - filename_length - separator - 1U) {
        return REPORT_ERROR_OVERFLOW;
    }
    total = directory_length + separator + filename_length + 1U;
    path = malloc(total);
    if (path == NULL) { return REPORT_ERROR_OUT_OF_MEMORY; }
    memcpy(path, directory, directory_length);
    if (separator != 0U) { path[directory_length] = REPORT_SEPARATOR; }
    memcpy(path + directory_length + separator, filename, filename_length + 1U);
    *output = path;
    return REPORT_OK;
}

static int write_bytes(FILE *file, const void *bytes, size_t length)
{
    return length == 0U || fwrite(bytes, 1, length, file) == length;
}

static int formula_dangerous(const char *text, size_t length)
{
    size_t index = 0;
    while (index < length) {
        unsigned char value = (unsigned char)text[index];
        if (value > 0x20U && value != 0x7fU) { break; }
        ++index;
    }
    if (index == length) { return 0; }
    return text[index] == '=' || text[index] == '+' ||
           text[index] == '-' || text[index] == '@';
}

static int write_text_field(FILE *file, const char *text)
{
    static const char quote = '"';
    static const char escaped_quote[] = "\"\"";
    size_t length = strlen(text), segment = 0, index;
    int prefix_apostrophe = formula_dangerous(text, length);

    if (!write_bytes(file, &quote, 1U)) { return 0; }
    if (prefix_apostrophe) {
        static const char apostrophe = '\'';
        if (!write_bytes(file, &apostrophe, 1U)) { return 0; }
    }
    for (index = 0; index < length; ++index) {
        if (text[index] == '"') {
            if (!write_bytes(file, text + segment, index - segment) ||
                !write_bytes(file, escaped_quote, sizeof(escaped_quote) - 1U)) {
                return 0;
            }
            segment = index + 1U;
        }
    }
    return write_bytes(file, text + segment, length - segment) &&
           write_bytes(file, &quote, 1U);
}

static int write_size(FILE *file, size_t value)
{
    char buffer[sizeof(size_t) * 3U + 1U];
    int length = snprintf(buffer, sizeof(buffer), "%zu", value);
    return length >= 0 && (size_t)length < sizeof(buffer) &&
           write_bytes(file, buffer, (size_t)length);
}

static int write_csv_row(FILE *file, const ReportRow *row)
{
    const char *type = document_type_name(row->document_type);
    const char *outcome = report_row_status_name(row->outcome);
    return write_text_field(file, row->document_url) &&
           write_bytes(file, ",", 1U) &&
           write_text_field(file, type) &&
           write_bytes(file, ",", 1U) &&
           write_text_field(file, row->source_page_url) &&
           write_bytes(file, ",", 1U) &&
           write_size(file, row->source_depth) &&
           write_bytes(file, ",", 1U) &&
           write_text_field(file, outcome) &&
           write_bytes(file, ",", 1U) &&
           write_text_field(file, row->sha256_hex) &&
           write_bytes(file, ",", 1U) &&
           write_size(file, row->size_bytes) &&
           write_bytes(file, ",", 1U) &&
           write_text_field(file, row->stored_path) &&
           write_bytes(file, ",", 1U) &&
           write_text_field(file, row->error_detail) &&
           write_bytes(file, "\r\n", 2U);
}

static ReportStatus validate_rows(const ReportRow *rows, size_t row_count,
                                  const ReportFilter *filter)
{
    size_t index;
    ReportStatus status;
    if (row_count > REPORT_MAX_ROWS) { return REPORT_ERROR_TOO_MANY_ROWS; }
    if (row_count != 0U && rows == NULL) {
        return REPORT_ERROR_INVALID_ARGUMENT;
    }
    status = validate_filter(filter);
    if (status != REPORT_OK) { return status; }
    for (index = 0; index < row_count; ++index) {
        int matches = 0;
        status = validate_row(&rows[index]);
        if (status != REPORT_OK) { return status; }
        status = report_row_matches(&rows[index], filter, &matches);
        if (status != REPORT_OK) { return status; }
    }
    return REPORT_OK;
}

ReportStatus report_write_csv(const char *existing_output_directory,
                              const char *safe_csv_basename,
                              const ReportRow *rows,
                              size_t row_count,
                              const ReportFilter *filter,
                              ReportResult *result)
{
    ReportStatus status;
    char *output_path = NULL;
    int descriptor = -1, created = 0;
    FILE *file = NULL;
    size_t index;
    int close_failed = 0;

    if (result == NULL || !result_is_empty(result)) {
        return REPORT_ERROR_INVALID_ARGUMENT;
    }
    if (existing_output_directory == NULL ||
        existing_output_directory[0] == '\0' ||
        report_validate_csv_basename(safe_csv_basename) != REPORT_OK) {
        status = REPORT_ERROR_INVALID_ARGUMENT;
        goto done;
    }
    if (!path_is_directory(existing_output_directory)) {
        status = REPORT_ERROR_FILESYSTEM;
        goto done;
    }
    status = validate_rows(rows, row_count, filter);
    if (status != REPORT_OK) { goto done; }
    status = make_output_path(existing_output_directory, safe_csv_basename,
                              &output_path);
    if (status != REPORT_OK) { goto done; }

#ifdef _WIN32
    descriptor = REPORT_OPEN(output_path, REPORT_CREATE_FLAGS,
                             REPORT_FILE_MODE);
#else
    {
        int flags = REPORT_CREATE_FLAGS;
#ifdef O_NOFOLLOW
        flags |= O_NOFOLLOW;
#endif
        descriptor = REPORT_OPEN(output_path, flags, REPORT_FILE_MODE);
    }
#endif
    if (descriptor < 0) {
        status = errno == EEXIST ? REPORT_ERROR_EXISTS
                                 : REPORT_ERROR_FILESYSTEM;
        goto done;
    }
    created = 1;
    file = REPORT_FDOPEN(descriptor, "wb");
    if (file == NULL) {
        (void)REPORT_CLOSE(descriptor);
        descriptor = -1;
        status = REPORT_ERROR_FILESYSTEM;
        goto done;
    }
    descriptor = -1;

    if (!write_bytes(file, REPORT_HEADER, sizeof(REPORT_HEADER) - 1U)) {
        status = REPORT_ERROR_WRITE;
        goto close_file;
    }
    status = REPORT_OK;
    for (index = 0; index < row_count; ++index) {
        int matches = 0;
        status = report_row_matches(&rows[index], filter, &matches);
        if (status != REPORT_OK) { break; }
        if (!matches) {
            ++result->rows_excluded;
            continue;
        }
        if (!write_csv_row(file, &rows[index])) {
            status = REPORT_ERROR_WRITE;
            break;
        }
        ++result->rows_written;
    }

close_file:
    if (fclose(file) != 0) { close_failed = 1; }
    file = NULL;
    if (status == REPORT_OK && close_failed) {
        status = REPORT_ERROR_CLOSE;
    }
    if (status != REPORT_OK) { goto done; }

    result->output_path = output_path;
    output_path = NULL;
    set_result_status(result, REPORT_OK, NULL);
    return REPORT_OK;

done:
    if (file != NULL) {
        if (fclose(file) != 0 && status == REPORT_OK) {
            status = REPORT_ERROR_CLOSE;
        }
        file = NULL;
    }
    if (descriptor >= 0) { (void)REPORT_CLOSE(descriptor); }
    if (created && remove(output_path) != 0) {
        result->output_path = output_path;
        output_path = NULL;
        status = REPORT_ERROR_CLEANUP;
    }
    free(output_path);
    set_result_status(result, status, NULL);
    return status;
}
