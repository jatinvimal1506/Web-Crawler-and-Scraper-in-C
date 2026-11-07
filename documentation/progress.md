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

## Part 4 — Implemented as standalone HTML hyperlink extraction

- Public `HtmlLinks`, `HtmlStatus`, extraction, cleanup and status-message API.
- Anchor href scanning, quoted/unquoted values, whitespace and practical entities.
- Existing URL resolution and HTTP(S) validation; duplicate occurrences preserved.
- Deterministic normal, malformed, boundary, empty and ownership tests.
- Linux Makefile support while preserving Windows/MSYS2 commands.
- Targeted unquoted query-string href handling and malformed-attribute regressions.
- Authorized HTTP safety fixes: exact HTML MIME tokens and HTTP(S)-only redirects.
- C-only loopback HTTP fixtures for redirects, MIME rejection and response limits.
- HTTP metadata-copy OOM cleanup and deterministic allocator-failure regressions.
- Raw-text iframe/xmp/noembed/noframes exclusions and plaintext handling.
- Entity-aware unquoted query context (`&#63;`, hexadecimal references, `&quest;`).
- Beginner guide: `documentation/phase4.md`.

## Parts not implemented in this working branch

- Part 5: crawl queue and visited URL tracking.
- Part 6: BFS crawler integration and live crawl statistics.
- Part 7: integrated crawl testing and final system validation.

Parts 3–4 are not yet integrated into `crawler_run`, which continues to return
`CRAWLER_NOT_IMPLEMENTED` for valid configurations. Queue and visited remain
placeholders. IPv6, internationalized names and exhaustive RFC 3986 conformance
remain outside the existing URL module's scope. The optional Phase 4 allocation
test also exercises failures in the URL resolution allocations it invokes.
