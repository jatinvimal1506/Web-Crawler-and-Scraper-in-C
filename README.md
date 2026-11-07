# DeepDownload-C

This university project remains a reusable C11 static library with separate CLI
demonstration programs. Parts 1–3 provide the public crawler API, HTTP(S) URL
parsing, and single-page HTTP downloading. Phase 4 now provides standalone HTML
anchor extraction. Recursive crawling is not yet integrated.

## Build on Linux / Replit

GCC, GNU Make and libcurl development files are required.

```sh
cd DeepDownload-C # from the surrounding Replit project; omit if already here
make
make test-unit   # deterministic API, URL and HTML tests
make test-http-local # C-only loopback HTTP fixtures (POSIX; Windows prints SKIP)
make test-http-alloc # adds deterministic HTTP metadata-copy OOM tests
make test        # includes local fixtures and original network-dependent tests
```

Linux executables are `build/crawler-demo` and `build/demo_pipeline`.
For a beginner explanation of the new functions, pointers, memory ownership,
module integration and parsing limitations, see
[`documentation/phase4.md`](documentation/phase4.md).

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

Queue and visited tracking, recursive crawling, document downloads,
deduplication, provenance and reporting remain future work. See
`documentation/progress.md` and `documentation/architecture.md`.

## Phase 4: HTML hyperlink extraction

`include/html.h` and `src/html.c` provide `html_extract_links`,
`html_links_free` and `html_status_message`. The extractor accepts page HTML
and its final base URL, decodes practical HTML escapes, filters unsafe schemes
and uses the existing `url_resolve` to return owned absolute HTTP(S) URLs.
Duplicates stay in document order. It does not fetch links or change
`crawler_run`.

Run `make test-html` for the offline HTML suite and `make test-html-alloc`
for optional GNU-linker allocation-failure tests.

The targeted HTTP corrections require exact HTML/XHTML MIME media-type tokens
(parameters allowed) and restrict initial/redirect protocols to HTTP(S), without
disabling TLS verification. Local C fixtures verify redirect resolution,
non-HTML rejection and the response size boundary. Public API signatures remain
unchanged. See `documentation/phase4-test-results.md` for actual results and
limitations.

Metadata copy failures now return out-of-memory with cleared owned response
fields instead of a partial success. The HTML scanner excludes iframe and
other supported raw-text blocks and recognizes entity-encoded question marks
in unquoted queries. Queue/visited/BFS remain unimplemented.
