# DeepDownload-C instruction and decision history

Recorded 2026-11-07. This is a **retrospective summary**, not a verbatim
historical prompt. Exact accessible user requests are in
[historical requests](2026-11-07-historical-requests.md). Original request
timestamps and requests before the available conversation are unavailable;
the date above is the recording date, not an invented date for each event.

## Chronological milestones

1. **Original Phases 1–3 — predates this recorded conversation.**
   The university project already supplied a C11 static library, CLI demos,
   configuration/status API, URL parsing/normalization/resolution and a libcurl
   HTML downloader. The crawler entry point intentionally returned not
   implemented for valid configurations. Source provenance:
   [586eb0a](https://github.com/jatinvimal1506/Web-Crawler-and-Scraper-in-C/commit/586eb0a8dae417d81689499bf31c06945c656203).
   The original creation prompts are NOT AVAILABLE.
2. **Import and Phase 4 order — available conversation, recorded 2026-11-07.**
   Import the actual repository, not a substitute React/Python implementation.
   DeepDownload-C is the final name. Only standalone HTML anchor extraction
   was authorized. Preserve Windows/MSYS2 support and existing APIs.
3. **Standing constraints reiterated.**
   All application and executable test logic must be C11. No AI features,
   browser automation, GUI, paid upgrades, deployment or mode switches without
   approval. Do not overwrite unrelated Replit scaffolding. The product's
   eventual PDF/DOCX/ZIP discovery/provenance/download goals were explicitly
   outside these phases.
4. **Phase 4 audits and initial corrections.**
   Read actual code and run tests rather than trusting plans. Tests/code were
   added for unquoted query `=`, malformed attributes, exact HTML MIME tokens,
   HTTP(S)-only redirects, final redirect URL as a resolution base, binary
   rejection, and exact/over-limit 5 MiB responses.
   [f6ec516](https://github.com/jatinvimal1506/Web-Crawler-and-Scraper-in-C/commit/f6ec516596675e787ae0da8c6cfab36daf6262d8).
5. **A later review found defects despite green tests.**
   HTTP metadata malloc failures could look successful; iframe text could
   produce fake anchors; encoded question marks broke unquoted query scanning.
   Targeted fixes, C fault injection, raw-text tests, encoded-query tests and
   documentation corrections followed.
   [decbbe7](https://github.com/jatinvimal1506/Web-Crawler-and-Scraper-in-C/commit/decbbe73fae6fb04e5cb4038aa7e5011c26151a0).
6. **GitHub sync initially blocked, then succeeded.**
   Earlier checks/pushes returned HTTP 403 for the connected identity.
   A later explicitly authorized permission dry-run succeeded; one normal
   non-force push succeeded. Live GitHub main then matched decbbe7 and contained
   both Phase 4 commits. This is not a claim that all earlier attempts worked.
7. **Repeated final read-only Phase 4 gates.**
   Actual local/live remote SHAs matched. Fresh Linux build/tests passed:
   HTML 14 groups, local HTTP 81 checks, HTTP instrumentation 128 total checks,
   HTML allocation 143 failure points, original HTTP 7/7 groups.
   Native Windows and complete leak scanning were not verified.
8. **Phase 5 authoritative order — recorded 2026-11-07.**
   The user now explicitly authorizes queue + visited ONLY, deterministic C
   tests, student documentation, this prompt-history folder and a scoped Git
   commit/push. The complete instruction is preserved in
   [the authoritative order](2026-11-07-phase5-authoritative-instruction.md).
9. **Phase 5 implementation decisions.**
   A head/tail linked queue copies input URLs and transfers strings on dequeue.
   A linked visited list owns canonical keys from the unmodified URL module.
   Linear membership favors simple teaching code over hash-table scale.
   Error returns are distinct from empty/missing/duplicate states; insertion
   publishes nodes only after allocation succeeds. No combined BFS engine or
   network operations were added. Final evidence belongs to the
   [handoff](2026-11-07-phase5-handoff.md).

## Mode and repository distinctions

The C source tree is `DeepDownload-C/` in the surrounding Replit workspace.
The separate GitHub checkout is `.local/github-sync/deepdownload-c`; it has the
C project at its repository root. The outer workspace's checkpoint repository
is NOT the GitHub C repository and must not be pushed as this product.

The existing outer `.replit` has `expertMode = true`. This configuration flag
does not establish the active account UI model/billing mode. No mode change was
needed or authorized for these C tasks. Do not claim active normal mode was
verified merely because no settings were changed.

## Deferred, not accidentally omitted

Phase 6: orchestration/BFS and runtime policy, only after explicit approval.
Later: document discovery/downloads, document provenance/deduplication, final
integrated crawl testing. No network policy assumptions are made in Phase 5.
