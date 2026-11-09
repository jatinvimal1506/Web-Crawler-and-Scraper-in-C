# Phase 11 Design Decisions and Handoff

**Work-order reference:** [`2026-11-09-phases10-12-master-instruction.md`](2026-11-09-phases10-12-master-instruction.md)
**Evidence date:** 2026-11-10 (workspace date when final Phase 11 checks ran)
**Scope:** Phase 11 only. Phase 12 integration has not started.

This file records what was implemented and measured. The referenced master
instruction is the full authoritative specification; this handoff does not
replace it or claim to reproduce earlier conversation text.

## Decisions and rationale

1. **Filter metadata, not document bytes.** The matcher examines only
   `document_url` and `source_page_url`. It can therefore select rows before a
   download is attempted, and it never opens PDF, DOCX, or ZIP content.
2. **Borrow inputs; make only the result path owned.** `ReportRow` and keyword
   strings remain caller-owned and unmodified. This avoids copying up to 1,000
   rows. `ReportResult.output_path` is the one result allocation and has an
   explicit free function.
3. **Use bounded literal matching.** A filter has at most eight printable
   ASCII keywords, each 1–64 bytes. Matching is ASCII case-insensitive and
   searches either URL. A zero-keyword filter includes every row. A fixed KMP
   prefix array gives O(text length + keyword length) work per keyword without
   heap allocation; a naive rescanning matcher was rejected because long
   overlapping near-matches could repeat too much work.
4. **Keep provenance rows separate.** One row represents one source-page /
   document relationship. Rows are written in caller order, including multiple
   sources for one document; report writing does not deduplicate them.
5. **Make CSV safe and bounded.** The writer uses the exact nine requested
   columns, quotes every text cell, doubles embedded quotes, preserves commas
   and CR/LF inside quoted values, and uses CRLF record endings. It prefixes an
   apostrophe when leading ASCII controls/whitespace are followed by `=`, `+`,
   `-`, or `@`. Text fields are limited to 4,096 bytes and input to 1,000 rows.
6. **Never overwrite or write outside the selected basename.** The directory
   must already exist; filenames are safe ASCII `.csv` basenames up to 128
   bytes. Exclusive file creation prevents overwriting an existing report.
   Only a partial file created by this call is eligible for cleanup.
7. **Validate outcomes rather than printing contradictory rows.** New and
   duplicate stored outcomes require a lowercase 64-character digest, a
   nonempty path, and nonzero size. Failed/skipped outcomes must not claim a
   digest or stored path. The metadata matcher intentionally ignores these
   result fields so it works before outcomes exist; the writer validates them.
8. **Initialize results without reading indeterminate memory.** Source review
   found that the first initializer version inspected result fields before
   clearing them, which is unsafe for an ordinary uninitialized stack object.
   `report_result_init` now initializes fresh or already-freed storage directly.
   Callers must free an owned path before initializing that result again. A
   C test now initializes a genuinely uninitialized local result.
9. **Keep failure hooks test-only.** Allocation and file-operation wrappers are
   compiled into a separate test object. `nm` of the release archive found no
   `report_test_*` symbols.

## Public API and beginner ownership guide

The declarations are in `include/report.h`; implementation is in
`src/report.c`.

- `report_status_message(status)` and `report_row_status_name(status)` return
  static strings. They are not allocated and must not be freed.
- `report_result_init(result)` clears fresh or already-freed storage without
  allocating. It returns an invalid-argument status for NULL. Do not call it
  while `output_path` is owned; call `report_result_free` first.
- `report_result_free(result)` releases the result-owned path, clears the
  fields, and is safe for NULL or repeated cleanup.
- `report_row_matches(row, filter, matches)` sets the output integer to zero
  before validation. It uses only the two metadata URL pointers, allocates
  nothing, and sets the result to one when the empty filter includes the row or
  any keyword is found in either URL.
- `report_write_csv(directory, basename, rows, count, filter, result)` checks
  the full inputs, exclusively opens a new file, writes the header and selected
  rows, closes it, then transfers the allocated output-path string to
  `result`. The directory, basename, rows, row strings, keyword array and
  keyword strings are borrowed; callers keep them valid through the call.

`ReportResult` must be initialized with `report_result_init` or `{0}` before
`report_write_csv`. Do not shallow-copy a result after it owns a path. On
success, or when removal of a failed partial file also fails, the result owns
`output_path`. `report_result_free` is the matching release operation. Ordinary
errors leave no owned path. The report API does not own or modify the input
rows or their strings.

