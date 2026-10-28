# Project progress

## Part 1 — Complete

- Modular C11 project and reusable static library build.
- Public crawler API, status codes, configuration and statistics structures.
- Argument validation, zero-initialized output statistics, and status messages.
- Separate demonstration CLI, Windows MSYS2 Make support, and foundational tests.

## Part 2 — Complete, subject to ongoing verification

- URL parsing into scheme, hostname, port, path, query and fragment.
- Validation of empty and oversized inputs, supported schemes, hostname syntax, characters and port range.
- Canonical normalization and common relative URL resolution.
- Parsed URL ownership/cleanup and URL unit tests.

## Parts not implemented in this working branch

- Part 3: HTTP downloading.
- Part 4: HTML parsing, scraping and link extraction.
- Part 5: crawl queue and visited URL tracking.
- Part 6: BFS crawler integration and live crawl statistics.
- Part 7: integrated network testing and final system validation. Foundational and URL tests exist.

The source modules for Parts 3–5 remain placeholders for teammates. The CLI reports that crawling is not implemented for a valid configuration. Concurrent crawling is an optional extension and has not been started.
