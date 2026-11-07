# DeepDownload-C

This university project remains a reusable C11 static library with separate CLI
demonstration programs. Parts 1–3 provide the public crawler API, HTTP(S) URL
parsing, and single-page HTTP downloading. Phase 4 now provides standalone HTML
anchor extraction. Phase 5 adds a reusable FIFO URL/depth queue and normalized
visited-URL set. Recursive crawling is not yet integrated.

## Build on Linux / Replit

GCC, GNU Make and libcurl development files are required.

```sh
cd DeepDownload-C # from the surrounding Replit project; omit if already here
make
make test-unit   # deterministic API, URL and HTML tests
make test-phase5 # offline queue and visited tests
make test-phase5-alloc # deterministic queue/visited/URL allocation failures
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

Recursive crawling, document downloads,
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
in unquoted queries. Queue/visited are implemented; BFS remains unimplemented.

## Phase 5: queue and visited URLs

`include/queue.h` provides a linked FIFO queue. Enqueue copies a nonempty URL
and a nonnegative `int` depth; dequeue transfers the owned string to an item
that the caller frees with `queue_item_free`. A queue containing `(A,0)`,
`(B,1)`, `(C,2)` returns those items in exactly that order. A head/tail list
makes linking/removal constant-time; copying the URL costs O(URL length).

`include/visited.h` provides a simple linked set of canonical absolute HTTP(S)
keys from the existing `url_normalize`. For example,
`HTTPS://Example.COM:443/docs#top` and `https://example.com/docs` identify one
key. Distinct paths/queries stay distinct. Membership/insertion is O(n*L),
favoring clarity over hash-table scale. Duplicate, missing, invalid and
out-of-memory outcomes are separate. Both containers own their allocations;
`queue_free` and `visited_free` reset them for repeated cleanup/reuse.

Start containers/items with `{0}`; do not shallow-copy populated containers.
Queue strings are not validated/normalized by the queue itself.
See [the student guide](documentation/phase5.md) for every function, pointer,
allocation, error and the future-only Phase 6 scheduling/failure boundary.
`crawler_run()` still returns `CRAWLER_NOT_IMPLEMENTED` for valid input.

The Markdown-only [prompt history](prompt%20history/2026-11-07-project-history.md)
contains accessible historical requests, the complete Phase 5 order and the
acceptance/evidence handoff. No document downloading, AI, GUI or parallelism
is implemented.
