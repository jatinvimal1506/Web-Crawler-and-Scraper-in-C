# Public API

The public interface is declared in `include/project.h`. Include that header and link the static library, `build/libcrawler.a`.

## Configuration

`CrawlerConfig` fields:

- `start_url`: required non-empty string, borrowed for the duration of `crawler_run`.
- `max_depth`: maximum planned crawl depth; the root is depth 0.
- `same_domain`: must be 0 (allow other domains) or 1 (restrict to the starting host).
- `timeout_seconds`: must be greater than zero.

## Statistics

`CrawlerStats` is caller-owned. It contains pages visited, pages failed, links discovered, maximum depth reached, and bytes downloaded. The function sets every field to zero whenever the pointer is non-NULL, even when argument validation fails.

## Functions and statuses

`CrawlerStatus crawler_run(const CrawlerConfig *config, CrawlerStats *stats)`

Both pointers must be non-NULL. The URL must be non-NULL and non-empty. The domain flag and timeout must be in range. Invalid inputs return `CRAWLER_INVALID_ARGUMENT`. Valid configurations currently return `CRAWLER_NOT_IMPLEMENTED`; no network or parsing work is claimed.

`const char *crawler_status_string(CrawlerStatus status)`

Returns a static, readable message for each status. The caller must not modify or free the returned string. Unknown enum values return `unknown error`.

Defined statuses: `CRAWLER_OK`, `CRAWLER_INVALID_ARGUMENT`, `CRAWLER_OUT_OF_MEMORY`, `CRAWLER_NETWORK_ERROR`, `CRAWLER_PARSE_ERROR`, and `CRAWLER_NOT_IMPLEMENTED`.
