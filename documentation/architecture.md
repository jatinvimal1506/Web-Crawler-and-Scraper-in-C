# Architecture

The project keeps the library and demonstration application separate. The Makefile compiles each module into `build/`, archives them into `libcrawler.a`, then links the CLI and tests.

## Implemented modules

- `project.c` / `project.h`: public crawler API, validation, statuses/statistics
  and Phase 6 single-threaded bounded BFS orchestration.
- `url.c` / `url.h`: Part 2 URL parsing, validation, normalization, relative-reference resolution, and memory cleanup.
- `http.c` / `http.h`: Part 3 single-page HTTP(S) downloader, using libcurl and the URL parser.
- `http_scoped.c`: Phase 7 bounded redirect loop with URL validation BEFORE
  each hop; reuses the checked single-transfer helper in `http.c`.
- `html.c` / `html.h`: standalone Phase 4 anchor extraction, practical entity
  decoding, raw-text exclusions and relative HTTP(S) link resolution.
- `queue.c` / `queue.h`: Phase 5 linked FIFO URL/depth ownership container.
- `visited.c` / `visited.h`: Phase 5 linked canonical URL set using `url_normalize`.
- `document.c` / `document.h`: Phase 8 extension classifier and owned
  document/source provenance results; observes BFS links without fetching files.
- `download.c` / `download.h`: Phase 9 one-document binary streamer with
  checked output ownership, response validation and pre-request redirects.
- `examples/demo.c`: Part 1 CLI that validates its arguments and demonstrates the current API status.
- `examples/demo_pipeline.c`: presentation demo that calls the crawler API, URL parser, and downloader in sequence.
- `examples/discover_documents.c`: Phase 8 CLI for a controlled URL/depth
  discovery run; it does not save binary files.
- `examples/download_document.c`: Phase 9 CLI for one caller-selected file;
  it never downloads all Phase 8 results.

## Current integration boundary

`crawler_run` now connects URL, queue, visited, HTTP and HTML modules. A FIFO
stores canonical URL/depth pairs; scheduled and attempted/known-final sets
prevent repeated scheduling and skip known queued redirect aliases.
Queue/visited APIs and implementations are unchanged. HTTP keeps its original
signature and adds a scoped entry point; both reuse one checked transport.
HTML gains a bounded-output entry point while keeping its original API.
The legacy pipeline still calls standalone URL/HTTP functions in addition to
its root-only crawler call; use crawler-bfs and the C loopback fixture for BFS.
`document_discover` uses the additive `crawler_run_with_link_observer` hook so
it sees each resolved, normalized, in-scope href before the crawler decides
whether to queue it as an HTML page. The observer records recognized document
URLs and returns `crawl_link = 0`; it never downloads those links. The ordinary
`crawler_run` API supplies no observer and keeps its Phase 6 behavior.

## Ownership and failure boundary

The downloader owns the body while receiving it. On success it transfers that
body and copied metadata to `HttpResponse`. If copying a present metadata value
fails, it frees the body and any earlier copies, resets response fields, and
returns `HTTP_ERR_OUT_OF_MEMORY` with a diagnostic. A missing Content-Type is
different: there is no header to copy, so the existing HTML-body heuristic is
used. Callers free responses before reuse.

Extraction borrows the response body and final URL only for the duration of the
call. Its output owns independent strings and is freed with `html_links_free`.
The HTTP/HTML public signatures are unchanged.
Document discovery retains independently owned normalized strings and dynamic
arrays. `DocumentResults` is caller-owned: initialize it empty, inspect records
through const accessors, and call `document_results_free` on every exit path.
Fatal allocation failures roll results back to empty; a partial child-crawl
failure keeps the discovered records and returns `DOCUMENT_PARTIAL`.

## Implemented Phase 6 data flow

