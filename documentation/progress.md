# Project progress

## Part 1 — Complete

- Modular C11 project and reusable static library build.
- Public crawler API, status codes, configuration and statistics structures.
- Argument validation, zero-initialized output statistics, status messages, separate CLI, Windows MSYS2 Make support, and foundational tests.

## Part 2 — Implemented; tested within documented scope

- HTTP/HTTPS parsing, validation, scheme/host/port/path/query/fragment extraction.
- Public `UrlStatus` values and `url_status_message`.
- Normalization of case, standard ports, dot segments, queries, fragments, and path separator/trailing-slash preservation.
- Common absolute and relative URL reference resolution.
- Explicit ownership rules and cleanup on parse, normalize, and resolve paths.
- Deterministic parser, boundary, resolution, cleanup, and Part 1 regression tests.

## Part 3 — Implemented as a standalone downloader

- Single-page HTTP(S) download through libcurl with redirects, timeout, and response size limits.
- HTTP status and HTML content-type validation with structured HTTP/network errors.
- Owned response body and metadata with explicit cleanup functions.
- `examples/demo_pipeline.c` demonstrates the public API, URL parser, and downloader sequentially.

## Parts not implemented in this working branch

- Part 4: HTML parsing, scraping and link extraction.
- Part 5: crawl queue and visited URL tracking.
- Part 6: BFS crawler integration and live crawl statistics.
- Part 7: integrated crawl testing and final system validation.

Part 3 is not yet integrated into `crawler_run`, which continues to return `CRAWLER_NOT_IMPLEMENTED` for valid configurations. The HTML, queue, and visited modules remain placeholders. IPv6, internationalized names, exhaustive RFC 3986 conformance, and deterministic allocation-failure injection are outside the current URL implementation/test scope.
