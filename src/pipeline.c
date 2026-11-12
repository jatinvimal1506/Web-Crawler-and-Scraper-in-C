#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
#include "pipeline.h"

#include "document.h"
#include "download.h"
#include "http.h"
#include "organize.h"
#include "report.h"
#include "url.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
typedef struct _stat PipelineStat;
#define PIPELINE_STAT _stat
#define PIPELINE_MKDIR(path) _mkdir(path)
#define PIPELINE_IS_DIRECTORY(mode) (((mode) & _S_IFMT) == _S_IFDIR)
#else
#include <unistd.h>
typedef struct stat PipelineStat;
#define PIPELINE_STAT lstat
#define PIPELINE_MKDIR(path) mkdir((path), 0700)
#define PIPELINE_IS_DIRECTORY(mode) S_ISDIR(mode)
#endif

#define PIPELINE_PATH_LIMIT ((size_t)4096)
#define PIPELINE_DETAIL_BYTES ((size_t)256)

typedef enum {
    PIPELINE_PATH_MISSING = 0,
    PIPELINE_PATH_DIRECTORY,
    PIPELINE_PATH_OTHER,
    PIPELINE_PATH_ERROR
} PipelinePathKind;

typedef struct {
    ReportRowStatus outcome;
    char sha256_hex[65];
    size_t size_bytes;
    char *stored_path;
    char error_detail[256];
} DocumentOutcome;

static int add_size(size_t *value, size_t amount)
{
    if (value == NULL || amount > SIZE_MAX - *value) { return 0; }
    *value += amount;
    return 1;
}

static void set_detail(char destination[256], const char *text)
{
    if (text == NULL) { text = ""; }
    (void)snprintf(destination, 256U, "%s", text);
}

static size_t bounded_string_length(const char *text, size_t maximum)
{
    size_t length = 0U;
    if (text == NULL) { return maximum + 1U; }
    while (length <= maximum && text[length] != '\0') { ++length; }
    return length;
}

static void set_result(PipelineResult *result, PipelineStatus status,
                       const char *detail)
{
    result->status = status;
    if (detail == NULL) { detail = pipeline_status_message(status); }
    set_detail(result->detail, detail);
}

static int stats_are_zero(const PipelineStats *stats)
{
    return stats->html_pages_visited == 0U &&
           stats->html_pages_failed == 0U &&
           stats->html_links_discovered == 0U &&
           stats->unique_document_urls_discovered == 0U &&
           stats->document_source_relationships_discovered == 0U &&
           stats->unique_document_urls_selected == 0U &&
           stats->source_document_relationships_selected == 0U &&
           stats->newly_stored_documents == 0U &&
           stats->duplicate_content_documents == 0U &&
           stats->failed_document_downloads == 0U &&
           stats->documents_skipped_limit == 0U &&
           stats->successful_document_bytes_downloaded == 0U &&
           stats->bytes_newly_stored == 0U &&
           stats->csv_rows_written == 0U;
}

static int result_is_empty(const PipelineResult *result)
{
    return result->status == PIPELINE_OK && stats_are_zero(&result->stats) &&
           result->report_path == NULL &&
           result->orphaned_paths[0] == NULL &&
           result->orphaned_paths[1] == NULL &&
           result->orphaned_path_count == 0U && result->detail[0] == '\0';
}

PipelineStatus pipeline_result_init(PipelineResult *result)
{
    if (result == NULL) { return PIPELINE_ERROR_INVALID_ARGUMENT; }

    *result = (PipelineResult){0};
    return PIPELINE_OK;
}

void pipeline_result_free(PipelineResult *result)
{
    size_t index;
    if (result == NULL) { return; }
    free(result->report_path);
    for (index = 0U; index < 2U; ++index) {
        free(result->orphaned_paths[index]);
    }
    *result = (PipelineResult){0};
}

