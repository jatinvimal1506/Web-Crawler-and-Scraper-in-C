#ifndef CRAWLER_H
#define CRAWLER_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CRAWLER_OK = 0,
    CRAWLER_INVALID_ARGUMENT,
    CRAWLER_OUT_OF_MEMORY,
    CRAWLER_NETWORK_ERROR,
    CRAWLER_PARSE_ERROR,
    CRAWLER_NOT_IMPLEMENTED
} CrawlerStatus;

typedef struct {
    const char *start_url;
    size_t max_depth;
    int same_domain;
    long timeout_seconds;
} CrawlerConfig;

typedef struct {
    size_t pages_visited;
    size_t pages_failed;
    size_t links_discovered;
    size_t max_depth_reached;
    size_t bytes_downloaded;
} CrawlerStats;

/* Run a crawl. Configuration strings are borrowed for the duration of the call.
 * Stats is caller-owned and is zeroed before work begins. */
CrawlerStatus crawler_run(const CrawlerConfig *config, CrawlerStats *stats);
const char *crawler_status_string(CrawlerStatus status);

#ifdef __cplusplus
}
#endif
#endif
