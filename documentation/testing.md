# Testing

Run from `DeepDownload-C/` with GCC, Make and libcurl development files:

```sh
make clean
make
make test
make test-html-alloc
make test-http-local
make test-http-alloc
make test-phase5
make test-phase5-alloc
make test-crawler-engine
make test-crawler-alloc
make test-document
make test-document-integration
make test-document-alloc
make test-download
make test-download-integration
make test-download-alloc
make test-sha256
make test-organize
make test-organize-integration
make test-organize-alloc
make test-report
make test-report-csv
make test-report-alloc
make test-pipeline
make test-pipeline-integration
make test-pipeline-alloc
```

`make` also builds the retained presentation demo, the real `crawler-bfs` CLI,
and the Phase 12 `deepdownload` CLI.
The basic API test checks invalid arguments, statistics initialization/statuses
and CRAWLER_OK on a controlled empty HTML page from a C stub (NOT the former
NOT_IMPLEMENTED result). The URL suite retains parsing/normalization/resolution,
boundaries and ownership regressions. Use the controlled C fixture below for
the current demo, not an unpermissioned public-site crawl.

`make test` includes the original API/URL tests, standalone HTML tests, local
HTTP fixtures, Phase 5 tests, HTTP metadata allocation-failure tests, Phase 8
discovery, Phase 9 download tests, Phase 10 hash/organization tests, Phase 11
report tests, Phase 12 pipeline tests, plus original live HTTP tests. The live
suite may skip probes or fail when public network access is blocked. Local
fixtures use only C servers bound to loopback on a free port.

Phase 8's `test-document` covers URL-path extension parsing, case, query/
fragment handling, invalid inputs and result accessor/cleanup behavior.
`test-document-integration` uses the existing C loopback BFS fixture to check
root/depth discovery, unique normalized document/source pairs, preserved
meaningful query differences, redirected relative links, same-domain/port
filtering, partial failures, empty HTML, the 1000-result boundary and that
recognized files are not fetched. `test-document-alloc` compiles the C project
modules with test-only allocation wrappers, injects failure at each of 719
observed application C-module allocation points in the controlled discovery run, and checks
all-or-empty rollback, cleanup and a successful retry. Those wrappers are
separate test objects and are not included in `libcrawler.a`.

Run the end-to-end local demonstration with `make build/crawler-fixture` and
`./build/crawler-fixture --documents`. It launches only the controlled C
loopback site and the C `discover-documents` CLI.

The HTML suite covers quoted/unquoted values, literal/entity-encoded query
delimiters, malformed attributes, raw-text exclusions (including iframe),
plaintext, entities, unsafe schemes, duplicates, ownership and truncations.
`make test-html-alloc` uses GNU linker wrapping to fail allocations in a
representative HTML/URL extraction and count outstanding allocations.

`make test-http-alloc` builds a separate HTTP test object with C allocator
replacements. The installed library has no test hooks. The test fails each
metadata-copy position, checks `HTTP_ERR_OUT_OF_MEMORY`, empty/reset response
fields, immediate cleanup, repeated free and successful reuse. It also proves
that a missing optional Content-Type is not an allocation failure. Its tracking
covers allocations made by the HTTP object, not libcurl's internal allocations.

Production code retains the Linux/Windows Makefile branches. The local C
fixtures and their HTTP allocation tests require POSIX fork/sockets and print
SKIP on native Windows. Windows runtime and older-libcurl fallback execution
are not verified here. An old MSYS2 report of unavailable libasan is historical,
not evidence about the current Linux toolchain.

Historical Phase 4 results remain in `phase4-test-results.md`. Current Phase 7
commands/results are in `../prompt history/2026-11-09-phase7-acceptance-and-handoff.md`.
LeakSanitizer availability can vary: isolated runs have passed while a full
sweep has failed with ptrace restrictions. Report each actual command/result;
neither blanket success nor universal unavailability is justified.
Allocation accounting is not a complete process leak scan. There is no comprehensive
HTML5 parser, fuzzing campaign, or deterministic local TLS fixture.