`rows_written` counts completed data records, not the header;
`rows_excluded` counts rows rejected by the keyword filter. A report with no
matching data still contains the header. On a write/close error, counts may
describe rows completed before the failure. If cleanup fails, status is
`REPORT_ERROR_CLEANUP` and the leftover path remains owned by the result so the
caller can locate it.

## Phase 11 acceptance table

| Requirement | Result | Evidence |
|---|---|---|
| Metadata-only matching on document/source URL; case-insensitive ASCII literal substring; any keyword; empty list includes all | PASS | `test-report`: document and source matches, case folding, any/all behavior, overlapping-prefix match/miss, and metadata-only pre-download row |
| Keyword bounds and invalid input | PASS | `test-report`: NULL/empty/overlong/control/non-ASCII keyword cases, count limit, invalid filter and NULL arguments |
| Per-source provenance rows retained in caller order | PASS | `test-report-csv`: exact bytes for two rows with the same document and different source pages |
| Exact nine-column header and required text/numeric ordering | PASS | `test-report-csv`: byte-for-byte output comparison |
| RFC-style quoted text, doubled quotes, commas, embedded CR/LF, CRLF record endings, empty strings | PASS | `test-report-csv`: exact serialized file comparison |
| Spreadsheet formula prefix after leading controls/whitespace | PASS | `test-report-csv`: formula-like values in multiple text fields, including a leading tab |
| 1,000-row boundary and over-limit rejection | PASS | `test-report-csv`: exactly 1,000 data rows and rejection of 1,001 |
| Safe basename, existing directory, no-overwrite behavior | PASS | `test-report-csv`: invalid names/device names, missing directory, long accepted name, and existing-file preservation |
| Result/path ownership, idempotent cleanup, input immutability | PASS | Unit/CSV tests; result is initialized from an uninitialized local, cleanup is repeated, and input strings are compared after writing |
| Allocation, write, close and cleanup failure handling; retry | PASS | `test-report-alloc`: 48 checks, one allocation failure position, 31 write positions, injected close/remove failures, cleanup path ownership and retry |
| C-only library/tests and no new runtime dependency | PASS | New implementation/tests are `.c`/`.h`; Makefile/Markdown only otherwise; static library uses existing project dependencies |
| Prior Phase 1–10 build/tests preserved | PASS | Clean `make test` exit 0; suite evidence below |
| C11 `-Wall -Wextra -Wpedantic -Werror` full build and suite | PASS | Strict clean build and `make test` exit 0; no warnings/errors/skips |
| AddressSanitizer + UndefinedBehaviorSanitizer for Phase 11 targets | PASS | All three Phase 11 targets pass with leak detection disabled; no ASan/UBSan report |
| Leak-enabled sanitizer / Valgrind | NOT VERIFIED | LeakSanitizer exits with its environment error under ptrace; Valgrind is not installed |
| Native Windows/MSYS2 runtime | NOT VERIFIED | This session ran on Linux; POSIX CSV/fault targets print SKIP on Windows. The Makefile retains its Windows branch |
| GitHub commit/push receipt | PENDING | Fill after the one safe Phase 11 push and remote SHA verification |
| Phase 12 orchestration | NOT STARTED | Deliberately outside this Phase 11 implementation gate |

## Actual commands and results

All final build/test commands ran from `.local/github-sync/deepdownload-c` on
Linux with GCC, GNU Make, and the existing libcurl package.

| Command | Exit | Result |
|---|---:|---|
| `make clean` | 0 | Clean completed |
| `make` | 0 | Static library, examples, and test executables built |
| `make test` | 0 | Full regression suite passed; no warning, error, or SKIP lines in the captured build/test logs |
| `CURL_CFLAGS="$(pkg-config --cflags libcurl)"; STRICT_CFLAGS="$CURL_CFLAGS -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude"; make clean; make CFLAGS="$STRICT_CFLAGS"; make test CFLAGS="$STRICT_CFLAGS"` | 0 for each | Entire C project built and tested with warnings treated as errors |
| `make test-report test-report-csv test-report-alloc CFLAGS="$STRICT_CFLAGS"` | 0 | 41 unit + 167 CSV + 48 allocation checks, no failures |
| ASan/UBSan `make test-report test-report-csv test-report-alloc` with `-fsanitize=address,undefined`, `ASAN_OPTIONS=detect_leaks=0:halt_on_error=1`, and `UBSAN_OPTIONS=halt_on_error=1` | 0 | 41 + 167 + 48 checks, no sanitizer diagnostic |
| Same targets with `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1` | 2 | LeakSanitizer: `LeakSanitizer does not work under ptrace`; this is not a passing leak check |
| `git diff --check` | 0 | No whitespace errors |
| `nm -g build/libcrawler.a \| grep report_test_...` | no matching symbols | Test-only hooks are absent from the release archive |
| `command -v valgrind` | unavailable | No Valgrind run |

