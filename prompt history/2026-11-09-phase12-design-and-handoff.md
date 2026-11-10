# Phase 12 Design Decisions and Handoff

**Work-order:** [`2026-11-09-phases10-12-master-instruction.md`](2026-11-09-phases10-12-master-instruction.md)
**Verification recorded:** 2026-11-10
**Scope:** Phase 12 end-to-end pipeline only. No Phase 13 or later work was started.

This handoff records implementation choices and observed evidence. The linked
master instruction is the full source of truth and acceptance checklist. Its
attached text was copied into the master file with line endings normalized
from CRLF to LF; a comparison after removing carriage returns matched the
attachment. Earlier history and the Phase 10/11 handoffs remain in this folder.

## What the program now does

The C11 pipeline connects the earlier modules into one bounded command-line
workflow. A user supplies a website address and an existing output directory.
The program crawls permitted HTML pages, finds PDF/DOCX/ZIP links and records
which source page exposed each link. Optional keywords select metadata rows
before any document download. The program downloads at most ten distinct
selected document URLs, organizes files by type, identifies identical file
contents by SHA-256, and writes a CSV that retains each source/document
relationship.

This is more than fetching one page, but it is not full-text search, a general
browser, an unrestricted crawler, or a guarantee that every website works.
URL deduplication avoids fetching the same normalized URL repeatedly.
Content deduplication is separate: the organizer compares SHA-256 values and
stores identical bytes only once even when their URLs differ.

## Design decisions

1. **Compose the existing C modules instead of replacing them.** `pipeline_run`
   calls the existing crawler/document-discovery, metadata matcher, downloader,
   organizer and CSV writer. Existing crawler, URL, HTTP, HTML, queue, visited,
   document, download, hashing and organization APIs remain in place.
2. **Validate before network activity.** The seed, depth, scope flag, timeout,
   output directory, CSV basename and keyword filter are checked first. The
   report must not already exist. New `report_validate_filter` and
   `report_validate_csv_basename` helpers make these checks reusable and
   allocation-free.
3. **Filter metadata before downloading.** Keywords match the document URL or
   the source-page URL, case-insensitively, using the Phase 11 matcher. They do
   not inspect document bytes. Zero keywords selects all metadata rows.
4. **Use one bounded attempt per unique URL.** A maximum of ten selected unique
   document URLs is attempted in first-discovery order. Existing crawl,
   timeout, redirect, body-size, link-count and report-row limits continue to
   apply. Reaching a configured bound is reported as
   `PIPELINE_LIMIT_REACHED`, not as an exhaustive-success claim.
5. **Stage before organizing.** Downloaded bytes go to a unique,
   exclusive-created staging file. The organizer receives a completed file.
   Only files created by this run may be removed on failure. If cleanup itself
   fails, the result returns an owned path for the leftover file.
6. **Keep provenance rows separate from file storage.** Each selected
   source-page/document relationship is a CSV row. Several rows can point to
   one stored file. A duplicate-content URL still has its own provenance row.
7. **Keep allocation-fault replacements out of production.** Pipeline tests
   compile a separate test object with allocator, `remove`, and report-writer
   replacements. These symbols are absent from `build/libcrawler.a`.

## Public functions, pointers and ownership

The public pipeline API is in `include/pipeline.h`.

- `pipeline_result_init(PipelineResult *result)` initializes fresh or already
  freed result storage and allocates nothing. Use `{0}` for a new result; do not
  initialize a result that still owns paths.
- `pipeline_run(const PipelineConfig *config, PipelineResult *result)` performs
  the work. `config` and all of its strings/keyword data are borrowed: the
  function reads them but does not edit, retain or free them. `result` is
  caller-provided writable storage.
- `pipeline_status_message(PipelineStatus status)` returns a static string;
  the caller must not free it.
- `pipeline_result_free(PipelineResult *result)` frees the result's owned path
  strings and resets it. It is safe for `NULL` and repeated calls. It does not
  delete files named by those strings.

