#include "document.h"
#include "url.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int is_empty(const DocumentResults *results)
{
    return results->items == NULL && results->count == 0 &&
           results->capacity == 0 && results->source_count == 0;
}

static int text_equal_ci(const char *text, const char *word)
{
    while (*text != '\0' && *word != '\0') {
        unsigned char a = (unsigned char)*text++;
        unsigned char b = (unsigned char)*word++;
        if (a >= 'A' && a <= 'Z') { a = (unsigned char)(a + ('a' - 'A')); }
        if (b >= 'A' && b <= 'Z') { b = (unsigned char)(b + ('a' - 'A')); }
        if (a != b) { return 0; }
    }
    return *text == '\0' && *word == '\0';
}

const char *document_type_name(DocumentType type)
{
    switch (type) {
    case DOCUMENT_TYPE_PDF: return "PDF";
    case DOCUMENT_TYPE_DOCX: return "DOCX";
    case DOCUMENT_TYPE_ZIP: return "ZIP";
    case DOCUMENT_TYPE_UNKNOWN: return "UNKNOWN";
    default: return "INVALID";
    }
}

const char *document_status_message(DocumentStatus status)
{
    switch (status) {
    case DOCUMENT_OK: return "success";
    case DOCUMENT_PARTIAL: return "crawl completed with failed HTML pages";
    case DOCUMENT_LIMIT_REACHED: return "document/crawl budget reached (incomplete)";
    case DOCUMENT_ERROR_INVALID_ARGUMENT: return "invalid argument or nonempty result";
    case DOCUMENT_ERROR_INVALID_URL: return "invalid or unsupported HTTP(S) URL";
    case DOCUMENT_ERROR_OUT_OF_MEMORY: return "out of memory";
    case DOCUMENT_ERROR_CRAWL: return "root crawl or crawler failure";
    case DOCUMENT_ERROR_OVERFLOW: return "result size overflow";
    default: return "unknown document-discovery error";
    }
}

DocumentStatus document_type_from_url(const char *url, DocumentType *type)
{
    ParsedUrl parsed = {0};
    UrlStatus status;
    const char *dot, *slash;
    DocumentStatus result = DOCUMENT_OK;
    if (type == NULL) { return DOCUMENT_ERROR_INVALID_ARGUMENT; }
    *type = DOCUMENT_TYPE_UNKNOWN;
    if (url == NULL || url[0] == '\0') {
        return DOCUMENT_ERROR_INVALID_ARGUMENT;
    }
    status = url_parse(url, &parsed);
    if (status != URL_OK) {
        return status == URL_ERROR_OUT_OF_MEMORY
            ? DOCUMENT_ERROR_OUT_OF_MEMORY : DOCUMENT_ERROR_INVALID_URL;
    }
    dot = strrchr(parsed.path, '.');
    slash = strrchr(parsed.path, '/');
    if (dot != NULL && (slash == NULL || dot > slash)) {
        if (text_equal_ci(dot, ".pdf")) { *type = DOCUMENT_TYPE_PDF; }
        else if (text_equal_ci(dot, ".docx")) { *type = DOCUMENT_TYPE_DOCX; }
        else if (text_equal_ci(dot, ".zip")) { *type = DOCUMENT_TYPE_ZIP; }
    }
    url_free(&parsed);
    return result;
}

DocumentStatus document_results_init(DocumentResults *results)
{
    if (results == NULL) { return DOCUMENT_ERROR_INVALID_ARGUMENT; }
    if (!is_empty(results)) { return DOCUMENT_ERROR_INVALID_ARGUMENT; }
    *results = (DocumentResults){0};
    return DOCUMENT_OK;
}

void document_results_free(DocumentResults *results)
{
    size_t i, j;
    if (results == NULL) { return; }
    for (i = 0; i < results->count; ++i) {
        DocumentRecord *record = &results->items[i];
        free(record->url);
        for (j = 0; j < record->source_count; ++j) {
            free(record->sources[j].page_url);
        }
        free(record->sources);
    }
    free(results->items);
    *results = (DocumentResults){0};
}

