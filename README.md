# Web Crawler and Scraper in C

This repository is a university project organized as a reusable C11 library with a separate demonstration program. The current working scope is Part 1 (project foundation) and Part 2 (URL handling). HTTP downloading, HTML scraping, crawl data structures, and crawler integration are reserved for later team work.

## Build on Windows

Use MSYS2 UCRT64 GCC and GNU Make. The workspace includes a PowerShell-compatible Make wrapper and VS Code tasks.

```powershell
make clean
make
make test
```

The Makefile builds `build/libcrawler.a`, the `build/crawler-demo.exe` Part 1 demonstration, and tests. No third-party libraries are needed for Parts 1 and 2.

## Part 1: foundation

`include/project.h` defines the crawler status, configuration, statistics, and API. `src/project.c` clears statistics and validates required configuration values. A valid call returns `CRAWLER_NOT_IMPLEMENTED`, because crawling belongs to later parts. The demo accepts a starting URL and optional depth, then explains that crawling has not yet been implemented.

## Part 2: URL handling

`include/url.h` and `src/url.c` provide URL parsing, validation, normalization, relative URL resolution, and cleanup. `ParsedUrl` exposes scheme, host, optional port, path, query, and fragment. `url_normalize` lowercases scheme and host, removes standard ports and dot segments, ensures a path, retains a query, and omits the fragment. `url_resolve` supports absolute HTTP(S), protocol-relative, root-relative, directory-relative, query-only, and fragment-only references within the documented basic scope.

Returned component strings from `url_parse`, and result strings from normalize/resolve, are heap allocated. Call `url_free` for a parsed URL and `free` for a returned string.

## Planned work

Part 3 HTTP downloader; Part 4 HTML parser/scraper; Part 5 queue and visited tracking; Part 6 crawl engine; Part 7 integrated tests and final validation. The `http.c`, `html.c`, `queue.c`, and `visited.c` source files remain placeholders to preserve the planned module layout. See `documentation/progress.md` and `documentation/architecture.md`.