const char *pipeline_status_message(PipelineStatus status)
{
    switch (status) {
    case PIPELINE_OK: return "pipeline completed successfully";
    case PIPELINE_PARTIAL: return "pipeline completed with failures";
    case PIPELINE_LIMIT_REACHED: return "pipeline reached a configured limit";
    case PIPELINE_ERROR_INVALID_ARGUMENT: return "invalid pipeline configuration";
    case PIPELINE_ERROR_OUT_OF_MEMORY: return "pipeline ran out of memory";
    case PIPELINE_ERROR_FILESYSTEM: return "pipeline filesystem operation failed";
    case PIPELINE_ERROR_REPORT: return "pipeline report could not be created";
    case PIPELINE_ERROR_CRAWL: return "document discovery failed";
    case PIPELINE_ERROR_INTERNAL: return "pipeline internal error";
    default: return "unknown pipeline status";
    }
}

static PipelineStatus map_report_validation(ReportStatus status)
{
    return status == REPORT_ERROR_OUT_OF_MEMORY
        ? PIPELINE_ERROR_OUT_OF_MEMORY : PIPELINE_ERROR_INVALID_ARGUMENT;
}

static PipelineStatus validate_config(const PipelineConfig *config)
{
    ParsedUrl parsed = {0};
    UrlStatus url_status;
    ReportFilter filter;
    ReportStatus report_status;

    if (config == NULL || config->start_url == NULL ||
        config->start_url[0] == '\0' || config->output_root == NULL ||
        config->output_root[0] == '\0' || config->report_filename == NULL ||
        config->report_filename[0] == '\0' ||
        (config->same_domain != 0 && config->same_domain != 1) ||
        config->timeout_seconds < 1 ||
        config->timeout_seconds > DOWNLOAD_MAX_TIMEOUT_SECONDS ||
        bounded_string_length(config->output_root, PIPELINE_PATH_LIMIT) >
            PIPELINE_PATH_LIMIT) {
        return PIPELINE_ERROR_INVALID_ARGUMENT;
    }
    url_status = url_parse(config->start_url, &parsed);
    if (url_status != URL_OK) {
        return url_status == URL_ERROR_OUT_OF_MEMORY
            ? PIPELINE_ERROR_OUT_OF_MEMORY : PIPELINE_ERROR_INVALID_ARGUMENT;
    }
    url_free(&parsed);

    filter.keywords = config->keywords;
    filter.keyword_count = config->keyword_count;
    report_status = report_validate_filter(&filter);
    if (report_status != REPORT_OK) {
        return map_report_validation(report_status);
    }
    report_status = report_validate_csv_basename(config->report_filename);
    if (report_status != REPORT_OK) {
        return map_report_validation(report_status);
    }
    return PIPELINE_OK;
}

static PipelinePathKind inspect_path(const char *path)
{
    PipelineStat info;
    if (PIPELINE_STAT(path, &info) != 0) {
        return errno == ENOENT ? PIPELINE_PATH_MISSING : PIPELINE_PATH_ERROR;
    }
    if (PIPELINE_IS_DIRECTORY(info.st_mode)) { return PIPELINE_PATH_DIRECTORY; }
    return PIPELINE_PATH_OTHER;
}

static int is_separator(char value)
{
#ifdef _WIN32
    return value == '/' || value == '\\';
#else
    return value == '/';
#endif
}

static PipelineStatus join_path(const char *directory, const char *name,
                                char **joined)
{
    size_t directory_length, name_length, separator, total;
    char *path;
    if (joined == NULL) { return PIPELINE_ERROR_INTERNAL; }
    *joined = NULL;
    if (directory == NULL || name == NULL) {
        return PIPELINE_ERROR_INVALID_ARGUMENT;
    }
    directory_length = bounded_string_length(directory, PIPELINE_PATH_LIMIT);
    name_length = strlen(name);
    if (directory_length == 0U || directory_length > PIPELINE_PATH_LIMIT) {
        return PIPELINE_ERROR_INVALID_ARGUMENT;
    }
    separator = is_separator(directory[directory_length - 1U]) ? 0U : 1U;
    if (name_length > SIZE_MAX - separator - 1U ||
        directory_length > SIZE_MAX - name_length - separator - 1U) {
        return PIPELINE_ERROR_INTERNAL;
    }
    total = directory_length + separator + name_length + 1U;
    if (total - 1U > PIPELINE_PATH_LIMIT) {
        return PIPELINE_ERROR_INVALID_ARGUMENT;
    }
    path = malloc(total);
    if (path == NULL) { return PIPELINE_ERROR_OUT_OF_MEMORY; }
    memcpy(path, directory, directory_length);
    if (separator != 0U) {
#ifdef _WIN32
        path[directory_length] = '\\';
#else
        path[directory_length] = '/';
#endif
    }
    memcpy(path + directory_length + separator, name, name_length + 1U);
    *joined = path;
    return PIPELINE_OK;
}