size_t document_results_count(const DocumentResults *results)
{
    return results == NULL ? 0 : results->count;
}

size_t document_results_source_count(const DocumentResults *results)
{
    return results == NULL ? 0 : results->source_count;
}

const DocumentRecord *document_results_at(const DocumentResults *results,
                                          size_t index)
{
    if (results == NULL || index >= results->count) { return NULL; }
    return &results->items[index];
}

static DocumentStatus reserve_sources(DocumentRecord *record)
{
    size_t capacity;
    DocumentSource *sources;
    if (record->source_count < record->source_capacity) { return DOCUMENT_OK; }
    capacity = record->source_capacity == 0 ? 2 : record->source_capacity * 2;
    if (capacity < record->source_capacity ||
        capacity > SIZE_MAX / sizeof(*record->sources)) {
        return DOCUMENT_ERROR_OVERFLOW;
    }
    sources = realloc(record->sources, capacity * sizeof(*sources));
    if (sources == NULL) { return DOCUMENT_ERROR_OUT_OF_MEMORY; }
    record->sources = sources;
    record->source_capacity = capacity;
    return DOCUMENT_OK;
}


static DocumentStatus add_occurrence(DocumentResults *results,
                                     const char *document_url,
                                     const char *source_url,
                                     size_t source_depth,
                                     DocumentType type)
{
    char *document_key = NULL, *source_key = NULL;
    UrlStatus url_status;
    DocumentRecord *record = NULL;
    DocumentSource *new_sources;
    size_t i, j, new_capacity;
    DocumentStatus result;

    url_status = url_normalize(document_url, &document_key);
    if (url_status != URL_OK) {
        return url_status == URL_ERROR_OUT_OF_MEMORY
            ? DOCUMENT_ERROR_OUT_OF_MEMORY : DOCUMENT_ERROR_INVALID_URL;
    }
    url_status = url_normalize(source_url, &source_key);
    if (url_status != URL_OK) {
        free(document_key);
        return url_status == URL_ERROR_OUT_OF_MEMORY
            ? DOCUMENT_ERROR_OUT_OF_MEMORY : DOCUMENT_ERROR_INVALID_URL;
    }
    for (i = 0; i < results->count; ++i) {
        if (strcmp(results->items[i].url, document_key) == 0) {
            record = &results->items[i];
            break;
        }
    }
    if (record != NULL) {
        for (j = 0; j < record->source_count; ++j) {
            if (strcmp(record->sources[j].page_url, source_key) == 0) {
                free(document_key);
                free(source_key);
                return DOCUMENT_OK;
            }
        }
    }
    if (results->source_count >= DOCUMENT_MAX_OCCURRENCES) {
        free(document_key);
        free(source_key);
        return DOCUMENT_LIMIT_REACHED;
    }
    if (record != NULL) {
        result = reserve_sources(record);
        if (result != DOCUMENT_OK) {
            free(document_key);
            free(source_key);
            return result;
        }
        free(document_key);
        record->sources[record->source_count++] =
            (DocumentSource){source_key, source_depth};
        ++results->source_count;
        return DOCUMENT_OK;
    }



    new_sources = malloc(2 * sizeof(*new_sources));
    if (new_sources == NULL) {
        free(document_key);
        free(source_key);
        return DOCUMENT_ERROR_OUT_OF_MEMORY;
    }
    new_capacity = results->capacity;
    if (results->count == new_capacity) {
        new_capacity = new_capacity == 0 ? 4 : new_capacity * 2;
        if (new_capacity < results->capacity ||
            new_capacity > SIZE_MAX / sizeof(*results->items)) {
            free(new_sources);
            free(document_key);
            free(source_key);
            return DOCUMENT_ERROR_OVERFLOW;
        }
        DocumentRecord *new_items =
            realloc(results->items, new_capacity * sizeof(*new_items));
        if (new_items == NULL) {
            free(new_sources);
            free(document_key);
            free(source_key);
            return DOCUMENT_ERROR_OUT_OF_MEMORY;
        }
        results->items = new_items;
        results->capacity = new_capacity;
    }
    results->items[results->count++] = (DocumentRecord){
        document_key, type, new_sources, 1, 2
    };
    new_sources[0] = (DocumentSource){source_key, source_depth};
    ++results->source_count;
    return DOCUMENT_OK;
}

