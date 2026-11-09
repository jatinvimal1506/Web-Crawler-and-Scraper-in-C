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

## Deferred beyond Phase 9

- File organization, content hashing, keyword/CSV reporting and final bulk
  orchestration.

Phases 1–9 are implemented, including bounded BFS through `crawler_run` and
the single-document streamer.
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
- Phase 6 originally checked final scope after fetch; Phase 7 replaces this
  with validated pre-request redirects (still NOT an IP/DNS firewall).
- Bounded HTML extraction extension avoids unlimited temporary URL arrays.
- C-only loopback/order/stats/error/budget tests and whole-engine fault tests.
- C CLI plus actual controlled demonstration. Prior modules/tests retained,
  except the required basic API valid-run assertion now uses a C stub.
- Complete order, decisions and handoff: `../prompt history/2026-11-08-phase6-*`.
- At Phase 6 delivery, Phase 7 had not started and no document/AI/UI features
  were included.

Phase 7 was subsequently explicitly approved and implemented:

## Part 7 — Final validation and safety hardening

- HTTP/crawler timeout 1..3600 enforced before networking, all curl options checked.
- Additive scoped downloader validates redirects before each target request.
- Exact host/seed-port policy, five hops, HTTPS downgrade refusal.
- Standalone downloader and public config/stat layouts retained.
- Invalid timeout/option zero-request proofs; redirects/status/partial bodies/
  global candidate and alias-key budgets; real scoped/BFS allocation sweeps.
- Stale testing text corrected; native Windows remains NOT VERIFIED,
  POSIX fixtures explicitly SKIP; actual sanitizer/leak observations recorded.
- Full order/decisions/evidence: `../prompt history/2026-11-09-phase7-*`.
- Phase 8 was subsequently explicitly approved and implemented:

## Part 8 — Public-document link discovery and provenance

- URL-path-only, case-insensitive PDF/DOCX/ZIP extension classification;
  query strings and fragments do not affect type.
- Additive BFS link observer stores normalized document URLs and unique
  normalized source-page/depth pairs, preserving first-discovery order.
- Recognized documents are filtered out before page scheduling and are never
  downloaded. No content sniffing or network probes for file type.
- Results own all strings/arrays; public init/access/free functions document
  pointer lifetime. Explicit 1000 occurrence ceiling; linear lookup suits
  the current beginner-scale limits.
- C-only classifier, local BFS/provenance and allocation-failure tests.
- CLI demo `discover-documents` runs against the controlled C loopback fixture.
- Windows native execution remains NOT VERIFIED; POSIX fixture tests SKIP
  there. See `../prompt history/2026-11-09-phase8-*` for the full instruction,
  decisions and evidence.
- At Phase 8 delivery there was no binary downloader; Phase 9 below was approved
  separately. No AI, GUI or deployment is included.

## Part 9 — Safe one-document binary download

- `download_document` accepts a single Phase 8-recognized PDF, DOCX or ZIP URL,
  scope seed, timeout, existing output directory and safe basename.
- The C libcurl callback streams bytes directly to a file, preserving NUL and
  high-valued bytes without buffering the whole document; 20 MiB is inclusive.
- Content-Type, HTTP status, empty/incomplete body, timeout and byte limit are
  checked. The single `application/octet-stream` fallback is explicit.
- Redirects are manually resolved and scope-checked before each request; exact
  host (when enabled), seed effective port, HTTPS downgrade policy and five-hop
  cap match the prior crawler contract.
- Exclusive creation prevents overwrites. Failure paths close and remove only
  the new partial file; cleanup failure has its own status.
- C-only local tests cover binary bytes, boundaries, MIME/status, timeout,
  redirects, scope, file collision, allocator positions and injected write,
  close and cleanup errors.
- `download-document` is a separate one-file C CLI; Phase 8 discovery remains
  independent and does not bulk-download results.
- Full instruction, design decisions and evidence: `../prompt history/`
  `2026-11-09-phase9-*`.
- **NO PHASE 10 OR LATER STARTED.** Type-based folders, content hashing,
  keyword/CSV reporting and final orchestration remain deferred.
