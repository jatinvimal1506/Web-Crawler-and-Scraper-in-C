# Web Crawler and Scraper in C

This repository is a university project organized as a reusable C11 library with separate demonstration programs. Parts 1–3 provide the public crawler API, HTTP(S) URL parsing, and single-page HTTP downloading. HTML extraction and recursive crawling are not yet integrated.

## Build on Windows

Use MSYS2 UCRT64 GCC and GNU Make with libcurl available.

```powershell
make
make test
```

The Makefile builds `build/libcrawler.a`, the existing `build/crawler-demo.exe`, the combined `build/demo_pipeline.exe`, and the test programs. Both demos link with libcurl.

Run the combined pipeline with the default URL or supply one:

```powershell
.\build\demo_pipeline.exe
.\build\demo_pipeline.exe https://example.com
```

## Part 1: public crawler API

`include/project.h` defines the crawler status, configuration, statistics, and `crawler_run` API. `src/project.c` clears statistics and validates configuration values. A valid call returns `CRAWLER_NOT_IMPLEMENTED`; the crawl engine is not integrated.

## Part 2: URL handling

`include/url.h` and `src/url.c` provide HTTP(S) URL parsing, validation, normalization, relative URL resolution, and cleanup. `ParsedUrl` exposes scheme, host, optional explicit port, path, query, and fragment. Parsed component strings are heap allocated and released with `url_free`.

## Part 3: HTTP downloader

`include/http.h` and `src/http.c` provide `http_download` for one HTTP(S) page. It uses libcurl, follows redirects, applies a timeout and body-size limit, and reports HTTP status, response size, content type, final URL, and structured failures. Successful response fields are owned by the caller and released with `http_response_free`; call `http_cleanup` when finished.

The pipeline demo calls the Part 1 API, then demonstrates URL parsing and the standalone downloader. The latter two APIs are called separately because `crawler_run` does not yet orchestrate them.

## Remaining work

HTML extraction, link discovery, queue and visited tracking, and recursive crawl integration remain future work. See `documentation/progress.md` and `documentation/architecture.md`.
