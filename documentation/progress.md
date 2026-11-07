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

- Part 7: integrated crawl testing and final system validation.

Parts 1–6 are implemented, including bounded BFS through `crawler_run`.
Queue/visited remain reusable independent modules and are now used by it.
IPv6, internationalized names and exhaustive RFC 3986 conformance
remain outside the existing URL module's scope. The optional Phase 4 allocation
test also exercises failures in the URL resolution allocations it invokes.

## Part 5 — Implemented reusable containers

- Linked FIFO queue with owned URL copies and associated nonnegative depths.
- Dequeue transfers ownership; repeatable item/container cleanup.
- Visited list owns canonical keys from existing `url_normalize`.
- Missing/duplicate/invalid/OOM/overflow outcomes are explicit.
- Deterministic C tests: 1200 elements, boundaries, ownership, failure rollback.
- Test-only allocator objects do not enter the production static library.
- Student guide: `phase5.md`; full order/history/evidence: `../prompt history/`.
- As delivered in Phase 5, original Phases 1–4 source/APIs/tests were preserved.
- Phase 6 was subsequently explicitly approved; its scope/evidence is separate.

## Part 6 — Implemented, bounded single-threaded BFS

- Canonical scheduling, FIFO/depth limits, cycles/duplicates and known aliases.
- Root failures abort; child failures continue and finish PARTIAL; OOM and
  invariants abort. LIMIT_REACHED never claims exhaustive success.
- All stats defined centrally; checked additions, depth INT_MAX guard.
- Exact seed hostname in same-domain mode; effective port pinned in both.
- Final redirect scope checked after fetch, NOT network isolation.
- Bounded HTML extraction extension avoids unlimited temporary URL arrays.
- C-only loopback/order/stats/error/budget tests and whole-engine fault tests.
- C CLI plus actual controlled demonstration. Prior modules/tests retained,
  except the required basic API valid-run assertion now uses a C stub.
- Complete order, decisions and handoff: `../prompt history/2026-11-08-phase6-*`.
- NO PHASE 7 STARTED. No document downloader/AI/UI/concurrency/deployment.