static PipelineStatus allocate_zeroed(size_t count, size_t element_size,
                                      void **memory)
{
    size_t bytes;
    if (memory == NULL) { return PIPELINE_ERROR_INTERNAL; }
    *memory = NULL;
    if (count == 0U) { return PIPELINE_OK; }
    if (element_size == 0U || count > SIZE_MAX / element_size) {
        return PIPELINE_ERROR_INTERNAL;
    }
    bytes = count * element_size;
    *memory = malloc(bytes);
    if (*memory == NULL) { return PIPELINE_ERROR_OUT_OF_MEMORY; }
    memset(*memory, 0, bytes);
    return PIPELINE_OK;
}

static PipelineStatus ensure_staging_directory(const char *output_root,
                                               char **staging_directory)
{
    PipelineStatus status = join_path(output_root, "staging",
                                      staging_directory);
    PipelinePathKind kind;
    if (status != PIPELINE_OK) { return status; }
    kind = inspect_path(*staging_directory);
    if (kind == PIPELINE_PATH_DIRECTORY) { return PIPELINE_OK; }
    if (kind == PIPELINE_PATH_MISSING) {
        if (PIPELINE_MKDIR(*staging_directory) == 0 || errno == EEXIST) {
            kind = inspect_path(*staging_directory);
            if (kind == PIPELINE_PATH_DIRECTORY) { return PIPELINE_OK; }
        }
    }
    free(*staging_directory);
    *staging_directory = NULL;
    return PIPELINE_ERROR_FILESYSTEM;
}

static void copy_crawler_stats(PipelineStats *destination,
                               const CrawlerStats *source)
{
    destination->html_pages_visited = source->pages_visited;
    destination->html_pages_failed = source->pages_failed;
    destination->html_links_discovered = source->links_discovered;
}

static int adopt_orphaned_path(PipelineResult *result, char **path)
{
    if (path == NULL || *path == NULL) { return 1; }
    if (result->orphaned_path_count >= 2U) { return 0; }
    result->orphaned_paths[result->orphaned_path_count] = *path;
    ++result->orphaned_path_count;
    *path = NULL;
    return 1;
}

static PipelineStatus allocate_selection(
    const DocumentResults *documents, const ReportFilter *filter,
    PipelineStats *stats, unsigned char **document_selected,
    unsigned char **source_selected, DocumentOutcome **outcomes)
{
    size_t document_count = document_results_count(documents);
    size_t source_count = document_results_source_count(documents);
    size_t document_index, source_index = 0U;
    PipelineStatus status;

    if (document_count > DOCUMENT_MAX_OCCURRENCES ||
        source_count > DOCUMENT_MAX_OCCURRENCES) {
        return PIPELINE_ERROR_INTERNAL;
    }
    status = allocate_zeroed(document_count, sizeof(**document_selected),
                             (void **)document_selected);
    if (status != PIPELINE_OK) { return status; }
    status = allocate_zeroed(source_count, sizeof(**source_selected),
                             (void **)source_selected);
    if (status != PIPELINE_OK) { return status; }
    status = allocate_zeroed(document_count, sizeof(**outcomes),
                             (void **)outcomes);
    if (status != PIPELINE_OK) { return status; }

    stats->unique_document_urls_discovered = document_count;
    stats->document_source_relationships_discovered = source_count;
    for (document_index = 0U; document_index < document_count;
         ++document_index) {
        const DocumentRecord *record =
            document_results_at(documents, document_index);
        size_t source_index_in_record;
        int any_source_selected = 0;
        if (record == NULL || record->url == NULL ||
            record->source_count == 0U ||
            source_index > source_count ||
            record->source_count > source_count - source_index) {
            return PIPELINE_ERROR_INTERNAL;
        }
        (*outcomes)[document_index].outcome = REPORT_ROW_DOWNLOAD_FAILED;
        for (source_index_in_record = 0U;
             source_index_in_record < record->source_count;
             ++source_index_in_record, ++source_index) {
            ReportRow metadata = {0};
            int matches = 0;
            ReportStatus report_status;
            if (record->sources == NULL ||
                record->sources[source_index_in_record].page_url == NULL) {
                return PIPELINE_ERROR_INTERNAL;
            }
            metadata.document_url = record->url;
            metadata.source_page_url =
                record->sources[source_index_in_record].page_url;
            report_status = report_row_matches(&metadata, filter, &matches);
            if (report_status != REPORT_OK) {
                return report_status == REPORT_ERROR_OUT_OF_MEMORY
                    ? PIPELINE_ERROR_OUT_OF_MEMORY : PIPELINE_ERROR_INTERNAL;
            }
            if (matches) {
                (*source_selected)[source_index] = 1U;
                any_source_selected = 1;
                if (!add_size(&stats->source_document_relationships_selected,
                              1U)) {
                    return PIPELINE_ERROR_INTERNAL;
                }
            }
        }
        if (any_source_selected) {
            (*document_selected)[document_index] = 1U;
            if (!add_size(&stats->unique_document_urls_selected, 1U)) {
                return PIPELINE_ERROR_INTERNAL;
            }
        }
    }
    return source_index == source_count
        ? PIPELINE_OK : PIPELINE_ERROR_INTERNAL;
}

