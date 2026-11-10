# DeepDownload-C

This university project is a reusable C11 static library with separate C CLI
demos. Phases 1–12 include the crawler, URL/HTTP/HTML modules, reusable queue
and visited set, bounded BFS, PDF/DOCX/ZIP link discovery, one-document
streaming, local hashing/organization, metadata filtering, CSV reporting, and
the bounded end-to-end pipeline. No Phase 13 or later work is included.

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
make test-document # URL-extension classification and API tests
make test-document-integration # C-only local BFS/provenance tests
make test-document-alloc # injected result/crawler allocation failures
make test-download # downloader API, validation and scope-policy tests
make test-download-integration # C-only local binary/redirect/filesystem tests
make test-download-alloc # injected allocation and file-I/O failure tests
make test-sha256 # known-answer and file-size-boundary checks
make test-organize # folder, content deduplication and ownership checks
make test-organize-integration # local filesystem and conflict checks
make test-organize-alloc # allocation and file-I/O failure injection
make test-report # metadata keyword-matching tests
make test-report-csv # exact CSV encoding and filesystem tests
make test-report-alloc # injected allocation, write, close and cleanup failures
make test-pipeline # pipeline API and validation tests
make test-pipeline-integration # C-only local end-to-end and CLI tests
make test-pipeline-alloc # deterministic pipeline allocation-failure tests
make test        # includes local fixtures and original network-dependent tests
```

Linux executables include `build/crawler-demo`, `build/demo_pipeline`,
`build/crawler-bfs`, `build/discover-documents`, `build/download-document` and
`build/deepdownload`.
For beginner explanations, see [`documentation/phase4.md`](documentation/phase4.md),
[`documentation/phase10.md`](documentation/phase10.md),
[`documentation/phase12.md`](documentation/phase12.md), and
[`documentation/api.md`](documentation/api.md).

## Build on Windows

Use MSYS2 UCRT64 GCC and GNU Make with libcurl available.

```powershell
make
make test
```

The Makefile builds `build/libcrawler.a`, the existing crawler demos,
`build/discover-documents.exe`, `build/download-document.exe`,
`build/deepdownload.exe` and test programs.
Native Windows execution is not verified here; POSIX loopback integration and
fault-injection targets print SKIP on native Windows.

The older presentation demo still calls individual APIs in sequence; the actual
Phase 12 integration CLI is `deepdownload`:

```powershell
.\build\deepdownload.exe START_URL OUTPUT_ROOT REPORT.csv [DEPTH] [SAME_DOMAIN] [--keyword WORD]...
```

`OUTPUT_ROOT` must already exist. Depth defaults to 1 (the CLI accepts 0–3),
same-domain defaults to 1, and the request timeout is a fixed 3 seconds. Use
only a site you own or have permission to access. Exit code 0 means the bounded
run completed; 1 means partial results or a safety limit; 2 means invalid input
or a fatal error.

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

The presentation pipeline demo calls its standalone URL/downloader functions
separately; `crawler_run` itself already performs bounded BFS.

## Current status

The Phase 12 bounded pipeline is implemented and integrated. It does not claim
to exhaustively crawl a site. See [`documentation/progress.md`](documentation/progress.md)
and [`documentation/architecture.md`](documentation/architecture.md) for the
current boundaries and test coverage.

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
and dated Phase 5–12 instructions/hand-offs record the project decisions and
acceptance evidence. No AI, GUI, browser automation or parallel crawling is
implemented.

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
that port. Phase 7 checks every redirect URL BEFORE its request through the
additive scoped downloader (maximum five hops). Blocked host/port targets are
not fetched. HTTPS->HTTP downgrade is also refused. Standalone `http_download`
retains automatic redirects for callers that explicitly use that API.
This is NOT an IP/DNS firewall, SSRF defense or automatic robots-policy engine.
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

## Phase 7 hardening and final validation

Supported timeout for both HTTP and crawler APIs is **1..3600 seconds**.
Zero/negative/LONG_MAX/3601 are rejected before networking; essential curl
option failures abort setup. The scoped API uses this timeout per individual
transfer; up to six transfers (five redirects) can occur per logical page.
There is no whole-crawl wall-clock timer. CLI timeout remains 3 seconds.

```sh
make test-phase7-safety
make test-phase7-alloc
```

Both join `make test`. The public four-field config/five-field stats and
existing signatures remain source-compatible. HTTP_ERR_SCOPE is appended;
root scope rejection maps to PARSE_ERROR, child rejection to eventual PARTIAL.
Existing stats keep their Phase 6 definitions; blocked targets no longer
contribute bytes because they are never fetched.

See [decisions/fixes](prompt%20history/2026-11-09-phase7-decisions-and-fixes.md)
and [current validation/handoff](prompt%20history/2026-11-09-phase7-acceptance-and-handoff.md).
Earlier Phase 6 history records describe the old post-fetch policy and are
intentionally preserved, not silently rewritten.

Phase 10 provides standalone hashing and organization for one completed local
file. Phase 11 adds reusable metadata filtering and CSV reporting. Phase 12
connects these modules through the bounded pipeline described below.
No mode/configuration change, paid upgrade or deployment was made.

## Phase 8: public-document link discovery and provenance

`include/document.h` and `src/document.c` add `document_discover` and
`document_type_from_url`. The classifier looks only at the URL path extension:
`.pdf`, `.docx` and `.zip` are recognized without fetching or guessing file
content. The discovery observer runs while the existing C11 BFS processes
in-scope HTML links. It stores each normalized document URL once and records
each normalized source-page URL plus its crawl depth once. Repeated links and
fragment-only URL variants do not duplicate a record/source pair; meaningful
query differences remain distinct.

`document_discover` filters recognized document URLs out of the page queue and
does not download them. Phase 9 adds a separate single-URL operation; it does
not automatically download every discovered record.
Ordinary HTTP(S) links with other extensions remain normal crawler candidates.
Results own their strings and arrays; callers initialize with `{0}` or
`document_results_init`, read with the accessors, and release with
`document_results_free`. Linear searches keep this beginner-scale collection
simple (membership can take O(n)); results are limited to 1000 occurrences.
See `documentation/api.md`, `documentation/architecture.md`,
`documentation/testing.md`, and the Phase 8 files in `prompt history/`.

Try the controlled local demonstration with `make build/crawler-fixture`,
then `./build/crawler-fixture --documents`. It starts the C loopback fixture
and runs `build/discover-documents`; no external site is contacted.

## Phase 9: one-document binary streaming

`include/download.h` and `src/download.c` provide
`download_document(config, result)` for one recognized PDF, DOCX or ZIP URL.
The extension is classified by the existing Phase 8 URL helper. The libcurl
write callback streams exact bytes to a caller-selected file; it never builds a
whole binary body in memory or treats it as a C string. The per-file cap is
20 MiB: exactly 20 MiB is accepted, and any additional byte fails. PDF, DOCX
and ZIP MIME types are checked; one `application/octet-stream` header is an
explicit fallback, while missing, duplicate, HTML and unrelated types fail.
MIME labels do not prove document structure.

The caller supplies an existing output directory and a safe basename with the
matching extension. Names are restricted to ASCII letters, digits, dot, dash
and underscore, up to 128 bytes; paths, traversal, Windows device names and
trailing dots/spaces are rejected. Exclusive file creation prevents overwrite.
The file is visible during streaming (not atomically published); failed
transfers remove only the file created by that call. A failed cleanup has a
distinct status and preserves the path for diagnosis.

Redirects are followed manually, at most five. Each HTTP(S) target is parsed
and checked **before** its request. Both policy modes keep the seed's effective
port; same-domain mode also requires the exact seed hostname. HTTPS downgrade
is blocked and TLS verification remains enabled. This is not an IP/DNS
firewall. The timeout is 1–3600 seconds per hop.

```sh
make build/download-document
./build/download-document DOCUMENT_URL SEED_URL OUTPUT_DIRECTORY safe-name.pdf 1
./build/download-document DOCUMENT_URL SEED_URL OUTPUT_DIRECTORY safe-name.pdf 1 3
make test-download
make test-download-integration
make test-download-alloc
```

The CLI handles exactly one URL and prints its status, type, HTTP status, byte
count, final URL and path. It does not organize files or bulk-download Phase 8
results. Phase 10 provides a separate local-file hashing/organization library;
it does not interpret PDF/DOCX/ZIP internals. See
`documentation/api.md`, `documentation/architecture.md`,
`documentation/testing.md`, and the Phase 9 files in `prompt history/` for
ownership, limitations and measured test results.

## Phase 10: SHA-256 and type-based organization

`include/sha256.h` and `src/sha256.c` provide dependency-free, incremental
SHA-256 for a file up to 20 MiB. `include/organize.h` and `src/organize.c`
organize one already-downloaded PDF, DOCX or ZIP into an existing output root's
`pdf/`, `docx/` or `zip/` folder. Files are named by their lowercase SHA-256.
Identical bytes are compared and deduplicated across all three folders; the
input is never modified, and an existing destination is never overwritten.
New copies are hashed and size-checked after writing. The code uses a fixed
16 KiB buffer rather than loading a document into memory.

```sh
make test-sha256
make test-organize
make test-organize-integration
make test-organize-alloc
```

These are C tests. The integration and allocator fault-injection suites use
POSIX filesystem facilities and explicitly skip on native Windows. See
[`documentation/phase10.md`](documentation/phase10.md) for the APIs, memory
ownership, failure behavior and scope. The full Phase 10 instruction and
evidence-based handoff are in `prompt history/`.

Phase 10 remains a reusable one-file operation. Phase 11 remains a reusable
metadata/reporting module. Phase 12 connects them through a bounded workflow.

## Phase 11: metadata keyword filtering and CSV

`include/report.h` and `src/report.c` provide an allocation-free metadata
matcher and a bounded CSV writer. Up to eight borrowed ASCII keywords (1–64
characters each) are matched case-insensitively as literal substrings against
either the document URL or its source-page URL. An empty keyword list includes
all rows. This searches metadata only; it never reads or extracts PDF, DOCX or
ZIP contents.

The CSV contains one row for each source-document relationship in the supplied
order, using the documented nine-column schema. Text is quoted, embedded quotes
are doubled, CR/LF inside fields are preserved, and rows end in CRLF. Fields
that could begin a spreadsheet formula are prefixed with an apostrophe before
quoting. The writer accepts at most 1,000 rows and 4,096 bytes per text field,
requires an existing directory and a safe `.csv` basename, and exclusively
creates the file rather than overwriting one. If no rows match, it writes the
header only.

`ReportRow` pointers are borrowed for each call. A `ReportResult` owns
`output_path` only after success or when cleanup of a failed new file itself
fails; release it with `report_result_free`. Failed and skipped rows cannot
claim a stored hash or path. `report_row_matches` inspects only the two
metadata URLs, so the Phase 12 pipeline filters before download outcomes exist.

```sh
make test-report
make test-report-csv
make test-report-alloc
```

The metadata unit suite is portable C. CSV filesystem and fault-injection tests
use POSIX temporary directories and explicitly skip on native Windows. Phase 12
composes these modules but never inspects document contents. See
[`documentation/phase11.md`](documentation/phase11.md),
[`documentation/phase12.md`](documentation/phase12.md), and the prompt-history
handoffs for test counts, ownership and limitations.

## Phase 12: bounded end-to-end pipeline

`include/pipeline.h` and `src/pipeline.c` connect document discovery, metadata
keyword filtering, the existing one-document downloader, SHA-256
organization/deduplication, and the safe provenance CSV writer. The CLI is
`build/deepdownload`:

```sh
mkdir -p output
./build/deepdownload START_URL output report.csv [DEPTH 0..3] [SAME_DOMAIN 0|1] [--keyword WORD]...
make test-pipeline
make test-pipeline-integration
make test-pipeline-alloc
```

The output directory must already exist. Discovery order is preserved;
keyword matching uses document/source URL metadata; at most ten selected unique
documents are downloaded; and the CSV retains a row for every selected
source/document relationship. Identical bytes are reported as duplicates, not
stored twice. Failures and resource ceilings produce explicit partial/limited
statuses, never a claim of exhaustive success. `PipelineConfig` strings and
keywords are borrowed for the call. `PipelineResult` owns its report path and
any paths left behind when cleanup itself fails; release them with
`pipeline_result_free`.

The deterministic integration target starts a C-only loopback server and runs
the actual CLI. It checks BFS order/depth, filters, redirected relative-link
provenance, duplicate content, failed pages/downloads, limits, report safety,
timeouts and cleanup. Native Windows execution is not verified; POSIX fixture
tests explicitly skip there. See [`documentation/phase12.md`](documentation/phase12.md)
for the beginner-oriented API, memory, resource-limit and test explanations.
