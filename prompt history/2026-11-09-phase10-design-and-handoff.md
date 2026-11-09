# Phase 10 design decisions and acceptance handoff

Date: 2026-11-09

This is a factual Phase 10 implementation and test handoff. The complete
three-phase master work order is retained verbatim (with CRLF line endings
normalized) in
[`2026-11-09-phases10-12-master-instruction.md`](2026-11-09-phases10-12-master-instruction.md).
This record does not replace or rewrite earlier historical prompts.

## Scope delivered

Phase 10 adds two independent C11 library modules:

1. `sha256_file` incrementally computes a lowercase SHA-256 digest for one
   local file, with fixed memory and a 20 MiB inclusive size ceiling.
2. `organize_file` accepts one already-completed PDF, DOCX or ZIP file, checks
   its bytes against digest-named files in the three type folders, and either
   reports an identical existing copy or creates and verifies a new copy.

No crawler integration, download loop, file extraction, keyword filtering,
CSV, UI, AI, or Phase 12 orchestration was implemented. Phase 11 and Phase 12
remain behind their separate acceptance gates.

## Design decisions

- **Implement SHA-256 in ordinary C11 rather than add a package.** This keeps
  the university project self-contained and makes the transformation visible
  for learning. The public file helper uses a fixed 16 KiB buffer rather than
  loading a whole document. The independent known-answer tests guard the
  algorithm.
- **Use the already-known document type to choose a folder.** Phase 10 receives
  a caller-selected completed file and `DocumentType`; it does not repeat URL
  classification, inspect file signatures, or claim that a MIME label proves a
  valid PDF/DOCX/ZIP structure.
- **Deduplicate across all three folders and compare bytes.** A digest match
  is not blindly trusted: matching candidates must have the expected size and
  pass a byte-for-byte comparison. If the canonical digest path exists with
  different bytes, return a conflict rather than overwrite it.
- **Use exclusive create, then verify the stored copy.** The new file is opened
  with an exclusive-create flag, copied in bounded chunks, closed, and hashed
  again. The final byte count and digest must match the source. On error,
  remove only the destination created by this call; never remove or alter the
  borrowed input. If cleanup fails, return a distinct status and preserve the
  owned leftover path in the result.
- **Require an existing trusted output root.** Only its immediate `pdf`,
  `docx`, and `zip` directories are created/checked. POSIX `lstat` rejects
  symlink roots, child folders and input files; the Windows CRT path cannot
  guarantee detection of every junction or reparse point.
- **Keep this a one-file operation.** A general folder scanner, pipeline
  orchestration, report format, and automatic download were deferred to later
  authorized phases.

Rejected alternatives: importing OpenSSL would add a dependency; buffering the
whole file would make memory scale with document size; overwriting a matching
path could destroy user data; trusting a digest without comparing bytes would
hide a collision or corrupt preexisting file; and coupling this module to the
crawler would exceed Phase 10.

## Acceptance checklist

| Requirement | Result | Evidence / limitation |
|---|---|---|
| C11 SHA-256 implementation and public header | PASS | `include/sha256.h`, `src/sha256.c`; no third-party hash library. |
| Empty, known text, binary, 1,000,000-byte and boundary hash vectors | PASS | `test-sha256`: 34 checks, 0 failures; maximum-size expected digest independently checked with system `sha256sum`. |
| 20 MiB limit and over-limit rejection | PASS | Exact maximum and maximum+1 cases are included in the deterministic C suite. |
| Type folders and digest-named unique files | PASS | `test-organize` and local filesystem integration suite. |
| Duplicate detection across PDF/DOCX/ZIP folders | PASS | Same bytes under different declared types return the existing copy/type; distinct same-sized data stays separate. |
| Exclusive create; no overwrite of existing or conflicting content | PASS | Integration cases include pre-existing canonical correct content and a conflicting file at a digest path. |
| Recheck stored bytes/hash and size after copy | PASS | Implemented in `organize_file`; successful copy tests assert the stored content/result digest and byte count. The concurrent external-mutation race is not simulated. |
| Remove only this call's incomplete destination; preserve input | PASS | Read/write/close/verification failure paths use a `created` flag; integration/fault tests check source unchanged and no partial output after successful cleanup. Injected remove failure returns a cleanup status and owned leftover path. |
| Clear ownership and safe cleanup | PASS | `OrganizeResult` owns `stored_path` only for new, duplicate, or cleanup-failure results; repeated NULL-safe free and tracked cleanup are tested. |
| Allocation and I/O fault injection, production hooks excluded | PASS | 174 checks; 9 allocation positions; read/write/four close positions/remove failure and retry cases. `nm` found no `organize_test_*` symbols in `libcrawler.a`. |
| Preserve earlier Phases 1–9 and APIs | PASS | No earlier application source or existing test source was modified. The full regression `make test` exited 0. |
| Update current docs without rewriting old history | PASS | README, API, architecture, progress and testing docs were updated; old prompt-history entries were retained. |
| Native Windows/MSYS2 compatibility | NOT VERIFIED | Makefile's Windows `cmd.exe`/`.exe` branch remains. Native Windows was unavailable; POSIX filesystem/injection targets contain explicit Windows SKIP branches. Junction/reparse behavior is not guaranteed. |
| Full LeakSanitizer/Valgrind proof | NOT VERIFIED | Leak-enabled runs terminate with the environment's ptrace restriction. Valgrind is not installed. Allocation accounting and ASan/UBSan are not a universal leak proof. |
| GitHub commit and upload | PENDING AT THIS HANDOFF DRAFT | The code/test work is not considered fully delivered until the safe commit/push attempt and remote SHA verification are recorded below. |
| Power/advanced mode and deployment | NOT CHANGED | No mode/configuration change, upgrade, publish or deployment was made. The user reported Free selected; the account UI selection cannot be independently inspected here. |