static const char *document_extension(DocumentType type)
{
    switch (type) {
    case DOCUMENT_TYPE_PDF: return "pdf";
    case DOCUMENT_TYPE_DOCX: return "docx";
    case DOCUMENT_TYPE_ZIP: return "zip";
    default: return NULL;
    }
}

static PipelineStatus download_fatal_status(DownloadStatus status)
{
    switch (status) {
    case DOWNLOAD_ERROR_OUT_OF_MEMORY:
        return PIPELINE_ERROR_OUT_OF_MEMORY;
    case DOWNLOAD_ERROR_FILE:
    case DOWNLOAD_ERROR_WRITE:
    case DOWNLOAD_ERROR_CLEANUP:
        return PIPELINE_ERROR_FILESYSTEM;
    case DOWNLOAD_ERROR_OVERFLOW:
    case DOWNLOAD_ERROR_INVALID_ARGUMENT:
    case DOWNLOAD_ERROR_UNSUPPORTED_TYPE:
    case DOWNLOAD_ERROR_INIT:
        return PIPELINE_ERROR_INTERNAL;
    default:
        return PIPELINE_PARTIAL;
    }
}

static PipelineStatus organizer_fatal_status(OrganizerStatus status)
{
    switch (status) {
    case ORGANIZER_ERROR_OUT_OF_MEMORY:
        return PIPELINE_ERROR_OUT_OF_MEMORY;
    case ORGANIZER_ERROR_INVALID_ARGUMENT:
    case ORGANIZER_ERROR_INVALID_TYPE:
    case ORGANIZER_ERROR_OVERFLOW:
        return PIPELINE_ERROR_INTERNAL;
    default:
        return PIPELINE_ERROR_FILESYSTEM;
    }
}