At Phase 7 delivery, Phases 1–7 covered the original crawler and Phase 7
hardened its timeout/redirect safety; document downloading was not in that
phase. Phases 8 and 9 were approved and implemented separately afterward.

## Phase 5 tests

`make test-phase5` runs `test_queue.c` and `test_visited.c`. Each tests 1200
pending/stored URLs plus invalid inputs, ownership, normalization/boundary
semantics and repeated cleanup. Queue tests retain exact depth/FIFO pairing.
The count-overflow tests synthetically set SIZE_MAX and restore it immediately:
they exercise guards, not an impossible full-size allocation.

`make test-phase5-alloc` builds separate queue/visited/url C objects whose
malloc/free names are redirected to test counters. Every allocation in a
representative queue enqueue and four visited operations is failed in turn.
Existing items/keys, counts and FIFO remain intact, temporary allocations
are reclaimed, and the same containers accept a successful retry. Current
modules use malloc/free, not realloc/calloc. No hooks enter `libcrawler.a`.
These tests are included in `make test`, require no network/POSIX sockets,
and their checks are not disabled by NDEBUG.

Literal Phase 5 clean-build, regression, sanitizer commands and measured
results are recorded in `../prompt history/2026-11-07-phase5-handoff.md`.
Allocation accounting is not a complete process leak scan. Windows runtime
is NOT VERIFIED. Phase 5 code/tests themselves use only standard C11.

## Phase 6 acceptance tests and controlled demo

`make test-crawler-engine` uses `crawler_fixture.c`: a C-only POSIX loopback
server with dynamic ports and request-order pipe records. It checks real
libcurl/HTML/URL/queue/visited integration, BFS depth/order, all five stats,
failures/aliases/scope/ports and both sides of the resource budgets. No public
site or proxy is needed. `same_domain=0` tests cross-hostnames only on loopback.
Known failed final aliases are not retried as queued direct URLs. The reverse
unknown-alias case explicitly verifies the unavoidable duplicate redirect fetch.

`make test-crawler-alloc` uses separate instrumented project/url/html/queue/
visited objects and a C in-memory HTTP stub. It fails every allocation position
in a representative complete three-page crawl; checks queue OOM, visited OOM,
post-enqueue marking failure, retrying a new crawl after failure, zero tracked
allocations and synthetic INT_MAX/invariant boundaries. Large-frontier/candidate
budget cleanup is also accounted. These objects/stubs NEVER enter libcrawler.a.

The basic API test retains invalid-argument/stat-zeroing/status assertions.
Its old valid-call NOT_IMPLEMENTED expectation is replaced by OK/one empty
HTML page from the C stub. It stays portable and has no live network dependency.
All other Phase 1–5 tests remain unchanged.

The two Phase 6 targets are included in `make test`; original live HTTP tests
remain network-dependent and must be distinguished from local failures/skips.
Local engine fixtures/launcher print SKIP on native Windows; the in-memory
allocation/API fixtures themselves are standard C11.

```sh
make build/crawler-fixture
./build/crawler-fixture --demo
```

This starts the controlled local site and runs the real BFS CLI. It is not a
fabricated transcript. See the Phase 6 handoff for exact observed output,
commands/exits/counts and sanitizer flags. No test-only hooks are in release
artifacts, no new dependencies are installed, and no UI/browser is involved.

## Phase 7 safety and allocation targets

```sh
make test-phase7-safety
make test-phase7-alloc
```

Both are included in `make test`. The three old Phase 6 expectations that
accepted a network fetch to a blocked redirect target now require ZERO such
requests (intentional policy correction); all other scenarios are retained.

