# DeepDownload-C

This university project remains a reusable C11 static library with separate CLI
demonstration programs. Parts 1–3 provide the public crawler API, HTTP(S) URL
parsing, and single-page HTTP downloading. Phase 4 now provides standalone HTML
anchor extraction. Phase 5 adds a reusable FIFO URL/depth queue and normalized
visited-URL set. Phase 6 integrates these modules into a bounded,
single-threaded BFS crawler; document downloading and Phase 7 are NOT started.

## Build on Linux / Replit

GCC, GNU Make and libcurl development files are required.

```sh
cd DeepDownload-C # from the surrounding Replit project; omit if already here
make
make test-unit   # deterministic API, URL and HTML tests
make test-phase5 # offline queue and visited tests
make test-phase5-alloc # deterministic queue/visited/URL allocation failures
make test-crawler-engine # deterministic loopback BFS/redirect/scope/stats tests
make test-crawler-alloc # whole-engine allocation-failure and invariant tests
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

`include/project.h` defines the crawler status, configuration, statistics, and
`crawler_run` API. Phase 6 now performs actual BFS. Invalid calls zero available
stats and return INVALID_ARGUMENT; root failures are errors; child failures
continue and finish PARTIAL. LIMIT_REACHED means deliberately incomplete.
NOT_IMPLEMENTED remains a legacy enum value, not the valid-run behavior.

## Part 2: URL handling

`include/url.h` and `src/url.c` provide HTTP(S) URL parsing, validation, normalization, relative URL resolution, and cleanup. `ParsedUrl` exposes scheme, host, optional explicit port, path, query, and fragment. Parsed component strings are heap allocated and released with `url_free`.

## Part 3: HTTP downloader

`include/http.h` and `src/http.c` provide `http_download` for one HTTP(S) page. It uses libcurl, follows redirects, applies a timeout and body-size limit, and reports HTTP status, response size, content type, final URL, and structured failures. Successful response fields are owned by the caller and released with `http_response_free`; call `http_cleanup` when finished.

The pipeline demo calls the Part 1 API, then demonstrates URL parsing and the standalone downloader. The latter two APIs are called separately because `crawler_run` does not yet orchestrate them.

## Remaining work

Phase 7 final validation/hardening and future document downloads,
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
in unquoted queries. Queue/visited and Phase 6 BFS are now implemented.

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
allocation and error behavior as of Phase 5. Current integration and failure
contracts are in the Phase 6 documents linked below.

The Markdown-only [prompt history](prompt%20history/2026-11-07-project-history.md)
contains accessible historical requests, the complete Phase 5 order and the
acceptance/evidence handoff. No document downloading, AI, GUI or parallelism
is implemented.

## Phase 6: actual bounded BFS, controlled demonstration

No config fields were added. FIFO depth starts at zero. Only unseen canonical
HTTP(S) keys are enqueued, then marked scheduled; any failed mark aborts with
cleanup. A second visited set tracks attempts and learned redirect aliases.
There are no automatic retries. See
[design/contracts](prompt%20history/2026-11-08-phase6-design-decisions.md) and
[acceptance/handoff](prompt%20history/2026-11-08-phase6-handoff.md).

Safety limits: **100 HTTP download attempts, 1000 emitted href candidates,
1000 scheduled/alias keys**, existing 5 MiB response cap, five redirects per
download and configured request timeout. Stats count attempts, not every
redirect hop. Depth also stops before incrementing INT_MAX. CRAWLER_OK means
the in-scope, depth-bounded queue drained with no failed pages—not that an
entire website or the Internet was exhaustively crawled.

`same_domain=1` pins the exact canonical seed hostname, not subdomains.
**Both modes pin the effective seed port**, so removing a nondefault port
cannot silently select another local service. Mode 0 permits other hosts on
that port. Redirect final URLs are checked AFTER download: off-scope bodies
and links are not parsed, but their network fetch may already have occurred.
This is NOT a network firewall/SSRF defense or automatic robots-policy engine.
Use only controlled/permissioned sites under their owner's policy; no bypass
of logins, CAPTCHA, blocks or restrictions is implemented.

All executable demo/fixture code is C:

```sh
make
make build/crawler-fixture
./build/crawler-fixture --demo
```

That command starts a loopback fixture and actually runs
`crawler-bfs URL 2 1`. The local graph has two siblings and one grandchild;
the CLI prints its real status and all five stats. For an interactive fixture,
run `./build/crawler-fixture`, then in another terminal:

```sh
./build/crawler-bfs http://127.0.0.1:PRINTED_PORT/site 2 1
```

Press Enter in the fixture terminal to stop. CLI defaults: depth 1, same-domain
1, timeout 3 seconds; CLI depth is conservatively restricted to 0..10.
The original `crawler-demo` and `demo_pipeline` are retained. The legacy
pipeline also performs a separate standalone download, so it is not an
exact-once BFS demonstration. The controlled launcher requires POSIX and
prints SKIP on native Windows; native Windows execution is NOT VERIFIED.

**NO PHASE 7 STARTED.** Further work needs explicit approval and UI-mode
confirmation; no mode/configuration change or deployment accompanies Phase 6.