static PipelineStatus process_one_document(
    const PipelineConfig *config, const char *staging_directory,
    const DocumentRecord *record, DocumentOutcome *outcome,
    PipelineStats *stats, PipelineResult *result, size_t *stage_serial,
    int *partial)
{
    const char *extension = document_extension(record->type);
    size_t name_attempt;
    DownloadResult download = {0};
    DownloadStatus download_status = DOWNLOAD_ERROR_FILE;
    char filename[64] = {0};
    char *candidate_path = NULL;
    PipelinePathKind path_kind;
    PipelineStatus status = PIPELINE_ERROR_FILESYSTEM;
    OrganizeResult organized = {0};
    OrganizerStatus organize_status;
    int download_ready = 0;

    if (extension == NULL) { return PIPELINE_ERROR_INTERNAL; }
    for (name_attempt = 0U;
         name_attempt < PIPELINE_MAX_STAGE_NAME_ATTEMPTS; ++name_attempt) {
        int written;
        if (*stage_serial == SIZE_MAX) { return PIPELINE_ERROR_INTERNAL; }
        ++(*stage_serial);
        written = snprintf(filename, sizeof(filename), "stage-%06zu.%s",
                           *stage_serial, extension);
        if (written <= 0 || (size_t)written >= sizeof(filename)) {
            return PIPELINE_ERROR_INTERNAL;
        }
        status = join_path(staging_directory, filename, &candidate_path);
        if (status != PIPELINE_OK) { return status; }
        path_kind = inspect_path(candidate_path);
        free(candidate_path);
        candidate_path = NULL;
        if (path_kind == PIPELINE_PATH_ERROR) {
            return PIPELINE_ERROR_FILESYSTEM;
        }
        if (path_kind != PIPELINE_PATH_MISSING) { continue; }

        {
            DownloadConfig download_config;
            download_config.document_url = record->url;
            download_config.seed_url = config->start_url;
            download_config.same_domain = config->same_domain;
            download_config.timeout_seconds = config->timeout_seconds;
            download_config.output_directory = staging_directory;
            download_config.filename = filename;
            download_status = download_document(&download_config, &download);
        }
        if (download_status == DOWNLOAD_ERROR_EXISTS) {

            download_result_free(&download);
            continue;
        }
        download_ready = 1;
        break;
    }
    if (!download_ready) {
        return PIPELINE_ERROR_FILESYSTEM;
    }
    if (download_status != DOWNLOAD_OK) {
        PipelineStatus fatal;
        if (download.file_path != NULL) {
            (void)adopt_orphaned_path(result, &download.file_path);
            set_detail(result->detail,
                       download.detail[0] != '\0' ? download.detail
                           : download_status_message(download_status));
            download_result_free(&download);
            return PIPELINE_ERROR_FILESYSTEM;
        }
        fatal = download_fatal_status(download_status);
        if (fatal != PIPELINE_PARTIAL) {
            set_detail(result->detail,
                       download.detail[0] != '\0' ? download.detail
                           : download_status_message(download_status));
            download_result_free(&download);
            return fatal;
        }
        outcome->outcome = REPORT_ROW_DOWNLOAD_FAILED;
        set_detail(outcome->error_detail,
                   download.detail[0] != '\0' ? download.detail
                       : download_status_message(download_status));
        if (!add_size(&stats->failed_document_downloads, 1U)) {
            download_result_free(&download);
            return PIPELINE_ERROR_INTERNAL;
        }
        *partial = 1;
        download_result_free(&download);
        return PIPELINE_OK;
    }
    if (download.file_path == NULL) {
        download_result_free(&download);
        return PIPELINE_ERROR_INTERNAL;
    }
    if (download.bytes_written == 0U) {
        if (remove(download.file_path) != 0) {
            (void)adopt_orphaned_path(result, &download.file_path);
        }
        download_result_free(&download);
        return PIPELINE_ERROR_INTERNAL;
    }
    if (!add_size(&stats->successful_document_bytes_downloaded,
                  download.bytes_written)) {
        if (remove(download.file_path) != 0 &&
            !adopt_orphaned_path(result, &download.file_path)) {
            download_result_free(&download);
            return PIPELINE_ERROR_INTERNAL;
        }
        download_result_free(&download);
        return PIPELINE_ERROR_INTERNAL;
    }

    organize_status = organize_file(download.file_path, record->type,
                                    config->output_root, &organized);
    if (organize_status != ORGANIZER_NEW_FILE &&
        organize_status != ORGANIZER_DUPLICATE) {
        PipelineStatus fatal = organizer_fatal_status(organize_status);
        if (organized.stored_path != NULL &&
            !adopt_orphaned_path(result, &organized.stored_path)) {
            organize_result_free(&organized);
            download_result_free(&download);
            return PIPELINE_ERROR_INTERNAL;
        }
        if (remove(download.file_path) != 0 &&
            !adopt_orphaned_path(result, &download.file_path)) {
            organize_result_free(&organized);
            download_result_free(&download);
            return PIPELINE_ERROR_INTERNAL;
        }
        set_detail(result->detail,
                   organized.detail[0] != '\0' ? organized.detail
                       : organizer_status_message(organize_status));
        organize_result_free(&organized);
        download_result_free(&download);
        return fatal;
    }
    if (organized.stored_path == NULL || organized.sha256_hex[0] == '\0' ||
        organized.bytes == 0U) {
        organize_result_free(&organized);
        if (remove(download.file_path) != 0) {
            (void)adopt_orphaned_path(result, &download.file_path);
        }
        download_result_free(&download);
        return PIPELINE_ERROR_INTERNAL;
    }

    outcome->outcome = organize_status == ORGANIZER_NEW_FILE
        ? REPORT_ROW_DOWNLOADED_NEW : REPORT_ROW_DUPLICATE;
    (void)snprintf(outcome->sha256_hex, sizeof(outcome->sha256_hex), "%s",
                   organized.sha256_hex);
    outcome->size_bytes = organized.bytes;
    outcome->stored_path = organized.stored_path;
    organized.stored_path = NULL;
    if (organize_status == ORGANIZER_NEW_FILE) {
        if (!add_size(&stats->newly_stored_documents, 1U) ||
            !add_size(&stats->bytes_newly_stored, organized.bytes)) {
            organize_result_free(&organized);
            if (remove(download.file_path) != 0) {
                (void)adopt_orphaned_path(result, &download.file_path);
            }
            download_result_free(&download);
            return PIPELINE_ERROR_INTERNAL;
        }
    } else if (!add_size(&stats->duplicate_content_documents, 1U)) {
        organize_result_free(&organized);
        if (remove(download.file_path) != 0) {
            (void)adopt_orphaned_path(result, &download.file_path);
        }
        download_result_free(&download);
        return PIPELINE_ERROR_INTERNAL;
    }
    organize_result_free(&organized);

    if (remove(download.file_path) != 0) {
        (void)adopt_orphaned_path(result, &download.file_path);
        set_detail(result->detail,
                   "could not remove a completed staging file after organization");
        download_result_free(&download);
        return PIPELINE_ERROR_FILESYSTEM;
    }
    download_result_free(&download);
    return PIPELINE_OK;
}

