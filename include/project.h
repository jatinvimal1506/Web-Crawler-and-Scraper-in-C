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

/*
 * Optional per-link observation used by additive crawler integrations.
 * URL strings are borrowed and valid only until the observer returns.
 * Set *crawl_link to 0 to record/inspect a link without queuing it, or leave
 * it 1 to retain normal BFS behavior. Return CRAWLER_OK to continue; return
 * OUT_OF_MEMORY, LIMIT_REACHED or INTERNAL_ERROR to stop safely.
 */
typedef CrawlerStatus (*CrawlerLinkObserver)(
    const char *source_page_url, size_t source_depth, const char *link_url,
    int *crawl_link, void *context);

/* Fixed demo-safety budgets; no extra/uninitialized config fields. */
#define CRAWLER_MAX_PAGES ((size_t)100)
#define CRAWLER_MAX_CANDIDATES ((size_t)1000)
#define CRAWLER_MAX_SCHEDULED ((size_t)1000)

typedef struct {
    const char *start_url;   /* Borrowed; must stay valid for crawler_run(). */
    size_t max_depth;        /* Root page has depth zero. */
    int same_domain;         /* Exact seed hostname if 1. Both modes pin port. */
    long timeout_seconds;    /* 1..3600 seconds, inclusive, per HTTP transfer. */
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
 * Each redirect target is checked BEFORE its request. Not an IP/DNS firewall.
 * Root failures return an error; non-root failures continue, ending PARTIAL.
 * Budgets return LIMIT_REACHED, never an exhaustive-success claim.
 */
CrawlerStatus crawler_run(const CrawlerConfig *config, CrawlerStats *stats);

/*
 * Same bounded BFS, with a synchronous observer for each valid in-scope
 * absolute link. The observer sees links even at max_depth; ordinary links
 * are not queued beyond max_depth. crawler_run() remains unchanged and uses
 * no observer. No observer-owned pointer is retained by the crawler.
 */
CrawlerStatus crawler_run_with_link_observer(
    const CrawlerConfig *config, CrawlerStats *stats,
    CrawlerLinkObserver observer, void *context);

/* Return a static, human-readable message; the caller must not free it. */
const char *crawler_status_string(CrawlerStatus status);

#ifdef __cplusplus
}
#endif
#endif
