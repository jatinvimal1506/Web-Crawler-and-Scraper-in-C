# Public API

The Part 1 crawler interface is declared in `include/project.h`. `crawler_run(config, stats)` clears `stats` when supplied, validates both pointers, the start URL, the domain flag, and positive timeout. Valid configurations currently return `CRAWLER_NOT_IMPLEMENTED`; the function does not access the network. `crawler_status_string` returns a static message that must not be freed.

`CrawlerConfig` and `CrawlerStats` are caller-owned. `start_url` is borrowed during the call. Statistics fields are initialized to zero for valid and invalid calls whenever `stats` is non-NULL.

The Part 2 URL API is in `include/url.h`:

- `url_parse(input, result)`: fills a `ParsedUrl` on success; components are allocated and owned by the caller.
- `url_free(url)`: releases all parsed components and clears the pointers.
- `url_normalize(input, output)`: returns a heap-allocated canonical URL through `output`; caller uses `free`.
- `url_resolve(base, relative, output)`: returns an allocated absolute URL; caller uses `free`.

URL functions return 0 for success and -1 for invalid input or allocation failure. The implementation supports HTTP/HTTPS ASCII DNS hostnames and common URL reference forms. IPv6 literals and complete RFC 3986 edge behavior are outside the current scope.
