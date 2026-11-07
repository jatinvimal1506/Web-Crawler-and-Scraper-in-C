#include "project.h"
#include "html.h"
#include "http.h"
#include "queue.h"
#include "url.h"
#include "visited.h"
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

const char *crawler_status_string(CrawlerStatus status)
{
    switch (status) {
    case CRAWLER_OK: return "success";
    case CRAWLER_INVALID_ARGUMENT: return "invalid argument";
    case CRAWLER_OUT_OF_MEMORY: return "out of memory";
    case CRAWLER_NETWORK_ERROR: return "network error";
    case CRAWLER_PARSE_ERROR: return "parse error";
    case CRAWLER_NOT_IMPLEMENTED: return "not implemented";
    case CRAWLER_PARTIAL: return "completed with failed pages";
    case CRAWLER_LIMIT_REACHED: return "crawl budget/depth ceiling reached (incomplete)";
    case CRAWLER_INTERNAL_ERROR: return "crawler invariant failure";
    default: return "unknown error";
    }
}

typedef struct {
    UrlQueue pending;
    VisitedSet scheduled; /* Includes discovered keys and known redirect aliases. */
    VisitedSet attempted; /* Skips queued direct URLs learned from redirects. */
    ParsedUrl seed;
    const CrawlerConfig *config;
    CrawlerStats *stats;
    size_t attempts;
} Crawl;

/* All counters are checked even though fixed budgets make overflow unlikely. */
static int add_count(size_t *counter, size_t amount)
{
    if (amount > SIZE_MAX - *counter) {
        return 0;
    }
    *counter += amount;
    return 1;
}

static CrawlerStatus visited_error(VisitedStatus status)
{
    return status == VISITED_ERROR_OUT_OF_MEMORY
        ? CRAWLER_OUT_OF_MEMORY : CRAWLER_INTERNAL_ERROR;
}

static unsigned effective_port(const ParsedUrl *url)
{
    return url->port != NULL ? (unsigned)strtoul(url->port, NULL, 10)
        : strcmp(url->scheme, "https") == 0 ? 443U : 80U;
}

/* Both modes pin the seed's effective port; exact host is additionally pinned
 * in same_domain mode. This is a parsing/scheduling policy, NOT a redirect
 * firewall: the existing downloader follows redirects before this check. */
static CrawlerStatus in_scope(const Crawl *crawl, const char *url, int *allowed)
{
    ParsedUrl parts = {0};
    UrlStatus status = url_parse(url, &parts);
    if (status != URL_OK) {
        return status == URL_ERROR_OUT_OF_MEMORY
            ? CRAWLER_OUT_OF_MEMORY : CRAWLER_PARSE_ERROR;
    }
    *allowed = effective_port(&parts) == effective_port(&crawl->seed) &&
        (!crawl->config->same_domain || strcmp(parts.host, crawl->seed.host) == 0);
    url_free(&parts);
    return CRAWLER_OK;
}

static CrawlerStatus remember(VisitedSet *set, const char *url)
{
    VisitedStatus status = visited_insert(set, url);
    return status == VISITED_ADDED || status == VISITED_PRESENT
        ? CRAWLER_OK : visited_error(status);
}

static CrawlerStatus schedule(Crawl *crawl, const char *url, int depth)
{
    VisitedStatus found = visited_contains(&crawl->scheduled, url);
    QueueStatus queued;
    VisitedStatus inserted;
    if (found == VISITED_PRESENT) {
        return CRAWLER_OK;
    }
    if (found != VISITED_NOT_FOUND) {
        return visited_error(found);
    }
    if (crawl->scheduled.count >= CRAWLER_MAX_SCHEDULED) {
        return CRAWLER_LIMIT_REACHED;
    }
    queued = queue_enqueue(&crawl->pending, url, depth);
    if (queued != QUEUE_OK) {
        return queued == QUEUE_ERROR_OUT_OF_MEMORY ? CRAWLER_OUT_OF_MEMORY
            : queued == QUEUE_ERROR_OVERFLOW ? CRAWLER_LIMIT_REACHED
            : CRAWLER_INTERNAL_ERROR;
    }
    inserted = visited_insert(&crawl->scheduled, url);
    /* If insert failed AFTER enqueue, the caller aborts and frees BOTH
     * containers. Never continue with this untracked queued item. */
    return inserted == VISITED_ADDED ? CRAWLER_OK : visited_error(inserted);
}

