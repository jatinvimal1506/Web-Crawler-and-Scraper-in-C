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
    CRAWLER_NOT_IMPLEMENTED
} CrawlerStatus;

typedef struct {
    const char *start_url;   /* Borrowed; must stay valid for crawler_run(). */
    size_t max_depth;        /* Root page has depth zero. */
    int same_domain;         /* 1 to restrict to the start host, 0 otherwise. */
    long timeout_seconds;    /* Must be greater than zero. */
} CrawlerConfig;

typedef struct {
    size_t pages_visited;
    size_t pages_failed;
    size_t links_discovered;
    size_t max_depth_reached;
    size_t bytes_downloaded;
} CrawlerStats;

/*
 * Run a crawl. config, start_url, and stats are borrowed for this call.
 * stats is caller-owned and is zeroed whenever stats is non-NULL, including
 * when validation fails. No caller-visible memory is allocated.
 * Returns CRAWLER_NOT_IMPLEMENTED for valid configurations until the crawl
 * engine is implemented.
 */
CrawlerStatus crawler_run(const CrawlerConfig *config, CrawlerStats *stats);

/* Return a static, human-readable message; the caller must not free it. */
const char *crawler_status_string(CrawlerStatus status);

#ifdef __cplusplus
}
#endif
#endif