## Functions and memory in beginner terms

### Hash module

- `sha256_status_message` returns a pointer to a fixed, static message. The
  caller must not free it.
- `sha256_file(path, hex_out, bytes_read)` borrows `path`; it does not keep the
  name. `hex_out` points to caller-owned space for 65 bytes. The optional
  `bytes_read` pointer receives the size. The helper allocates no heap memory;
  the digest is produced from a local context and 16 KiB read buffer. On error,
  outputs are cleared.

### Organizer module

- `organizer_status_message` returns static text; do not free it.
- `organize_result_init` initializes an empty/zeroed result and allocates
  nothing.
- `organize_result_free` frees the result's owned path string and resets all
  fields. It is safe to call repeatedly or with NULL.
- `organizer_prepare_directories` borrows the root string and prepares the
  three immediate type folders. It stores no path pointers after returning.
- `organize_file(input_path, type, output_root, result)` borrows both path
  strings for the duration of the call. It allocates temporary candidate path
  strings and frees them before returning. The returned result owns
  `stored_path` only for `ORGANIZER_NEW_FILE`, `ORGANIZER_DUPLICATE`, or
  `ORGANIZER_ERROR_CLEANUP`; use `organize_result_free` to release it.

Do not shallow-copy an `OrganizeResult` after it owns a path: two structs would
then point to the same allocation and could double-free it. The input file
remains owned by the caller, and this module never removes it.

## Commands and observed results

Environment: Linux, GCC, GNU Make, libcurl 8.14.1. All commands ran in the
C-only Git checkout.

| Command | Exit | Result |
|---|---:|---|
| `make clean && make && make test` | 0 | Clean default build and complete test target passed; no `warning:` or `error:` lines. Existing live HTTP suite passed 7/7. |
| `make test-sha256 test-organize test-organize-integration test-organize-alloc` | 0 | Phase 10: 34 + 52 + 110 + 174 checks; all 0 failures; 9 allocation positions. |
| Strict `make clean && make CFLAGS="$(pkg-config --cflags libcurl) -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude"` followed by all four Phase 10 targets with the same CFLAGS | 0 | Entire production library, demos, and Phase 10 test executables compiled with `-Werror`; all Phase 10 tests passed. |
| ASan/UBSan Phase 10 build/tests with `-fsanitize=address,undefined -fno-omit-frame-pointer`, `ASAN_OPTIONS=detect_leaks=0`, `UBSAN_OPTIONS=halt_on_error=1` | 0 | All four suites passed: 34, 52, 110 and 174 checks; no ASan/UBSan diagnostic. |
| Leak-enabled ASan execution of each Phase 10 test binary | 1 each | Test body printed its passing checks, then LeakSanitizer exited with “does not work under ptrace”. This is NOT a passed leak scan and did not identify a project leak. |
| `command -v valgrind` | n/a | `VALGRIND=NOT_INSTALLED`. |

The full `make test` regression output included: queue 3,667 checks; visited
3,670; Phase 5 allocator 3,281; crawler engine 372 checks/25 site scenarios;
crawler allocator 223,355 checks/331 injected positions; Phase 7 safety 303;
Phase 7 allocator 271,334 checks/727 positions; document unit 41, integration
127, allocator 528,140 checks/719 positions; downloader unit 140,
integration 277, allocator 159/22 positions; and the Phase 10 counts above.
The HTML, HTTP local/metadata, URL/API and legacy network-dependent suites also
passed as prerequisites of the zero-exit full `make test`. No SKIP was printed
on this Linux run.

Two initial verification commands were invalid because they replaced Make's
environment-provided libcurl flags: a strict build lacked `curl/curl.h`, and a
sanitizer link lacked `-lcurl` search paths. Both were rerun successfully with
`pkg-config --cflags/--libs libcurl` included. These were command setup errors,
not code-test failures.

## Remaining limitations and next gate

- The organizer trusts the caller-provided `DocumentType`; it does not parse
  file signatures or validate a PDF/DOCX/ZIP structure.
- The output root must exist and be trusted. POSIX symlink checks do not make a
  concurrently modified parent directory race-proof; operations are intended
  for the project's single-threaded use, not concurrent external mutation.
- This is one local file, not a recursive filesystem scan or download pipeline.
- Windows runtime, junction/reparse-point behavior, and a complete process-wide
  leak scan remain unverified.
- Phase 11 and Phase 12 must not start until the Phase 10 commit is safely
  pushed and this handoff contains the confirmed local/remote SHA receipt.

### GitHub receipt (complete after push)

At the time this draft was written, the working branch was `main`, based on the
verified Phase 9 receipt `b42ccaff650cc94a7db5ccf1f5c1e67d7ac1c8b6`. The Phase 10
changes were not yet committed or pushed. Record the final commit, remote head,
and one non-force push result here after verifying the remote has not diverged.