`PipelineResult.report_path` is owned when a report exists. Each entry in
`orphaned_paths` is an owned path string; the file still exists and needs
attention if cleanup failed. The caller must use `pipeline_result_free`, must
not shallow-copy an owning result, and receives no other heap allocation from
`pipeline_run`.

`PipelineConfig` uses `const char *` for borrowed input strings and
`const char *const *` for the borrowed keyword list. `const` means this
function does not modify the pointed-to text. `PipelineStats` counters are
values, not pointers; counters are checked before increment/addition.

Phase 12 added `report_validate_filter(const ReportFilter *)` and
`report_validate_csv_basename(const char *)`. They check keyword and safe
filename inputs without allocating or touching the filesystem. The existing
`report_write_csv` API continues to create a new file exclusively and returns
its owned path through `ReportResult`; `report_result_free` releases that path.

## Results and limits

The CLI accepts:

```text
./build/deepdownload START_URL OUTPUT_DIRECTORY REPORT.csv [DEPTH 0..3] [SAME_DOMAIN 0|1] [--keyword WORD]...
```

Its timeout is three seconds. The library supports 1–3600 seconds per request.
Existing crawler limits are 100 attempted HTML pages, 1000 candidate hrefs,
and 1000 scheduled/alias URL keys. A document download is capped at 20 MiB.
Phase 12 adds ten unique document-download attempts; the report still has its
1000-row cap. The output directory must already exist.

`PIPELINE_OK` means the bounded run completed without a recoverable failure or
configured limit; it does not mean the website was exhaustively crawled.
`PIPELINE_PARTIAL` preserves useful results when pages or selected documents
fail. `PIPELINE_LIMIT_REACHED` reports that a crawl/discovery/download bound
was reached. Fatal invalid-input, allocation, filesystem, report, crawl and
internal errors are distinct statuses and preserve coherent collected
counters where possible.

The counters distinguish:

- HTML pages successfully visited from pages that failed.
- Extracted href occurrences from unique document URLs.
- Unique document URLs from source/document relationships.
- Selected URLs from selected relationship rows.
- New stored files from content duplicates.
- Successful downloaded bytes from newly stored bytes.
- Written data rows from the CSV header.

## Acceptance mapping to the master instruction

Status meanings: **PASS** means observed implementation and evidence meet the
requirement; **NOT VERIFIED** means the environment prevented a truthful test.
There were no known acceptance failures.

| Requirement IDs | Status | Evidence / note |
|---|---|---|
| A1 | PASS | Work followed the Phase 12 order; earlier APIs and tests were preserved. |
| A2 | PASS with limitation | No mode settings, paid features or deployments were changed. The user stated Free was selected; the active selector cannot be independently inspected here. |
| A3 | PASS | Worked in the C-only `main` checkout; public and configured remote main matched before the Phase 12 commit. |
| A4 | PASS | Existing project, crawler and Phase 1–11 module APIs remain available. Report preflight validation was additive. |
| A5 | PASS | Limits, validation, no-overwrite handling, HTTP(S)-only behavior and cleanup rules are documented and regression-tested. |
| B1–B9 | PASS | Phase 10 hashing, content-addressed organization, API and tests remain in the library; full Phase 10 suites passed. |
| C1–C9 | PASS | Phase 11 keyword matching and safe CSV behavior remain; the new preflight validators are additive; full Phase 11 suites passed. |
| D1–D2 | PASS | `pipeline_run` and the required public header, C CLI, fixture and tests are present. |
| D3 | PASS | `PipelineConfig`, `PipelineStats`, `PipelineResult` and four public functions are documented above and in `documentation/api.md`. |
| D4–D5 | PASS | Bounded page, link, key, byte, URL-attempt and CSV-row limits are retained and documented. |
| D6 | PASS | The pipeline validates, discovers, filters, limits, downloads, organizes, records provenance, writes CSV and frees internal allocations. |
| D7 | PASS | Status precedence, partial results, counters, byte meanings and overflow checks are documented and tested. |
| D8 | PASS | A C-only CLI accepts the requested arguments and reports status/counters/orphan paths. |
| D9 | PASS | The C local fixture tests discovery, filtering, provenance, byte/hash correctness, duplicate content, limits, failures, redirects, preflight, cleanup, injected report errors and retries. Full Phase 1–11 regression tests also passed. |
| D10 | PASS | A real controlled CLI run and a keyword-filtered run are recorded below. |
| D11 | PASS with NOT VERIFIED items | Clean Linux build, full suite, strict warnings, ASan/UBSan and archive-hook scan passed. Native Windows was unavailable; leak-enabled LSan was blocked by ptrace. |
| D12 | PASS | README/API/architecture/progress/testing docs and this handoff were updated; the full instruction is retained in the master prompt file. |
| D13 | PASS | Fixture bytes and independently checked SHA-256 values, relationships, filtering, CSV, cleanup, and full regression gates passed. |
| E1 | PASS | The complete accessible master work order is retained; only CRLF-to-LF line endings were normalized. |
| E2 | PASS | Phase 10 and 11 commits remain in history. Phase 12 source commit `afe8ed7fcef9c2f51687c5c983beda0c820d7600` was pushed normally and public GitHub main was verified equal to it. This handoff is a separate documentation receipt; its final SHA is reported in the Replit completion message. |
| E3 | PASS | This file and the completion response provide status, APIs, ownership, test evidence, limitations and Git evidence. |
| E4 | PASS | The master work order was reread during final acceptance. |
| E5 | PASS | Phase 12 handoff is complete; no later phase was started. |

