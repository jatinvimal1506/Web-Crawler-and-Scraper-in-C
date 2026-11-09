# Phase 8 acceptance and handoff

**Date:** 2026-11-09
**Phase status:** COMPLETE for the requested C11 link-discovery/provenance
scope. No Phase 9 or later work was started.

## Acceptance checklist

| Requirement | Result | Evidence |
|---|---|---|
| 1. Recognize PDF, DOCX and ZIP by URL path extension only | PASS | `document_type_from_url` tests case, query/fragment, valid unknown suffixes, relative and unsupported URLs. No content probing. |
| 2. Store unique normalized document URLs and source-page/depth pairs | PASS | C loopback tests cover first-discovery order, duplicate fragment variants, repeated links, multiple source pages and depths 0/1/2. |
| 3. Use existing BFS safely; never enqueue recognized documents | PASS | Additive `crawler_run_with_link_observer` filters recognized links. Exact local request order confirms document URLs are not fetched. Original `crawler_run` behavior remains. |
| 4. Respect existing scope, depth, page/candidate and redirect behavior | PASS | Integration cases cover depth boundaries, effective redirect page as relative-resolution/provenance base, same-domain and port rules, and partial crawl. |
| 5. Bound result memory and preserve ownership | PASS | 1000-pair ceiling, checked growth, independent owned canonical strings, nested cleanup and repeated cleanup tests. |
| 6. Handle OOM and overflow without false success | PASS | 719 deterministic application-module allocation failure positions; fatal OOM returns an empty result object; successful retry follows cleanup. |
| 7. Keep all executable project logic/tests in C11 | PASS | New library, CLI, local fixture cases and tests are `.c`/`.h`; no new non-C executable source was added. |
| 8. Preserve Phases 1–7 and add regression targets | PASS | Full existing queue, visited, URL, HTTP, HTML, BFS, redirect, safety and allocation suites pass unchanged in the fresh full run. |
| 9. Update student docs and preserve full prompt/history | PASS | README/API/architecture/progress/testing updated. The attached authoritative Phase 8 instruction was copied byte-for-byte. Prior history files were retained. |
| 10. Strict build, sanitizers and local demo | PASS with limitations | Clean Linux build and full tests pass; strict C11 `-Werror` run passes; ASan/UBSan targeted run passes with LeakSanitizer disabled. Leak-enabled run is blocked by the environment's ptrace restriction. |
| 11. Native Windows/MSYS2 run | NOT VERIFIED | Linux environment only. Existing Windows Makefile branch retained; documentation gives MSYS2 commands. |
| 12. GitHub commit and upload | See final agent handoff | Before committing, local `main`, `origin/main` and live `origin/main` all matched baseline `6dc654b1186863e6aaf155ecf24a96466649fdff`. This report is part of the published commit, so its own final SHA cannot be embedded in its contents without another receipt commit. The Replit final handoff reports the exact commit SHA, remote comparison and push outcome. |

## Implemented files and APIs

- `include/document.h`, `src/document.c`: `DocumentType`, `DocumentStatus`,
  owned `DocumentSource` / `DocumentRecord` / `DocumentResults`,
  `document_type_from_url`, result init/free/count/accessors, and
  `document_discover`.
- `include/project.h`, `src/project.c`: additive
  `CrawlerLinkObserver` and `crawler_run_with_link_observer`; the original
  `crawler_run` remains the no-observer entry point and retains Phase 6
  behavior.
- `examples/discover_documents.c`: C CLI for a start URL and conservative
  depth. It prints document type, URL, unique source pages/depth, crawler
  statistics and errors. It does not save files.
- `Makefile`: adds the static-library module, CLI, unit/integration/allocation
  targets and a separate test-only instrumented build.
- C test/fixture changes: `tests/test_document.c`,
  `tests/test_document_integration.c`, `tests/test_document_alloc.c`, and
  additional C routes in `tests/crawler_fixture.c` /
  `tests/crawler_fixture.h` / `tests/crawler_fixture_main.c`.
- Student documentation: `README.md`, `documentation/api.md`,
  `documentation/architecture.md`, `documentation/progress.md`, and
  `documentation/testing.md`.
- History: the complete authoritative instruction and this decisions/handoff
  pair in `prompt history/`.

## How to use the new API

1. Start with a zeroed `DocumentResults results = {0};`. `config` and its
   `start_url` are borrowed; they must remain valid until the call returns.
2. Call `document_discover(&config, &results, &stats)`.
3. Check its `DocumentStatus`. `DOCUMENT_OK` means the bounded BFS finished
   without failed pages; `DOCUMENT_PARTIAL` means child HTML pages failed;
   `DOCUMENT_LIMIT_REACHED` means a deliberate bound stopped the run. Fatal
   OOM/overflow returns an error and clears results.
