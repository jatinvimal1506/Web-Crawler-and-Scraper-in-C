#ifndef CRAWLER_H
#define CRAWLER_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Status returned by the public crawler API. */
typedef enum {
    CRAWLER_OK = 0,
    CRAWLER_INVALID_ARGUMENT,
    CRAWLER_OUT_OF_MEMORY,
    CRAWLER_NETWORK_ERROR,
    CRAWLER_PARSE_ERROR,
    CRAWLER_NOT_IMPLEMENTED, /* Retained for source/numeric compatibility. */
    CRAWLER_PARTIAL,        /* Queue drained, but one or more pages failed. */
    CRAWLER_LIMIT_REACHED,  /* Deliberate budget/depth ceiling; incomplete. */
    CRAWLER_INTERNAL_ERROR  /* Container/invariant contract failure. */
} CrawlerStatus;

typedef CrawlerStatus (*CrawlerLinkObserver)(
    const char *source_page_url, size_t source_depth, const char *link_url,
    int *crawl_link, void *context);
#define CRAWLER_MAX_PAGES ((size_t)100)
#define CRAWLER_MAX_CANDIDATES ((size_t)1000)
#define CRAWLER_MAX_SCHEDULED ((size_t)1000)

typedef struct {
    const char *start_url;
    size_t max_depth;
    int same_domain;
    long timeout_seconds;
} CrawlerConfig;

typedef struct {
    size_t pages_visited;     /* HTTP_OK + complete successful extraction. */
    size_t pages_failed;      /* Attempted downloads/parses/scope checks failed. */
    size_t links_discovered;  /* Entries from complete extracted pages; duplicates included. */
    size_t max_depth_reached; /* Maximum depth actually attempted. */
    size_t bytes_downloaded;  /* HTTP_OK body bytes, even if later parsing fails. */
} CrawlerStats;
CrawlerStatus crawler_run(const CrawlerConfig *config, CrawlerStats *stats);
CrawlerStatus crawler_run_with_link_observer(
    const CrawlerConfig *config, CrawlerStats *stats,
    CrawlerLinkObserver observer, void *context);
const char *crawler_status_string(CrawlerStatus status);

#ifdef __cplusplus
}
#endif
#endif
