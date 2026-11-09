#ifndef DEEPDOWNLOAD_DOCUMENT_H
#define DEEPDOWNLOAD_DOCUMENT_H

#include "project.h"
#include <stddef.h>

/* A run retains at most 1000 distinct (document URL, source page) pairs. */
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
    char *page_url;     /* Owned normalized source-page URL. */
    size_t depth;       /* Depth of that successful HTML source page. */
} DocumentSource;

typedef struct {
    char *url;                  /* Owned normalized document URL. */
    DocumentType type;
    DocumentSource *sources;    /* Owned dynamic array of owned page_url strings. */
    size_t source_count;
    size_t source_capacity;     /* Implementation storage; do not modify. */
} DocumentRecord;

/*
 * Caller owns this structure and all nested allocations. Start with {0}, or
 * call document_results_init() on a zeroed object. Do not shallow-copy or
 * directly modify a populated result. Keep arrays read-only; use accessors.
 */
typedef struct {
    DocumentRecord *items;  /* In first-discovery order; every entry is unique. */
    size_t count;           /* Number of unique normalized document URLs. */
    size_t capacity;        /* Implementation storage; do not modify. */
    size_t source_count;    /* Distinct (document URL, source page URL) pairs. */
} DocumentResults;

/*
 * Parse an absolute HTTP(S) URL and classify its final PATH extension.
 * Query/fragment are not used to decide type. A valid unrecognized suffix
 * (including /download?id=123, .doc or .pdfx) returns DOCUMENT_OK and UNKNOWN.
 * Output is always set to UNKNOWN before any failure.
 */
DocumentStatus document_type_from_url(const char *url, DocumentType *type);
const char *document_type_name(DocumentType type); /* Static string; do not free. */
const char *document_status_message(DocumentStatus status); /* Static string. */

/* Initialize only {0}/empty objects; does not allocate. */
DocumentStatus document_results_init(DocumentResults *results);
/* NULL-safe, repeatable; frees every URL, source string and array, resets it. */
void document_results_free(DocumentResults *results);
size_t document_results_count(const DocumentResults *results);
size_t document_results_source_count(const DocumentResults *results);
const DocumentRecord *document_results_at(const DocumentResults *results,
                                          size_t index);

/*
 * Run existing bounded BFS and observe its already-resolved HTTP(S) links.
 * Recognized documents are never scheduled/fetched; only successful,
 * in-scope HTML source pages are attributed. Inputs are borrowed. Initialize
 * results as {0}; it must be empty. stats is caller-owned and zeroed when
 * possible. On OOM/overflow, all result allocations are rolled back to empty.
 * Child page failures yield DOCUMENT_PARTIAL with successful results retained;
 * crawl/discovery budget exhaustion yields DOCUMENT_LIMIT_REACHED with
 * bounded results retained. Results preserve first-discovery/source order.
 */
DocumentStatus document_discover(const CrawlerConfig *config,
                                 DocumentResults *results,
                                 CrawlerStats *stats);

#endif