static PipelineStatus build_report_rows(
    const DocumentResults *documents, const unsigned char *document_selected,
    const unsigned char *source_selected, const DocumentOutcome *outcomes,
    size_t selected_relationships, ReportRow **rows_out)
{
    size_t document_count = document_results_count(documents);
    size_t source_index = 0U, row_index = 0U, document_index;
    PipelineStatus status;
    *rows_out = NULL;
    status = allocate_zeroed(selected_relationships, sizeof(**rows_out),
                             (void **)rows_out);
    if (status != PIPELINE_OK) { return status; }
    for (document_index = 0U; document_index < document_count;
         ++document_index) {
        const DocumentRecord *record =
            document_results_at(documents, document_index);
        size_t source_in_record;
        for (source_in_record = 0U; source_in_record < record->source_count;
             ++source_in_record, ++source_index) {
            ReportRow *row;
            const DocumentOutcome *outcome;
            if (!source_selected[source_index]) { continue; }
            if (row_index >= selected_relationships ||
                !document_selected[document_index]) {
                free(*rows_out);
                *rows_out = NULL;
                return PIPELINE_ERROR_INTERNAL;
            }
            row = &(*rows_out)[row_index++];
            outcome = &outcomes[document_index];
            row->document_url = record->url;
            row->document_type = record->type;
            row->source_page_url = record->sources[source_in_record].page_url;
            row->source_depth = record->sources[source_in_record].depth;
            row->outcome = outcome->outcome;
            row->sha256_hex = outcome->sha256_hex;
            row->size_bytes = outcome->size_bytes;
            row->stored_path = outcome->stored_path != NULL
                ? outcome->stored_path : "";
            row->error_detail = outcome->error_detail;
        }
    }
    if (row_index != selected_relationships) {
        free(*rows_out);
        *rows_out = NULL;
        return PIPELINE_ERROR_INTERNAL;
    }
    return PIPELINE_OK;
}

static PipelineStatus map_report_failure(ReportStatus status)
{
    if (status == REPORT_ERROR_OUT_OF_MEMORY) {
        return PIPELINE_ERROR_OUT_OF_MEMORY;
    }
    if (status == REPORT_ERROR_OVERFLOW) { return PIPELINE_ERROR_INTERNAL; }
    return PIPELINE_ERROR_REPORT;
}

