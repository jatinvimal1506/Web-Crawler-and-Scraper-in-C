#include "http.h"
#include "pipeline.h"
#include "report.h"

#include <stdio.h>
#include <string.h>

static int parse_depth(const char *text, size_t *depth)
{
    size_t value = 0U;
    size_t index;
    if (text == NULL || text[0] == '\0' || depth == NULL) { return 0; }
    for (index = 0U; text[index] != '\0'; ++index) {
        unsigned digit;
        if (text[index] < '0' || text[index] > '9') { return 0; }
        digit = (unsigned)(text[index] - '0');
        if (digit > 3U || value > (3U - digit) / 10U) { return 0; }
        value = value * 10U + digit;
        if (value > 3U) { return 0; }
    }
    *depth = value;
    return 1;
}

static int is_keyword_option(const char *text)
{
    return text != NULL && strcmp(text, "--keyword") == 0;
}

static void usage(const char *program)
{
    fprintf(stderr,
        "Usage: %s START_URL OUTPUT_ROOT REPORT.csv [DEPTH 0..3] "
        "[SAME_DOMAIN 0|1] [--keyword WORD]...\n"
        "Defaults: depth=1, same_domain=1, timeout=3 seconds; "
        "at most 8 keywords.\n"
        "Use only controlled or permissioned sites. Downloads are limited "
        "to 10 unique documents.\n",
        program);
}

int main(int argc, char **argv)
{
    PipelineConfig config = {0};
    PipelineResult result = {0};
    const char *keywords[REPORT_MAX_KEYWORDS];
    size_t keyword_count = 0U, argument = 4U;
    size_t depth = 1U;
    int same_domain = 1;
    PipelineStatus status;
    size_t index;

    if (argc < 4) {
        usage(argv[0]);
        return 2;
    }
    if (argument < (size_t)argc && !is_keyword_option(argv[argument])) {
        if (!parse_depth(argv[argument], &depth)) {
            usage(argv[0]);
            return 2;
        }
        ++argument;
    }
    if (argument < (size_t)argc && !is_keyword_option(argv[argument])) {
        if (strcmp(argv[argument], "0") == 0) {
            same_domain = 0;
        } else if (strcmp(argv[argument], "1") != 0) {
            usage(argv[0]);
            return 2;
        }
        ++argument;
    }
    while (argument < (size_t)argc) {
        if (!is_keyword_option(argv[argument]) ||
            argument + 1U >= (size_t)argc ||
            keyword_count >= REPORT_MAX_KEYWORDS) {
            usage(argv[0]);
            return 2;
        }
        keywords[keyword_count++] = argv[argument + 1U];
        argument += 2U;
    }

    config.start_url = argv[1];
    config.output_root = argv[2];
    config.report_filename = argv[3];
    config.max_depth = depth;
    config.same_domain = same_domain;
    config.timeout_seconds = 3;
    config.keywords = keywords;
    config.keyword_count = keyword_count;
    (void)pipeline_result_init(&result);
    status = pipeline_run(&config, &result);

    printf("Pipeline status: %s\n", pipeline_status_message(status));
    printf("HTML pages visited: %zu\nHTML pages failed: %zu\n"
           "HTML links discovered: %zu\nUnique document URLs discovered: %zu\n"
           "Document/source relationships discovered: %zu\n"
           "Unique document URLs selected: %zu\n"
           "Selected source/document relationships: %zu\n"
           "Newly stored documents: %zu\nDuplicate-content documents: %zu\n"
           "Failed document downloads: %zu\nSkipped by limit: %zu\n"
           "Successful document bytes downloaded: %zu\n"
           "Bytes newly stored: %zu\nCSV rows written: %zu\n",
           result.stats.html_pages_visited,
           result.stats.html_pages_failed,
           result.stats.html_links_discovered,
           result.stats.unique_document_urls_discovered,
           result.stats.document_source_relationships_discovered,
           result.stats.unique_document_urls_selected,
           result.stats.source_document_relationships_selected,
           result.stats.newly_stored_documents,
           result.stats.duplicate_content_documents,
           result.stats.failed_document_downloads,
           result.stats.documents_skipped_limit,
           result.stats.successful_document_bytes_downloaded,
           result.stats.bytes_newly_stored,
           result.stats.csv_rows_written);
    if (result.report_path != NULL) {
        printf("Report: %s\n", result.report_path);
    }
    for (index = 0U; index < result.orphaned_path_count; ++index) {
        printf("Orphaned path requiring attention: %s\n",
               result.orphaned_paths[index]);
    }
    if (result.detail[0] != '\0') {
        printf("Detail: %s\n", result.detail);
    }

    pipeline_result_free(&result);
    http_cleanup();
    if (status == PIPELINE_OK) { return 0; }
    if (status == PIPELINE_PARTIAL || status == PIPELINE_LIMIT_REACHED) {
        return 1;
    }
    return 2;
}