### D9 scenario notes

The local C fixture serves HTML and document bytes on one loopback port. It
checks the root and child depths, duplicate links, source attribution, empty
pages, wrong MIME/HTTP failures, redirected HTML and binary responses, same
host/port scope, blocked redirect targets, timeouts, limits and cycles. The
Phase 10 organizer integration checks different same-size bytes and existing
content-addressed files. The report allocation tests exercise write/close and
cleanup failures. The Phase 12 allocation test also simulates report-write and
report-close status returns at the orchestration boundary, confirms
`PIPELINE_ERROR_REPORT`, and retries successfully with a different report
name. Its `remove` replacement checks cleanup-failure orphan-path ownership.

The report failure wrapper, allocator replacements, and file-operation
replacements exist only in the test object; they are not production APIs.

## Actual controlled CLI demonstration

This is the output from the real C executable and C local fixture, not a
reconstructed transcript. The integration test prints a fresh ephemeral port
and temporary output path on each run. One observed command was:

```text
./build/deepdownload http://127.0.0.1:33241/site /tmp/deepdownload-pipeline-X7Tr5j/out-021 report.csv 1 1
Exit code: 0
```

Observed counters:

| Counter | Value |
|---|---:|
| HTML pages visited / failed | 3 / 0 |
| HTML links discovered | 9 |
| Unique document URLs discovered | 4 |
| Document/source relationships discovered | 6 |
| Unique URLs selected / relationships selected | 4 / 6 |
| Newly stored / content duplicates | 3 / 1 |
| Failed document downloads / skipped by limit | 0 / 0 |
| Successful document bytes / newly stored bytes | 55 / 39 |
| CSV rows written | 6 |

The observed output tree was:

```text
out-021/
  report.csv
  pdf/da609456d4b256ec70a2611b56b6bac2e9ea3162460752a384ae7d3a97cc6cfd.pdf
    16 bytes; SHA-256 da609456d4b256ec70a2611b56b6bac2e9ea3162460752a384ae7d3a97cc6cfd
  docx/7f525b7a512e13d07382af92a865628161eff9ed8ca8558a64e8a8577ae924af.docx
    12 bytes; SHA-256 7f525b7a512e13d07382af92a865628161eff9ed8ca8558a64e8a8577ae924af
  zip/904369fb47f28dd75a6bdf43fa7064031f37e4b7da52078bbc8067466538abd1.zip
    11 bytes; SHA-256 904369fb47f28dd75a6bdf43fa7064031f37e4b7da52078bbc8067466538abd1
  staging/ (empty)
```

