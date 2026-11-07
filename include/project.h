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

/* Fixed demo-safety budgets; no extra/uninitialized config fields. */
#define CRAWLER_MAX_PAGES ((size_t)100)
#define CRAWLER_MAX_CANDIDATES ((size_t)1000)
#define CRAWLER_MAX_SCHEDULED ((size_t)1000)

typedef struct {
    const char *start_url;   /* Borrowed; must stay valid for crawler_run(). */
    size_t max_depth;        /* Root page has depth zero. */
    int same_domain;         /* Exact seed hostname if 1. Both modes pin port. */
    long timeout_seconds;    /* Must be greater than zero. */
} CrawlerConfig;

typedef struct {
    size_t pages_visited;     /* HTTP_OK + complete successful extraction. */
    size_t pages_failed;      /* Attempted downloads/parses/scope checks failed. */
    size_t links_discovered;  /* Entries from complete extracted pages; duplicates included. */
    size_t max_depth_reached; /* Maximum depth actually attempted. */
    size_t bytes_downloaded;  /* HTTP_OK body bytes, even if later parsing fails. */
} CrawlerStats;

/*
 * Run a crawl. config, start_url, and stats are borrowed for this call.
 * stats is caller-owned and is zeroed whenever stats is non-NULL, including
 * when validation fails. No caller-visible memory is allocated.
 * Single-threaded BFS, depth 0 root, no automatic retries. SAME_DOMAIN pins
 * exact canonical hostname; effective seed port is pinned in BOTH modes.
 * Redirect scope is checked AFTER download (not network-level isolation).
 * Root failures return an error; non-root failures continue, ending PARTIAL.
 * Budgets return LIMIT_REACHED, never an exhaustive-success claim.
 */
CrawlerStatus crawler_run(const CrawlerConfig *config, CrawlerStats *stats);

/* Return a static, human-readable message; the caller must not free it. */
const char *crawler_status_string(CrawlerStatus status);

#ifdef __cplusplus
}
#endif
#endif