4. Use `document_results_count`, `document_results_source_count`, and
   `document_results_at` to read borrowed pointers. Do not change the arrays or
   keep pointers after cleanup.
5. Always call `document_results_free(&results)`.

`DocumentResults.items` owns the records. Each record owns its canonical URL
and source array; each `DocumentSource.page_url` is another owned string.
`document_results_free` releases the nested strings and arrays and resets the
structure, so it is safe to call twice. `document_type_name` and
`document_status_message` return static strings and must not be freed.

Example: if `https://university.example/a` at depth 0 links twice to
`/public/rules.PDF#first` and `#second`, the normalized document URL appears
once and its source list contains one `(page, depth 0)` pair. If a depth-1
page links to that same PDF, the same record gets one more source entry at
depth 1. A different query such as `rules.PDF?download=2` remains a distinct
URL under the project's existing normalizer.

## Verification record

Fresh clean Linux verification used GCC, GNU Make, libcurl and the existing
C11 flags `-Wall -Wextra -Wpedantic`.

| Command | Exit | Observed result |
|---|---:|---|
| `make clean` | 0 | Build directory removed. |
| `make` | 0 | Static library and all C demos, including `discover-documents`, built without warnings. |
| `make test` | 0 | Every Phase 1–7 and Phase 8 target passed; the original network-dependent HTTP suite reported 7/7. |
| `STRICT_CFLAGS="${CFLAGS:-} -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude"; make CFLAGS="$STRICT_CFLAGS"` | 0 | All release C targets compiled with warnings treated as errors. |
| `STRICT_CFLAGS="${CFLAGS:-} -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude"; make CFLAGS="$STRICT_CFLAGS" test` | 0 | Full regression suite passed under `-Werror`. |
| `make test-document` | 0 | 41 document classifier/API checks. |
| `make test-document-integration` | 0 | 127 deterministic loopback BFS/provenance checks. |
| `make test-document-alloc` | 0 | 528,140 checks and 719 injected application C-module allocation failures; results rolled back and retry succeeded. |
| `SAN_CFLAGS="${CFLAGS:-} -std=c11 -Wall -Wextra -Wpedantic -Iinclude -fsanitize=address,undefined -fno-omit-frame-pointer"; SAN_LDFLAGS="${LDFLAGS:-} -fsanitize=address,undefined"; ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 make CFLAGS="$SAN_CFLAGS" LDFLAGS="$SAN_LDFLAGS" test-document test-document-integration test-document-alloc` | 0 | All three targeted suites passed without ASan/UBSan findings. |
| `SAN_CFLAGS="${CFLAGS:-} -std=c11 -Wall -Wextra -Wpedantic -Iinclude -fsanitize=address,undefined -fno-omit-frame-pointer"; SAN_LDFLAGS="${LDFLAGS:-} -fsanitize=address,undefined"; ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 make CFLAGS="$SAN_CFLAGS" LDFLAGS="$SAN_LDFLAGS" test-document test-document-integration test-document-alloc` | 2 | LeakSanitizer exited with “does not work under ptrace”; this is NOT a successful leak scan. |
| `make build/crawler-fixture && ./build/crawler-fixture --documents` | 0 | Real controlled local C demo: 3 HTML pages visited, 0 failed, 9 href entries, depth 1, 373 HTML bytes, 4 unique document URLs, 6 source/document pairs (PDF 2, DOCX 1, ZIP 1). |

The full regression suite also reported: queue 3667 checks; visited 3670;
Phase 5 allocation 3281; crawler engine 372; crawler allocation 223355;
Phase 7 safety 303; Phase 7 allocation 271334. HTML, HTTP local fixtures and
metadata OOM tests passed; the final HTTP suite reported 7/7. The full test
log contained no warnings, failures or skips in this Linux run. Tests that
need native Windows were not run.

The `${CFLAGS:-}` and `${LDFLAGS:-}` expansions above used the environment
flags present in this Replit Linux shell. No Windows execution or full
LeakSanitizer/Valgrind scan is claimed.

## Deliberately not included

- No HTTP fetch or binary save for PDF/DOCX/ZIP.
- No hashing, filesystem organization, keyword extraction, CSV report,
  web/GUI, AI, browser automation, concurrency or Phase 9–12 code.
- Unknown URL extensions are not guessed to be documents; they remain normal
  HTTP(S) crawl candidates and may fail the existing HTML-only fetch.
- Windows/MSYS2 runtime remains unverified. Leak-enabled sanitizer coverage is
  blocked by the current ptrace environment.

## Before any later phase

Do not begin Phase 9 until the user explicitly authorizes it. A future
download phase will need separate rules for file size, content-type checks,
safe filenames/output folders, partial-download cleanup, provenance linkage,
and tests for binary streams. Phase 8 itself has no caller-visible result
allocations beyond `DocumentResults`; its owner must always free that object.