Normalize/check key → enqueue URL/depth → mark scheduled in visited
→ dequeue/download/extract → resolve/filter children → repeat. FIFO is
BFS-friendly because new children go behind previously scheduled work.
The root has depth zero. Fixed budgets and explicit failure statuses are used;
there is no cross-container atomic transaction or resume/retry mechanism.
If enqueue fails, do not mark visited. If marking after enqueue fails, stop
with an explicit error rather than continue with untracked queued work.
The engine aborts on a post-enqueue mark failure and frees both containers.
Every page owns/frees its HTTP response, extracted links and final-key copy.
The entry point frees the dequeued item, both sets, remaining queue and seed.
See `../prompt history/2026-11-08-phase6-design-decisions.md` for the central
stats/status/alias/port policy, alternatives and honest redirect limitations.
That Phase 6 record is historical: Phase 7 now validates each redirect before
requesting it, supports timeouts 1..3600, and checks essential curl options.
Current contracts/ownership and limitations:
`../prompt history/2026-11-09-phase7-decisions-and-fixes.md`.
The Phase 8 observer and result ownership decisions are recorded in
`../prompt history/2026-11-09-phase8-design-decisions.md`.

## Phase 8 document discovery

`document_type_from_url` parses an absolute HTTP(S) URL and checks only the
path suffix `.pdf`, `.docx` or `.zip` (ASCII case-insensitive). It does not
infer file types from response headers or content. The query and fragment are
not part of the suffix check. `document_discover` records each normalized
document URL once and appends each unique normalized `(document, source page)`
pair with the source's BFS depth. First-discovery order is stable. A repeated
same-page link, or a fragment variant normalized to the same key, does not
create a second pair; meaningful query differences remain separate.

The returned `DocumentRecord` owns its URL and source array; each source owns
its page URL. The results object owns its record array. Accessor pointers are
borrowed and remain valid until results are freed or the results are changed
by a new discovery call. `document_results_free` releases every owned layer
and resets the object. The per-result occurrence limit is 1000; linear search
uses O(n) membership checks and can be O(n²) across a full list. This is
deliberately simple and adequate for the bounded teaching demo.

This phase discovers link metadata only. It does not fetch PDF/DOCX/ZIP files,
inspect file contents, save files, compute hashes, or add reporting/UI.
Unknown extensions remain ordinary HTTP(S) crawl candidates; no filename or
content is guessed to be a supported document type.

## Implemented Phase 9: one binary file

Phase 9 is separate from the BFS and Phase 8 result collection. The caller
selects one recognized document URL, seed URL, scope mode, timeout, existing
output directory and safe basename. The implementation reuses
`document_type_from_url`, URL parsing/normalization/resolution, checked libcurl
options and the crawler's pre-request scope predicate. It does not add fields
to `CrawlerConfig` or modify the document provenance structures.

The downloader's write callback receives a byte pointer and byte count from
libcurl. It checks multiplication and cumulative-size overflow before calling
`fwrite`. `FILE *` is a standard C handle for an open file; the response is
written as bytes, not as a NUL-terminated string. The callback stores no full
body in memory, and the 20 MiB limit applies even when the response is chunked
and omits `Content-Length`.

The response's HTTP code and single MIME media type are checked. The URL must
identify PDF, DOCX or ZIP; the MIME must match, except for the explicit and
tested `application/octet-stream` fallback. HTML, missing or duplicate MIME,
HTTP 206, other non-success statuses, empty bodies, timeout, oversize and
truncated responses are failures. The MIME label does not establish that the
file bytes are structurally valid.

Redirects are handled by a bounded manual loop. `Location` is resolved against
the last fetched URL and checked for HTTP(S), exact host when requested,
effective seed port in both modes, no HTTPS downgrade, and a maximum of five
redirects before the destination request begins. The hostname check is not a
DNS/IP firewall and does not implement robots or authorization policy.

The output basename is validated rather than derived from a URL or response
header. An exclusive-create flag prevents replacement of existing files.
During transfer the newly created file is visible, so this is not atomic
publication. Every failure path closes the file and removes only the file
created by this call. If removal fails, a distinct cleanup status is returned
and the result retains the path for diagnosis. Result-owned URL/path strings
are released by `download_result_free`; the result never owns an open file
handle.

Tests use only C fixtures and a local loopback server. The production static
library has no allocation or file-operation injection hooks. See
`testing.md` for targets/results and
`../prompt history/2026-11-09-phase9-design-decisions.md` for rejected
alternatives and policy trade-offs.

Phase 9 does not bulk-download discovery results, make type folders, hash or
deduplicate file contents, parse document formats, or generate reports. Those
are deferred and require separate approval.
