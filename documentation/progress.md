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

## Current overall status

- Phase 12 connects the existing discovery, download, organization and report
  modules in a bounded C11 pipeline.

Phases 1–11 are implemented, including bounded BFS through `crawler_run`, the
single-document streamer, one-file local organization, and metadata filtering/
CSV reporting in Phase 11.
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
- At Phase 9 delivery, Phase 10 and later had not started. Phase 10 below was
  subsequently authorized and implemented.

## Part 10 — SHA-256 and one-file organization

- Plain C11 incremental SHA-256; no third-party hash dependency. Fixed 16 KiB
  buffer and 20 MiB inclusive file limit.
- One already-completed local PDF, DOCX or ZIP is placed under the matching
  `pdf/`, `docx/` or `zip/` folder with a lowercase digest filename.
- Identical bytes are compared across all three folders and stored once,
  including when callers provide different document types.
- Exclusive destination creation, post-copy digest/size verification, no input
  modification, and cleanup limited to a partial file created by this call.
- Tests cover known hash vectors, binary and size boundaries, local filesystem
  behavior, corrupt canonical paths, duplicates and injected allocation/I/O
  failures. Test-only wrappers remain outside the release static library.
- `documentation/phase10.md` explains API functions and ownership;
  `../prompt history/2026-11-09-phase10-design-and-handoff.md` records choices
  and actual acceptance evidence.
- POSIX filesystem integration/fault tests skip on native Windows; Windows
  runtime and junction/reparse-point protections are not verified here.
- Phase 11 adds standalone metadata keyword filtering and CSV reporting. Phase
  12 connects these modules; Phase 10 remains a separate one-file library.

## Part 11 — Metadata filtering and CSV reporting

- `ReportFilter` accepts 0–8 borrowed printable ASCII keywords, 1–64 bytes
  each; case-insensitive ASCII literal matching checks either normalized
  document URL or source-page URL. Empty filter includes everything.
- `report_row_matches` examines metadata only, allocates nothing, and is usable
  before any download result exists. KMP prefix matching keeps bounded search
  linear in text plus keyword length.
- `ReportRow` represents one `(document URL, source page)` relationship;
  writer preserves order and does not collapse multiple sources for one file.
- Safe CSV has the exact nine-column schema, quotes all text, escapes quotes,
  preserves embedded CR/LF, uses CRLF record separators and mitigates
  spreadsheet formula injection with an apostrophe prefix.
- CSV writer limits inputs to 1000 rows and text fields to 4096 bytes, requires
  an existing directory/safe `.csv` basename, creates exclusively, and removes
  only a partial file it created on failure.
- `ReportResult` owns its output path on success or cleanup failure and releases
  it with `report_result_free`.
- C-only unit, exact-byte CSV integration, boundary, no-overwrite and
  allocator/write/close/cleanup failure tests are included in `make test`.
- POSIX filesystem/fault fixtures skip on native Windows; Windows execution
  remains NOT VERIFIED. Metadata filtering never parses downloaded contents.
- Phase 12 below integrates the modules. The Phase 11 handoff remains a
  historical record of its separate standalone API.

## Part 12 — bounded end-to-end pipeline

- New `PipelineConfig`, `PipelineStats`, `PipelineResult` and `pipeline_run`
  API; the pre-existing module APIs and crawler configuration remain intact.
- Runs bounded document discovery, selects by up to eight metadata keywords,
  downloads at most ten selected unique document URLs, organizes/deduplicates
  bytes, then writes a provenance CSV.
- Maintains document/source relationship rows and discovery order. Partial
  crawl or document failures and resource limits have distinct non-success
  statuses; fatal allocation, filesystem, report and invariant errors abort.
- Existing safety bounds still apply: 100 attempted HTML pages, 1000 candidate
  hrefs/scheduled URL keys, 20 MiB per document, 1–3600 second per-request
  timeout. The pipeline adds a ten-document attempt ceiling.
- Owned result paths are documented and freed by `pipeline_result_free`;
  caller configuration strings remain borrowed. Staging names are exclusive,
  pre-existing reports are never overwritten, and cleanup failures retain paths
  for inspection.
- Added C-only API, C-loopback end-to-end/CLI, BFS-order/depth, filtering,
  redirect, failure/limit and allocation-failure tests. A new controlled
  CLI execution is captured by the integration test.
- Linux build/tests are measured in the Phase 12 handoff. Native Windows and
  full process-wide leak scanning remain unverified where tools are unavailable.
- Complete instruction is preserved in
  `../prompt history/2026-11-09-phases10-12-master-instruction.md`; Phase 12
  decisions and measured acceptance evidence are in
  `../prompt history/2026-11-09-phase12-design-and-handoff.md`.
- No Phase 13 or later work is included.