`test_phase7_safety.c` tests timeout 0/-1/LONG_MAX/3601 rejection with no
server requests; 1/3/3600 success; failures of 17 essential curl options before
networking; option OOM; 301/302/303/307/308; relative/query/fragment/scheme-relative
Location; exactly five and excessive/looping redirects; absent/invalid/duplicate/
unsupported Location; redirected 404/500/PDF; partial body and timeout cleanup;
cross-host open/strict and cross-port blocking; recoverable child scope rejection;
cross-page candidate and redirect-alias scheduled-key budgets. Standalone
automatic redirects retain old behavior. No public site is needed.

`test_phase7_alloc.c` instruments all seven production modules and fails each
application allocation in real scoped success, blocked-target, loop and actual
redirecting BFS profiles. It verifies OOM propagation, empty OOM output and
zero tracked allocations after cleanup/retry. An explicitly fake single-hop
test proves same-port HTTP->HTTPS URL policy and HTTPS downgrade rejection:
it is NOT a TLS handshake/certificate fixture. Native TLS remains verified
by existing settings/live tests only; deterministic local TLS is NOT VERIFIED.

Test-only option/allocator/transfer hooks are separate objects, never release
archive members. This adds no production allocator controls or CLI flags.

## Phase 9 binary-download tests

`test-download` covers empty/invalid result handling, timeout and filename
validation, unsupported input types, and the host/port/HTTPS-downgrade scope
predicate. Invalid configuration is rejected before it can contact the local
fixture.

`test-download-integration` runs a C-only POSIX loopback server. It verifies
byte-for-byte saving of embedded-NUL/high-byte payloads, PDF/DOCX/ZIP MIME,
the documented generic binary MIME fallback, duplicate/missing/wrong MIME,
empty and rejected HTTP responses, truncation, dropped connection and a
one-second timeout. It exercises relative and 301/302/303/307/308 redirects,
loop and hop limits, malformed/missing/unsupported Locations, a final 404 and
wrong MIME, and both host/port scope rules. The blocked redirected destination
must have a zero request count. It tests exact 20 MiB and 20 MiB + 1 with both
Content-Length and chunked framing; saved bytes are checked against a
deterministic pattern. Existing output files are checked for unchanged bytes.

`test-download-alloc` compiles isolated test versions of the C modules with
allocation and file-I/O replacements. It injects allocation failures into every
observed application allocation, plus `fwrite`, `fclose` and partial-file
`remove` failures. It verifies result ownership, cleanup status, no false
success, and safe manual removal when cleanup itself is deliberately blocked.
These replacements are test-only; confirm the production archive separately
with `nm` or the platform's symbol tool.

Measured Phase 9 results on the Linux Replit/Nix environment with GCC and
libcurl 8.14.1: `make clean`, `make`, and the complete `make test` each exited
0. The new suites passed with 140 unit checks, 277 local integration checks,
and 159 allocation/file-failure checks including 22 allocation-failure
positions. The local CLI fixture saved 8 bytes and verified them byte-for-byte.
Strict `-std=c11 -Wall -Wextra -Wpedantic -Werror` build and targeted tests
passed. Targeted AddressSanitizer/UndefinedBehaviorSanitizer tests passed with
leak detection disabled. A leak-enabled run aborted with LeakSanitizer's
environment error that it does not work under ptrace; this is not a successful
leak scan. The existing live HTTPS regression also passed 7/7; all new
downloader integration tests use only loopback fixtures. Windows runtime was
not run.

The local integration and allocation-fault servers use POSIX sockets/processes
and explicitly SKIP on native Windows. `make` retains the MSYS2 UCRT64 branch,
but Windows runtime is NOT VERIFIED unless these commands are actually run in
that environment. Strict C11 warnings and sanitizer results belong in the
Phase 9 acceptance handoff and must be based on the actual commands run.

## Windows/MSYS2 UCRT64 (native execution NOT VERIFIED here)

In the MSYS2 **UCRT64** terminal, install the normal free C dependencies if needed:

