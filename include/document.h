#ifndef DEEPDOWNLOAD_DOCUMENT_H
#define DEEPDOWNLOAD_DOCUMENT_H

#include "project.h"
#include <stddef.h>


#define DOCUMENT_MAX_OCCURRENCES ((size_t)1000)

typedef enum {
    DOCUMENT_TYPE_UNKNOWN = 0,
    DOCUMENT_TYPE_PDF,
    DOCUMENT_TYPE_DOCX,
    DOCUMENT_TYPE_ZIP
} DocumentType;

typedef enum {
    DOCUMENT_OK = 0,
    DOCUMENT_PARTIAL,
    DOCUMENT_LIMIT_REACHED,
    DOCUMENT_ERROR_INVALID_ARGUMENT,
    DOCUMENT_ERROR_INVALID_URL,
    DOCUMENT_ERROR_OUT_OF_MEMORY,
    DOCUMENT_ERROR_CRAWL,
    DOCUMENT_ERROR_OVERFLOW
} DocumentStatus;

typedef struct {
    char *page_url;
    size_t depth;
} DocumentSource;

typedef struct {
    char *url;
    DocumentType type;
    DocumentSource *sources;
    size_t source_count;
    size_t source_capacity;
} DocumentRecord;

typedef struct {
    DocumentRecord *items;
    size_t count;
    size_t capacity;
    size_t source_count;
} DocumentResults;


DocumentStatus document_type_from_url(const char *url, DocumentType *type);
const char *document_type_name(DocumentType type);
const char *document_status_message(DocumentStatus status);
DocumentStatus document_results_init(DocumentResults *results);
void document_results_free(DocumentResults *results);
size_t document_results_count(const DocumentResults *results);
size_t document_results_source_count(const DocumentResults *results);
const DocumentRecord *document_results_at(const DocumentResults *results, size_t index);
DocumentStatus document_discover(const CrawlerConfig *config,DocumentResults *results, CrawlerStats *stats);

#endif