The Phase 11 suite totals 256 reported checks: 41 matcher/unit checks, 167 CSV
integration checks, and 48 fault-injection checks. Allocation testing exercised
one report-path allocation failure and 31 separate `fwrite` positions, plus
close and removal failures. This is deterministic test accounting, not a
claim that an unavailable leak scanner proved the absence of every leak.

The complete regression suite also reported:

- Queue: 3,667 checks; visited set: 3,670 checks.
- Phase 5 allocation: 3,281 checks, zero tracked allocations after cleanup.
- Crawler engine: 372 checks; crawler allocation: 223,355 checks, zero tracked
  allocations after cleanup.
- Local HTTP suites: 81 and 128 checks; Phase 7 safety: 303 checks; Phase 7
  allocation: 271,334 checks, zero tracked allocations.
- Document unit: 41; document integration: 127; document allocation: 528,140
  checks, zero tracked allocations.
- Download unit: 140; integration: 277; allocation: 159 checks.
- SHA-256: 34; organize unit: 52; organize integration: 110; organize
  allocation: 174 checks.
- Existing URL, URL-validation and HTML test programs reported success; HTML
  reported 14 groups. The existing HTTP suite's normal HTTPS case returned
  status 200, 577 bytes, and `text/html; charset=utf-8` in this run. That
  particular test depends on public network availability; the Phase 11 tests
  themselves use no network.

## Changed files

- `include/report.h`, `src/report.c`: public report/filter/result types,
  validation, bounded matcher and CSV writer.
- `tests/test_report.c`: deterministic filter/validation unit tests.
- `tests/test_report_csv.c`: exact CSV and local filesystem boundary tests.
- `tests/test_report_alloc.c`: C-only injected allocation/write/close/remove
  failures. The instrumented source is a separate object, not a library member.
- `Makefile`: static-library source and three report test targets integrated
  into `make test`.
- `README.md`, `documentation/api.md`, `documentation/architecture.md`,
  `documentation/progress.md`, `documentation/testing.md`: current Phase 11
  status and public behavior.
- `documentation/phase11.md`: beginner explanation and usage/ownership guide.
- This handoff: design rationale, acceptance matrix and measured results.

No Phase 1–10 implementation file was changed. No Phase 12 code, document
download orchestration, UI, AI, browser automation, or other-language
implementation was added.

## Platform, mode and remaining gate

The session ran on Linux; native Windows/MSYS2 execution is NOT VERIFIED.
`test-report-csv` and `test-report-alloc` use POSIX temporary-directory APIs
and explicitly skip on Windows. The Makefile's existing Windows executable and
shell branches remain in place. Valgrind is unavailable. LeakSanitizer was
blocked by ptrace; ASan/UBSan passed with leak detection disabled.

No Replit mode setting was changed. The live Agent selector is not observable
from the C repository, so the active UI tier cannot be independently verified
by this code audit. The user previously reported that Free was selected.

**Phase 12 is not started.** Its gate is to retain this clean Phase 11 result,
commit and safely push only the C-project changes, verify `origin/main`, then
reread the complete master instruction before proceeding. Phase 12 must remain
limited to the already-authorized bounded orchestration work order.

## Git delivery receipt

At the time this handoff was first staged, local `main`, `origin/main`, and the
public GitHub `main` were all at
`fe5bfb44c78f9ed7183d2726efcad1148a12a997`. The Phase 11 change set was
uncommitted and limited to C source/header/tests, Makefile, and Markdown
documentation. The Phase 11 commit and push are intentionally recorded below
only after actual commit and remote verification:

- Phase 11 implementation commit: **PENDING**
- Push result: **PENDING**
- Verified GitHub `main` SHA: **PENDING**