```sh
pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-curl make git
cd /c/path/to/Web-Crawler-and-Scraper-in-C
make clean
make
make test
make test-phase5-alloc
make test-crawler-alloc
make test-html-alloc
./build/crawler-bfs.exe http://YOUR_CONTROLLED_HOST:PORT/ 1 1
```

The Makefile retains cmd.exe /C, .exe and native cleanup branches. If using
cmd.exe directly, use `build\\crawler-bfs.exe` and ensure UCRT64/bin and MSYS2
make are on PATH. Link against UCRT64 libcurl, not a mismatched MINGW64 package.
POSIX fork/socket targets explicitly print SKIP on native Windows; portable
API/URL/HTML/queue/visited/in-memory engine tests remain. A successful Makefile
dry-run is NOT Windows runtime evidence. Windows allocator/linker wrapping and
native sanitizer runtime availability must still be checked on a real target.

## Phase 10 hashing and organization tests

`make test-sha256` checks independently verified known digests for empty, text,
binary, one-million-byte and maximum-sized inputs, and rejects a file larger
than the 20 MiB limit. `make test-organize` covers result initialization and
cleanup, supported types, output folders, duplicate content across types,
caller-owned input preservation and returned path ownership.

`make test-organize-integration` uses local temporary files/directories to
check byte-identical duplicate files, different same-sized files, a preexisting
correct digest path, a conflicting file at a canonical digest path, empty and
oversized inputs, missing roots, symlink rejection where POSIX supports it, and
directory collisions. It needs POSIX `mkdtemp`/symlink facilities and prints
`SKIP` on native Windows.

`make test-organize-alloc` builds separate C test objects with allocation and
stdio/file-operation wrappers. It injects each observed allocation failure and
read/write/close/remove failures, checks that only new partial destinations
are cleaned up, verifies inputs remain unchanged, tracks outstanding
application allocations, and retries successfully after failures. The wrappers
are not linked into `libcrawler.a`. It also explicitly skips on native Windows.
`make test` runs all four Phase 10 targets alongside the previous suites.

This accounting is not a process-wide leak proof. Actual clean-build, suite,
strict compiler, sanitizer, Windows and Git evidence for this delivery is
recorded in `../prompt history/2026-11-09-phase10-design-and-handoff.md`.

## Phase 11 metadata filter and CSV tests

`make test-report` is a portable C unit suite for zero through eight keywords,
one-to-64-byte printable ASCII validation, empty-filter inclusion,
case-insensitive substring matches in either URL, any-keyword behavior,
overlap/near-match cases, malformed rows and filter inputs, and proof that the
matcher does not depend on download results. It does no I/O. The implementation
uses a fixed KMP prefix array to avoid repeated rescanning of long near-matches.

`make test-report-csv` writes deterministic files in a temporary directory and
compares exact bytes. It checks the header, output order, multiple source rows
for one document, quote/comma/CRLF handling, formula-prefix mitigation, empty
fields, matching and excluded counts, header-only reports, safe basenames,
existing-file preservation, invalid inputs, the 1000-row boundary and
caller-string preservation.

`make test-report-alloc` uses separate C test objects and injected `malloc`,
`fwrite`, `fclose` and `remove` failures. It checks first/last write failures,
allocation failure, close failure, cleanup failure with an owned leftover path,
successful retry, and outstanding allocation accounting. No wrappers are
linked into the release static library.

The filesystem and fault-injection targets use POSIX temporary directories and
explicitly print `SKIP` on native Windows. The unit matcher is portable C11.
Native Windows execution remains NOT VERIFIED. Actual Phase 11 counts and full
regression results are in
`../prompt history/2026-11-09-phase11-design-and-handoff.md`.

## Phase 12 end-to-end pipeline tests

`make test-pipeline` is a deterministic C API/validation suite for result
initialization/cleanup, status mapping and invalid configuration. It does not
use public websites.

