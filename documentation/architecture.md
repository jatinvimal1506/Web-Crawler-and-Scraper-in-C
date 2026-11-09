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
- `examples/demo.c`: Part 1 CLI that validates its arguments and demonstrates the current API status.
- `examples/demo_pipeline.c`: presentation demo that calls the crawler API, URL parser, and downloader in sequence.

## Current integration boundary

`crawler_run` now connects URL, queue, visited, HTTP and HTML modules. A FIFO
stores canonical URL/depth pairs; scheduled and attempted/known-final sets
prevent repeated scheduling and skip known queued redirect aliases.
Queue/visited APIs and implementations are unchanged. HTTP keeps its original
signature and adds a scoped entry point; both reuse one checked transport.
HTML gains a bounded-output entry point while keeping its original API.
The legacy pipeline still calls standalone URL/HTTP functions in addition to
its root-only crawler call; use crawler-bfs and the C loopback fixture for BFS.

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
No concurrency or future document features are included.