The three fixture byte strings were respectively 16, 12 and 11 bytes. Their
SHA-256 values above are asserted by the C test against independently computed
expected digests. The two identical PDF URLs resolve to the same stored path
and hash; the CSV still contains all six provenance relationships.

The exact CSV header and observed representative rows were:

```csv
document_url,document_type,source_page_url,source_depth,status,sha256,size_bytes,stored_path,error
"http://127.0.0.1:33241/docs/rules.pdf","PDF","http://127.0.0.1:33241/site",0,"DOWNLOADED_NEW","da609456d4b256ec70a2611b56b6bac2e9ea3162460752a384ae7d3a97cc6cfd",16,"/tmp/deepdownload-pipeline-X7Tr5j/out-021/pdf/da609456d4b256ec70a2611b56b6bac2e9ea3162460752a384ae7d3a97cc6cfd.pdf",""
"http://127.0.0.1:33241/docs/rules.pdf","PDF","http://127.0.0.1:33241/courses",1,"DOWNLOADED_NEW","da609456d4b256ec70a2611b56b6bac2e9ea3162460752a384ae7d3a97cc6cfd",16,"/tmp/deepdownload-pipeline-X7Tr5j/out-021/pdf/da609456d4b256ec70a2611b56b6bac2e9ea3162460752a384ae7d3a97cc6cfd.pdf",""
"http://127.0.0.1:33241/docs/research.pdf","PDF","http://127.0.0.1:33241/research",1,"DUPLICATE","da609456d4b256ec70a2611b56b6bac2e9ea3162460752a384ae7d3a97cc6cfd",16,"/tmp/deepdownload-pipeline-X7Tr5j/out-021/pdf/da609456d4b256ec70a2611b56b6bac2e9ea3162460752a384ae7d3a97cc6cfd.pdf",""
```

The keyword-filtered CLI run was:

```text
./build/deepdownload http://127.0.0.1:33241/site /tmp/deepdownload-pipeline-X7Tr5j/out-022 report.csv 1 1 --keyword LECTURE
Exit code: 0
HTML pages visited: 3; discovered unique document URLs: 4
Selected URLs / relationships: 1 / 1
New files / duplicates: 1 / 0
Successful document bytes: 12; CSV rows: 1
```

The invalid-depth CLI case returned exit code 2 and the fixture recorded zero
requests. The missing-output-root API case returned
`PIPELINE_ERROR_FILESYSTEM` with zero fixture requests.

## Exact Linux build and test evidence

All commands were run in the C-only checkout on Linux/Replit Nix with GCC and
libcurl 8.14.1:

| Command | Exit | Observed result |
|---|---:|---|
| `make clean` | 0 | Removed `build/`. |
| `make` | 0 | Built the static library and C demos; no compiler warning/error lines. |
| `make test` | 0 | All regression suites passed; no Linux skips or assertion failures; existing live HTTP/HTTPS test passed 7/7. |
| `make test-html-alloc` | 0 | 143 allocation-failure positions passed with no tracked allocations left. |
| `make clean` then `CFLAGS="$CFLAGS -Werror" make test` | 0 | Full suite compiled with `-std=c11 -Wall -Wextra -Wpedantic -Werror` and passed without warnings. |
| `make clean` then `ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 CFLAGS="$CFLAGS -fsanitize=address,undefined -fno-omit-frame-pointer -g" make test-pipeline test-pipeline-integration test-pipeline-alloc` | 0 | Phase 12 tests passed under ASan/UBSan: 50, 268 and 116 checks. |
| `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 CFLAGS="$CFLAGS -fsanitize=address,undefined -fno-omit-frame-pointer -g" make test-pipeline-alloc` | 2 | LeakSanitizer fatal error: “LeakSanitizer does not work under ptrace.” This is NOT a successful leak-enabled scan. |
| `nm -g build/libcrawler.a` scanned for `pipeline_test_`, test report-writer and wrapped allocator/file-operation symbols | 0 matches | No test fault-injection symbols are in the production archive. |