static CrawlerStatus http_failure(HttpStatus status)
{
    if (status == HTTP_ERR_OUT_OF_MEMORY) {
        return CRAWLER_OUT_OF_MEMORY;
    }
    if (status == HTTP_ERR_NOT_HTML) {
        return CRAWLER_PARSE_ERROR;
    }
    if (status == HTTP_ERR_BAD_URL || status == HTTP_ERR_INVALID_ARGUMENT) {
        return CRAWLER_INVALID_ARGUMENT;
    }
    return CRAWLER_NETWORK_ERROR;
}

/* One attempted page owns its response, extracted array and normalized final
 * key locally. A single cleanup block releases them on every path. */
static CrawlerStatus process_page(Crawl *crawl, const UrlQueueItem *item)
{
    HttpResponse response = {0};
    HtmlLinks links = {0};
    char *final = NULL;
    HttpStatus downloaded;
    HtmlStatus extracted;
    UrlStatus normalized;
    CrawlerStatus result = CRAWLER_OK;
    size_t i;
    int allowed;
    VisitedStatus alias;
    ++crawl->attempts; /* Caller checked the 100-attempt bound. */
    if ((size_t)item->depth > crawl->stats->max_depth_reached) {
        crawl->stats->max_depth_reached = (size_t)item->depth;
    }
    downloaded = http_download(item->url, crawl->config->timeout_seconds, &response);
    if (downloaded == HTTP_ERR_OUT_OF_MEMORY) {
        result = http_failure(downloaded);
        goto failed;
    }
    if (downloaded == HTTP_OK &&
        !add_count(&crawl->stats->bytes_downloaded, response.size)) {
        result = CRAWLER_LIMIT_REACHED;
        goto failed;
    }
    if (response.final_url == NULL) {
        result = downloaded == HTTP_OK ? CRAWLER_PARSE_ERROR : http_failure(downloaded);
        goto failed;
    }
    normalized = url_normalize(response.final_url, &final);
    if (normalized != URL_OK) {
        result = normalized == URL_ERROR_OUT_OF_MEMORY
            ? CRAWLER_OUT_OF_MEMORY : CRAWLER_PARSE_ERROR;
        goto failed;
    }
    result = in_scope(crawl, final, &allowed);
    if (result != CRAWLER_OK || !allowed) {
        if (result == CRAWLER_OK) {
            result = CRAWLER_PARSE_ERROR;
        }
        goto failed; /* Do NOT parse off-scope final bodies or their links. */
    }
    alias = visited_contains(&crawl->scheduled, final);
    if (alias != VISITED_PRESENT && alias != VISITED_NOT_FOUND) {
        result = visited_error(alias);
        goto failed;
    }
    if (alias == VISITED_NOT_FOUND &&
        crawl->scheduled.count >= CRAWLER_MAX_SCHEDULED) {
        result = CRAWLER_LIMIT_REACHED;
        goto failed;
    }
    result = remember(&crawl->scheduled, final);
    if (result != CRAWLER_OK) {
        goto failed;
    }
    result = remember(&crawl->attempted, final);
    if (result != CRAWLER_OK) {
        goto failed;
    }
    if (downloaded != HTTP_OK) {
        result = http_failure(downloaded);
        goto failed;
    }
    /* Remember known final aliases even on 404/non-HTML/parse failure, so a
     * queued direct URL is not an accidental retry of that failed target. */
    if (response.body == NULL || memchr(response.body, '\0', response.size) != NULL) {
        result = CRAWLER_PARSE_ERROR;
        goto failed;
    }
    extracted = html_extract_links_limited(response.body, final,
        CRAWLER_MAX_CANDIDATES - crawl->stats->links_discovered, &links);
    if (extracted != HTML_OK) {
        result = extracted == HTML_ERROR_OUT_OF_MEMORY ? CRAWLER_OUT_OF_MEMORY
            : extracted == HTML_ERROR_LIMIT ? CRAWLER_LIMIT_REACHED
            : CRAWLER_PARSE_ERROR;
        goto failed;
    }
    if (!add_count(&crawl->stats->pages_visited, 1) ||
        !add_count(&crawl->stats->links_discovered, links.count)) {
        result = CRAWLER_LIMIT_REACHED;
        goto done;
    }
    if ((size_t)item->depth >= crawl->config->max_depth || item->depth == INT_MAX) {
        if (item->depth == INT_MAX && crawl->config->max_depth > (size_t)INT_MAX &&
            links.count != 0) {
            result = CRAWLER_LIMIT_REACHED;
        }
        goto done;
    }
    for (i = 0; i < links.count; ++i) {
        char *key = NULL;
        normalized = url_normalize(links.urls[i], &key);
        if (normalized != URL_OK) {
            if (normalized == URL_ERROR_OUT_OF_MEMORY) {
                result = CRAWLER_OUT_OF_MEMORY;
                goto done;
            }
            continue; /* Invalid/unsupported references never get scheduled. */
        }
        result = in_scope(crawl, key, &allowed);
        if (result == CRAWLER_OK && allowed) {
            result = schedule(crawl, key, item->depth + 1);
        }
        free(key);
        if (result != CRAWLER_OK) {
            goto done;
        }
    }
    goto done;

failed:
    if (!add_count(&crawl->stats->pages_failed, 1)) {
        result = CRAWLER_LIMIT_REACHED;
    }
    /* Recoverable child failures continue; root and resource/invariant errors
     * abort. Final status becomes PARTIAL after draining if children failed. */
    if (crawl->attempts != 1 && result != CRAWLER_OUT_OF_MEMORY &&
        result != CRAWLER_INTERNAL_ERROR && result != CRAWLER_LIMIT_REACHED) {
        result = CRAWLER_OK;
    }
done:
    free(final);
    html_links_free(&links);
    http_response_free(&response);
    return result;
}