typedef struct {
    DocumentResults *results;
    DocumentStatus failure;
} DiscoveryContext;

static CrawlerStatus observe_link(const char *source_page_url,
                                  size_t source_depth,
                                  const char *link_url,
                                  int *crawl_link, void *opaque)
{
    DiscoveryContext *context = opaque;
    DocumentType type;
    DocumentStatus status;
    if (crawl_link == NULL || context == NULL) { return CRAWLER_INTERNAL_ERROR; }
    *crawl_link = 1;
    status = document_type_from_url(link_url, &type);
    if (status == DOCUMENT_ERROR_OUT_OF_MEMORY) {
        context->failure = status;
        return CRAWLER_OUT_OF_MEMORY;
    }
    if (status != DOCUMENT_OK) {


        context->failure = DOCUMENT_ERROR_INVALID_URL;
        return CRAWLER_INTERNAL_ERROR;
    }
    if (type == DOCUMENT_TYPE_UNKNOWN) { return CRAWLER_OK; }
    *crawl_link = 0;
    status = add_occurrence(context->results, link_url, source_page_url,
                            source_depth, type);
    if (status == DOCUMENT_OK) { return CRAWLER_OK; }
    context->failure = status;
    if (status == DOCUMENT_ERROR_OUT_OF_MEMORY) { return CRAWLER_OUT_OF_MEMORY; }
    if (status == DOCUMENT_LIMIT_REACHED) { return CRAWLER_LIMIT_REACHED; }
    return CRAWLER_INTERNAL_ERROR;
}

DocumentStatus document_discover(const CrawlerConfig *config,
                                 DocumentResults *results,
                                 CrawlerStats *stats)
{
    DiscoveryContext context;
    CrawlerStatus crawl_status;
    if (stats != NULL) { *stats = (CrawlerStats){0}; }
    if (results == NULL || stats == NULL) {
        return DOCUMENT_ERROR_INVALID_ARGUMENT;
    }
    if (!is_empty(results)) { return DOCUMENT_ERROR_INVALID_ARGUMENT; }
    if (config == NULL || config->start_url == NULL ||
        config->start_url[0] == '\0') {
        return DOCUMENT_ERROR_INVALID_ARGUMENT;
    }
    context = (DiscoveryContext){results, DOCUMENT_OK};
    crawl_status = crawler_run_with_link_observer(config, stats,
                                                   observe_link, &context);
    if (crawl_status == CRAWLER_OUT_OF_MEMORY) {
        document_results_free(results);
        return DOCUMENT_ERROR_OUT_OF_MEMORY;
    }
    if (context.failure == DOCUMENT_ERROR_OVERFLOW ||
        context.failure == DOCUMENT_ERROR_INVALID_URL) {
        document_results_free(results);
        return context.failure;
    }
    if (crawl_status == CRAWLER_LIMIT_REACHED) { return DOCUMENT_LIMIT_REACHED; }
    if (crawl_status == CRAWLER_PARTIAL) { return DOCUMENT_PARTIAL; }
    if (crawl_status != CRAWLER_OK) {
        return crawl_status == CRAWLER_INVALID_ARGUMENT
            ? DOCUMENT_ERROR_INVALID_ARGUMENT : DOCUMENT_ERROR_CRAWL;
    }
    return DOCUMENT_OK;
}