PipelineStatus pipeline_run(const PipelineConfig *config,
                            PipelineResult *result)
{
    DocumentResults documents = {0};
    CrawlerStats crawler_stats = {0};
    CrawlerConfig crawler_config;
    ReportFilter filter;
    ReportResult report_result = {0};
    ReportStatus report_status;
    DocumentStatus document_status;
    PipelineStatus status;
    PipelinePathKind path_kind;
    char *preflight_report_path = NULL;
    char *staging_directory = NULL;
    unsigned char *document_selected = NULL;
    unsigned char *source_selected = NULL;
    DocumentOutcome *outcomes = NULL;
    ReportRow *rows = NULL;
    size_t selected_relationships = 0U;
    size_t document_count, source_count, document_index, stage_serial = 0U;
    size_t selected_ordinal = 0U, source_index;
    int partial = 0, limit_reached = 0;

    if (result == NULL || !result_is_empty(result)) {
        return PIPELINE_ERROR_INVALID_ARGUMENT;
    }
    status = validate_config(config);
    if (status != PIPELINE_OK) {
        set_result(result, status, NULL);
        return status;
    }
    if (inspect_path(config->output_root) != PIPELINE_PATH_DIRECTORY) {
        set_result(result, PIPELINE_ERROR_FILESYSTEM,
                   "output root must be an existing trusted directory");
        return PIPELINE_ERROR_FILESYSTEM;
    }
    status = join_path(config->output_root, config->report_filename,
                       &preflight_report_path);
    if (status != PIPELINE_OK) {
        set_result(result, status, NULL);
        return status;
    }
    path_kind = inspect_path(preflight_report_path);
    if (path_kind != PIPELINE_PATH_MISSING) {
        free(preflight_report_path);
        if (path_kind == PIPELINE_PATH_ERROR) {
            set_result(result, PIPELINE_ERROR_FILESYSTEM,
                       "could not check the requested report path");
            return PIPELINE_ERROR_FILESYSTEM;
        }
        set_result(result, PIPELINE_ERROR_REPORT,
                   "the requested report file already exists");
        return PIPELINE_ERROR_REPORT;
    }
    free(preflight_report_path);
    preflight_report_path = NULL;

    {
        OrganizerStatus directory_status =
            organizer_prepare_directories(config->output_root);
        if (directory_status != ORGANIZER_OK) {
            status = directory_status == ORGANIZER_ERROR_OUT_OF_MEMORY
                ? PIPELINE_ERROR_OUT_OF_MEMORY : PIPELINE_ERROR_FILESYSTEM;
            set_result(result, status, organizer_status_message(directory_status));
            return status;
        }
    }
    status = ensure_staging_directory(config->output_root,
                                      &staging_directory);
    if (status != PIPELINE_OK) {
        set_result(result, status, NULL);
        return status;
    }

    if (document_results_init(&documents) != DOCUMENT_OK) {
        status = PIPELINE_ERROR_INTERNAL;
        set_result(result, status, NULL);
        goto cleanup;
    }
    crawler_config.start_url = config->start_url;
    crawler_config.max_depth = config->max_depth;
    crawler_config.same_domain = config->same_domain;
    crawler_config.timeout_seconds = config->timeout_seconds;
    document_status = document_discover(&crawler_config, &documents,
                                        &crawler_stats);
    copy_crawler_stats(&result->stats, &crawler_stats);
    if (document_status == DOCUMENT_ERROR_OUT_OF_MEMORY) {
        status = PIPELINE_ERROR_OUT_OF_MEMORY;
        set_result(result, status, document_status_message(document_status));
        goto cleanup;
    }
    if (document_status == DOCUMENT_ERROR_CRAWL) {
        status = PIPELINE_ERROR_CRAWL;
        set_result(result, status, document_status_message(document_status));
        goto cleanup;
    }
    if (document_status == DOCUMENT_ERROR_OVERFLOW) {
        status = PIPELINE_ERROR_INTERNAL;
        set_result(result, status, document_status_message(document_status));
        goto cleanup;
    }
    if (document_status != DOCUMENT_OK &&
        document_status != DOCUMENT_PARTIAL &&
        document_status != DOCUMENT_LIMIT_REACHED) {
        status = PIPELINE_ERROR_INVALID_ARGUMENT;
        set_result(result, status, document_status_message(document_status));
        goto cleanup;
    }
    if (document_status == DOCUMENT_PARTIAL) { partial = 1; }
    if (document_status == DOCUMENT_LIMIT_REACHED) { limit_reached = 1; }

    document_count = document_results_count(&documents);
    source_count = document_results_source_count(&documents);
    filter.keywords = config->keywords;
    filter.keyword_count = config->keyword_count;
    status = allocate_selection(&documents, &filter, &result->stats,
                                &document_selected, &source_selected,
                                &outcomes);
    if (status != PIPELINE_OK) {
        set_result(result, status, NULL);
        goto cleanup;
    }
    selected_relationships =
        result->stats.source_document_relationships_selected;
    if (result->stats.unique_document_urls_selected >
        PIPELINE_MAX_DOCUMENT_ATTEMPTS) {
        limit_reached = 1;
    }

    for (document_index = 0U; document_index < document_count;
         ++document_index) {
        const DocumentRecord *record;
        PipelineStatus item_status;
        if (!document_selected[document_index]) { continue; }
        record = document_results_at(&documents, document_index);
        if (selected_ordinal >= PIPELINE_MAX_DOCUMENT_ATTEMPTS) {
            outcomes[document_index].outcome = REPORT_ROW_SKIPPED_LIMIT;
            set_detail(outcomes[document_index].error_detail,
                       "the ten-document download limit was reached");
            if (!add_size(&result->stats.documents_skipped_limit, 1U)) {
                status = PIPELINE_ERROR_INTERNAL;
                set_result(result, status, NULL);
                goto cleanup;
            }
            ++selected_ordinal;
            continue;
        }
        ++selected_ordinal;
        item_status = process_one_document(
            config, staging_directory, record, &outcomes[document_index],
            &result->stats, result, &stage_serial, &partial);
        if (item_status != PIPELINE_OK) {
            status = item_status;
            if (result->detail[0] == '\0') {
                set_result(result, status, NULL);
            } else {
                result->status = status;
            }
            goto cleanup;
        }
    }
    if (source_count == 0U && document_count != 0U) {
        status = PIPELINE_ERROR_INTERNAL;
        set_result(result, status, NULL);
        goto cleanup;
    }

    status = build_report_rows(&documents, document_selected, source_selected,
                               outcomes, selected_relationships, &rows);
    if (status != PIPELINE_OK) {
        set_result(result, status, NULL);
        goto cleanup;
    }
    (void)report_result_init(&report_result);
    report_status = report_write_csv(config->output_root,
                                     config->report_filename, rows,
                                     selected_relationships, &filter,
                                     &report_result);
    if (report_status != REPORT_OK) {
        if (report_result.output_path != NULL) {
            result->report_path = report_result.output_path;
            report_result.output_path = NULL;
        }
        status = map_report_failure(report_status);
        set_result(result, status,
                   report_result.detail[0] != '\0' ? report_result.detail
                       : report_status_message(report_status));
        goto cleanup;
    }
    result->report_path = report_result.output_path;
    report_result.output_path = NULL;
    result->stats.csv_rows_written = report_result.rows_written;
    if (report_result.rows_excluded != 0U ||
        report_result.rows_written != selected_relationships) {
        status = PIPELINE_ERROR_INTERNAL;
        set_result(result, status,
                   "CSV row count did not match selected provenance rows");
        goto cleanup;
    }

    status = limit_reached ? PIPELINE_LIMIT_REACHED
        : partial ? PIPELINE_PARTIAL : PIPELINE_OK;
    if (status == PIPELINE_LIMIT_REACHED) {
        set_result(result, status, NULL);
    } else if (status == PIPELINE_PARTIAL) {
        set_result(result, status,
                   "crawl or one or more selected document operations were partial");
    } else {
        set_result(result, PIPELINE_OK, NULL);
    }

cleanup:
    free(preflight_report_path);
    free(staging_directory);
    free(document_selected);
    free(source_selected);
    free(rows);
    if (outcomes != NULL) {
        for (source_index = 0U; source_index < document_count; ++source_index) {
            free(outcomes[source_index].stored_path);
        }
    }
    free(outcomes);
    document_results_free(&documents);
    report_result_free(&report_result);
    return status;
}