`make test-pipeline-integration` starts the shared C-only POSIX loopback fixture
and runs the real `deepdownload` executable as a child process. Its assertions
cover:

- Full root/courses/research discovery, duplicate URL and source relationships,
  depth 0/1/2, exact FIFO request order, and the depth-two document source row.
- Keyword selection from both a document URL and a source-page URL, no matches,
  header-only reports, and filtering before downloads.
- A failed HTML child while preserving the root's document discovery and
  writing a partial report.
- Redirected HTML with relative href resolution from its effective URL and a
  binary-file relative redirect.
- Same-host/port restrictions, no request to blocked redirect destinations,
  permitted cross-host/same-port behavior when configured, and duplicate bytes.
- Existing canonical content preservation, temporary staging-name collision
  safety, pre-existing report rejection before network access, and an empty
  staging directory after normal completion.
- The ten-unique-document attempt limit, 404/500/wrong-MIME/empty/missing-MIME/
  interrupted responses, short timeout, and CSV status/stat consistency.
- Real unfiltered and keyword-filtered CLI runs against the local fixture,
  including actual exit codes, counters, output tree, file sizes, SHA-256
  values and CSV header/representative rows.
- Invalid CLI depth and a missing output root both stop before network access.

`make test-pipeline-alloc` compiles a separate test-only version of
`src/pipeline.c` with a failing allocator and test-only replacements for
`remove` and `report_write_csv`. It injects failure at every observed allocation
in a one-document run, checks cleanup and repeated result freeing, and verifies
that a staging-directory collision makes zero requests. The tests force a
staging cleanup failure and check the returned orphan path, and simulate CSV
write/close errors to verify pipeline status propagation; each failure is
followed by a successful retry with a different report filename. Actual report
file-write and close failures are also covered by `make test-report-alloc`.
These replacements are not included in `libcrawler.a`.

All three targets are included in `make test`. The integration and allocation
fixtures use POSIX sockets, `fork`, and temporary filesystem functions and
explicitly print `SKIP` on native Windows. `make` retains its Windows/MSYS2
branches, but a Linux run is not evidence of Windows runtime success. Current
counts, literal command exit statuses, compiler warnings, sanitizer results
and remaining limitations are recorded in
`../prompt history/2026-11-09-phase12-design-and-handoff.md`.

### Phase 12 Linux verification

On the Linux Replit/Nix environment with GCC and libcurl 8.14.1, the final
commands `make clean`, `make`, and `make test` each exited 0. The complete
suite reported no skips or assertion failures; its existing live HTTPS
downloader checks passed 7/7. A separate strict run,
`CFLAGS="$CFLAGS -Werror" make test`, also exited 0 with no compiler warnings.

The Phase 12 targets reported 50 unit checks, 268 local end-to-end checks, and
116 allocation/cleanup checks across seven allocation failure positions. The
optional `make test-html-alloc` target passed all 143 HTML allocation failure
positions. The local CLI fixture prints actual unfiltered and keyword-filtered
runs, output files and hashes, CSV header/rows, and an invalid-depth run that
returned exit 2 with zero fixture requests.

Targeted AddressSanitizer/UndefinedBehaviorSanitizer runs of the three Phase 12
targets passed with `ASAN_OPTIONS=detect_leaks=0` and no ASan/UBSan diagnostics.
Leak-enabled execution was attempted separately but exited 2 because
LeakSanitizer reported that it does not work under ptrace in this environment;
therefore a leak-enabled sanitizer result is NOT VERIFIED. The C allocation
accounting tests did pass, but are not a process-wide leak proof.

This is Linux-only evidence. Native Windows/MSYS2 execution was not available
and remains NOT VERIFIED. The POSIX local fixture targets explicitly skip on
native Windows; the Makefile's Windows branches and setup instructions are
maintained but are not claimed as runtime-tested.