CrawlerStatus crawler_run(const CrawlerConfig *config, CrawlerStats *stats)
{
    Crawl crawl = {0};
    UrlQueueItem item = {0};
    char *start = NULL;
    CrawlerStatus result;
    UrlStatus normalized;
    if (stats != NULL) {
        *stats = (CrawlerStats){0};
    }
    if (config == NULL || stats == NULL || config->start_url == NULL ||
        config->start_url[0] == '\0' ||
        (config->same_domain != 0 && config->same_domain != 1) ||
        config->timeout_seconds <= 0) {
        return CRAWLER_INVALID_ARGUMENT;
    }
    crawl.config = config;
    crawl.stats = stats;
    normalized = url_normalize(config->start_url, &start);
    if (normalized != URL_OK) {
        return normalized == URL_ERROR_OUT_OF_MEMORY
            ? CRAWLER_OUT_OF_MEMORY : CRAWLER_INVALID_ARGUMENT;
    }
    normalized = url_parse(start, &crawl.seed);
    if (normalized != URL_OK) {
        result = normalized == URL_ERROR_OUT_OF_MEMORY
            ? CRAWLER_OUT_OF_MEMORY : CRAWLER_INVALID_ARGUMENT;
        goto done;
    }
    if (queue_init(&crawl.pending) != QUEUE_OK ||
        visited_init(&crawl.scheduled) != VISITED_OK ||
        visited_init(&crawl.attempted) != VISITED_OK) {
        result = CRAWLER_INTERNAL_ERROR;
        goto done;
    }
    result = schedule(&crawl, start, 0);
    while (result == CRAWLER_OK && crawl.pending.count != 0) {
        VisitedStatus seen;
        if (queue_dequeue(&crawl.pending, &item) != QUEUE_OK || item.depth < 0) {
            result = CRAWLER_INTERNAL_ERROR;
            break;
        }
        seen = visited_contains(&crawl.attempted, item.url);
        if (seen == VISITED_PRESENT) {
            queue_item_free(&item); /* Known redirect target already attempted. */
            continue;
        }
        if (seen != VISITED_NOT_FOUND) {
            result = visited_error(seen);
            break;
        }
        if (crawl.attempts >= CRAWLER_MAX_PAGES) {
            result = CRAWLER_LIMIT_REACHED;
            break;
        }
        result = remember(&crawl.attempted, item.url);
        if (result == CRAWLER_OK) {
            result = process_page(&crawl, &item);
        }
        queue_item_free(&item);
    }
    if (result == CRAWLER_OK && stats->pages_failed != 0) {
        result = CRAWLER_PARTIAL;
    }
done:
    queue_item_free(&item);
    queue_free(&crawl.pending);
    visited_free(&crawl.scheduled);
    visited_free(&crawl.attempted);
    url_free(&crawl.seed);
    free(start);
    return result;
}