The normal full test run reported:

| Suite | Checks / result |
|---|---:|
| Queue / visited | 3667 / 3670 |
| URL / URL validation / HTML | all pass; HTML reports 14 groups |
| Phase 5 allocation | 3281 |
| Crawler engine / crawler allocation | 372 / 223355 checks; 331 allocation positions |
| HTTP local / HTTP allocation | 81 / 128 |
| Phase 7 safety / Phase 7 allocation | 303 / 271334 checks; 727 failure positions |
| Document unit / integration / allocation | 41 / 127 / 528140 checks; 719 failure positions |
| Download unit / integration / allocation | 140 / 277 / 159 checks; 22 allocation positions |
| SHA-256 / organize unit / organize integration / organize allocation | 34 / 52 / 110 / 174 checks; 9 allocation positions |
| Report unit / CSV integration / allocation | 45 / 167 / 48 checks; 31 write positions |
| Pipeline unit / integration / allocation | 50 / 268 / 116 checks; 7 pipeline allocation positions |
| Existing live HTTP downloader | 7 of 7 passed |

No `warning:` or compiler `error:` line was observed in the normal or strict
build. POSIX local tests did not skip on Linux. Their native Windows behavior
is different: several fixture targets explicitly print `SKIP` there.

## Changed-file inventory

The Phase 12 source commit changed only the following C project files,
Makefile and Markdown documentation:

- `Makefile`
- `README.md`
- `documentation/api.md`
- `documentation/architecture.md`
- `documentation/phase12.md`
- `documentation/progress.md`
- `documentation/testing.md`
- `examples/deepdownload.c`
- `include/pipeline.h`
- `include/report.h`
- `src/pipeline.c`
- `src/report.c`
- `tests/download_fixture.c`
- `tests/download_fixture.h`
- `tests/test_pipeline.c`
- `tests/test_pipeline_alloc.c`
- `tests/test_pipeline_integration.c`
- `tests/test_report.c`

No generated binary, test output, downloaded document, credential, secret or
outer Replit React/TypeScript scaffold was included. The repository contains
53 C source files and 17 C headers. A scan found no Python, JavaScript,
TypeScript, C++, or Rust source files in this C project.

## GitHub evidence and environment limits

Canonical repository:
<https://github.com/jatinvimal1506/Web-Crawler-and-Scraper-in-C>

- Branch: `main`
- Phase 12 implementation commit: `afe8ed7fcef9c2f51687c5c983beda0c820d7600`
- Commit URL: <https://github.com/jatinvimal1506/Web-Crawler-and-Scraper-in-C/commit/afe8ed7fcef9c2f51687c5c983beda0c820d7600>
- Before push, both configured origin and public GitHub main were
  `fe7090f79383af9525af3680ff81c3be2e07a661`.
- A normal, non-force `git push origin main` succeeded.
- After push, both configured origin and live public GitHub `main` were
  `afe8ed7fcef9c2f51687c5c983beda0c820d7600`.

This handoff is added as a documentation-only receipt after the implementation
commit. The exact final receipt commit and live branch SHA are reported in the
completion response; a commit cannot contain its own final hash without
creating another commit.

Native Windows/MSYS2 was not available, so Windows runtime success is
**NOT VERIFIED**. The Makefile retains its Windows branches and
`documentation/testing.md` contains UCRT64 setup/build/test commands. The
full leak-enabled sanitizer result is **NOT VERIFIED** because LeakSanitizer
fails under the workspace's ptrace restrictions. The allocator accounting
tests pass, but they are not a process-wide leak proof.

No mode settings were changed, no deployment or publication was performed,
and no paid or higher-cost capability was intentionally enabled. The user
reported that Free was selected; the active Replit UI selector itself is not
visible for independent verification.

## Stop condition

Phase 12 is complete and its implementation commit is verified on GitHub.
**NO PHASE 13 OR LATER PHASE STARTED.** Any further feature work requires a new
explicit user authorization.
