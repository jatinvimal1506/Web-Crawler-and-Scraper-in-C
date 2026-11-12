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

typedef struct {
    const char *const *keywords;
    size_t keyword_count;
} ReportFilter;

typedef struct {
    ReportStatus status;
    size_t rows_written;
    size_t rows_excluded;
    char *output_path;
    char detail[256];
} ReportResult;

const char *report_status_message(ReportStatus status);
const char *report_row_status_name(ReportRowStatus status);
ReportStatus report_result_init(ReportResult *result);
void report_result_free(ReportResult *result);

ReportStatus report_validate_filter(const ReportFilter *filter);
ReportStatus report_validate_csv_basename(const char *safe_csv_basename);


ReportStatus report_row_matches(const ReportRow *row,const ReportFilter *filter,int *matches);
//it checks whether the users keyword appears in the document URL or the source pagea URL.
// and our filter examines URL information, not the internal text of PDF or word files.
ReportStatus report_write_csv(const char *existing_output_directory,
  const char *safe_csv_basename,const ReportRow *rows,size_t row_count,  const ReportFilter *filter, ReportResult *result);

#ifdef __cplusplus
}
#endif

#endif
