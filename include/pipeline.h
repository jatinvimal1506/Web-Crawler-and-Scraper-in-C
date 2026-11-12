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
    const char *start_url;
    const char *output_root;
    const char *report_filename;
    size_t max_depth;
    int same_domain;
    long timeout_seconds;
    const char *const *keywords;
    size_t keyword_count;
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
    char *report_path;
    char *orphaned_paths[2];
    size_t orphaned_path_count;
    char detail[256];
} PipelineResult;


PipelineStatus pipeline_result_init(PipelineResult *result);

void pipeline_result_free(PipelineResult *result);
// releases the memory owned by the result structure.

const char *pipeline_status_message(PipelineStatus status);
PipelineStatus pipeline_run(const PipelineConfig *config,
                            PipelineResult *result);

#ifdef __cplusplus
}
#endif

#endif
