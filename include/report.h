#ifndef DEEPDOWNLOAD_REPORT_H
#define DEEPDOWNLOAD_REPORT_H

#include "document.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define REPORT_MAX_KEYWORDS ((size_t)8)
#define REPORT_MAX_KEYWORD_BYTES ((size_t)64)
#define REPORT_MAX_ROWS ((size_t)1000)
#define REPORT_MAX_FIELD_BYTES ((size_t)4096)
#define REPORT_MAX_FILENAME_BYTES ((size_t)128)

typedef enum {
    REPORT_OK = 0,
    REPORT_ERROR_INVALID_ARGUMENT,
    REPORT_ERROR_INVALID_FILTER,
    REPORT_ERROR_INVALID_ROW,
    REPORT_ERROR_OUT_OF_MEMORY,
    REPORT_ERROR_FILESYSTEM,
    REPORT_ERROR_EXISTS,
    REPORT_ERROR_WRITE,
    REPORT_ERROR_CLOSE,
    REPORT_ERROR_TOO_MANY_ROWS,
    REPORT_ERROR_OVERFLOW,
    REPORT_ERROR_CLEANUP
} ReportStatus;

typedef enum {
    REPORT_ROW_DOWNLOADED_NEW = 0,
    REPORT_ROW_DUPLICATE,
    REPORT_ROW_DOWNLOAD_FAILED,
    REPORT_ROW_SKIPPED_LIMIT
} ReportRowStatus;

/*
 * A single source-page/document relationship. All pointers are borrowed.
 * Pass an empty string for unavailable hash, path or error fields.
 */
typedef struct {
    const char *document_url;
    DocumentType document_type;
    const char *source_page_url;
    size_t source_depth;
    ReportRowStatus outcome;
    const char *sha256_hex;
    size_t size_bytes;
    const char *stored_path;
    const char *error_detail;
} ReportRow;

/* The caller owns the array, not the keyword strings; matching never edits them. */
/* The caller owns the array, not the strings; each keyword is 1-64 printable
 * ASCII bytes and matching never edits them. */
typedef struct {
    const char *const *keywords;
    size_t keyword_count;
} ReportFilter;

typedef struct {
    ReportStatus status;
    size_t rows_written;  /* Complete data records, excluding the header. */
    size_t rows_excluded; /* Rows rejected by the optional keyword filter. */
    char *output_path;    /* Owned on success or cleanup failure. */
    char detail[256];
} ReportResult;

const char *report_status_message(ReportStatus status);
const char *report_row_status_name(ReportRowStatus status);
/* Initialize fresh or already-freed storage; free an owned path before reuse. */
ReportStatus report_result_init(ReportResult *result);
/* Free the owned path, reset the result; safe for NULL and repeated calls. */
void report_result_free(ReportResult *result);

/*
 * Match one metadata row against zero to eight ASCII literal substrings.
 * Matching ignores ASCII letter case and checks either URL. An empty filter
 * includes every row. This function performs no allocation or I/O.
 */
ReportStatus report_row_matches(const ReportRow *row,
                                const ReportFilter *filter,
                                int *matches);

/*
 * Write selected rows to a new, exclusive-created CSV in an existing directory.
 * The directory, safe basename, rows and keyword strings are borrowed. The
 * filename must be a safe .csv basename no longer than 128 bytes. At most
 * 1000 rows and 4096 bytes per text field are accepted. All text columns are
 * quoted and formula-guarded; integer columns are decimal. Initialize result
 * with report_result_init() or {0};
 * never shallow-copy a result that owns output_path. Free it with
 * report_result_free().
 *
 * On failure, only a file created by this call is removed. If removal fails,
 * REPORT_ERROR_CLEANUP is returned and output_path remains owned by result.
 */
ReportStatus report_write_csv(const char *existing_output_directory,
                              const char *safe_csv_basename,
                              const ReportRow *rows,
                              size_t row_count,
                              const ReportFilter *filter,
                              ReportResult *result);

#ifdef __cplusplus
}
#endif

#endif
