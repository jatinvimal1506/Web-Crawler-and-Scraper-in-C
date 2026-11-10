#ifndef DEEPDOWNLOAD_PIPELINE_H
#define DEEPDOWNLOAD_PIPELINE_H

#include "project.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PIPELINE_MAX_DOCUMENT_ATTEMPTS ((size_t)10)
#define PIPELINE_MAX_STAGE_NAME_ATTEMPTS ((size_t)100)

typedef enum {
    PIPELINE_OK = 0,
    PIPELINE_PARTIAL,
    PIPELINE_LIMIT_REACHED,
    PIPELINE_ERROR_INVALID_ARGUMENT,
    PIPELINE_ERROR_OUT_OF_MEMORY,
    PIPELINE_ERROR_FILESYSTEM,
    PIPELINE_ERROR_REPORT,
    PIPELINE_ERROR_CRAWL,
    PIPELINE_ERROR_INTERNAL
} PipelineStatus;

typedef struct {
    const char *start_url;             /* Borrowed absolute HTTP(S) seed. */
    const char *output_root;           /* Borrowed existing trusted directory. */
    const char *report_filename;       /* Borrowed safe .csv basename. */
    size_t max_depth;
    int same_domain;
    long timeout_seconds;              /* 1..3600 per HTTP request. */
    const char *const *keywords;       /* Borrowed; each keyword remains valid. */
    size_t keyword_count;              /* 0..8; zero selects all metadata. */
} PipelineConfig;

typedef struct {
    size_t html_pages_visited;
    size_t html_pages_failed;
    size_t html_links_discovered;
    size_t unique_document_urls_discovered;
    size_t document_source_relationships_discovered;
    size_t unique_document_urls_selected;
    size_t source_document_relationships_selected;
    size_t newly_stored_documents;
    size_t duplicate_content_documents;
    size_t failed_document_downloads;
    size_t documents_skipped_limit;
    size_t successful_document_bytes_downloaded;
    size_t bytes_newly_stored;
    size_t csv_rows_written;
} PipelineStats;

typedef struct {
    PipelineStatus status;
    PipelineStats stats;
    char *report_path;       /* Owned when a report exists, including cleanup failure. */
    char *orphaned_paths[2]; /* Owned paths that could not be cleaned; see count. */
    size_t orphaned_path_count;
    char detail[256];
} PipelineResult;

/* Initialize fresh or already-freed result storage; no allocation is done. */
PipelineStatus pipeline_result_init(PipelineResult *result);
/* NULL-safe/repeatable; frees owned report and orphaned paths. */
void pipeline_result_free(PipelineResult *result);
/* Static message; the caller must not free it. */
const char *pipeline_status_message(PipelineStatus status);

/*
 * Run bounded document discovery, metadata filtering, up to ten unique
 * downloads, content organization/deduplication, and a provenance CSV.
 * Config strings and keyword data are borrowed for this call. Initialize
 * result with pipeline_result_init() or {0}; free owned paths with
 * pipeline_result_free(). No other heap memory escapes.
 */
PipelineStatus pipeline_run(const PipelineConfig *config,
                            PipelineResult *result);

#ifdef __cplusplus
}
#endif

#endif
