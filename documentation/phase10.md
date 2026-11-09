# Phase 10: hash and organize one completed file

Phase 10 is a small C11 library step after the Phase 9 one-document download.
It accepts one already-completed local PDF, DOCX or ZIP file, computes its
SHA-256 digest, and stores one copy in a type-named folder. It does not crawl,
download, unpack, inspect document contents, filter keywords, or create a
catalog/report. Those behaviors are outside this phase.

## Build and tests

```sh
make clean
make
make test-sha256
make test-organize
make test-organize-integration
make test-organize-alloc
make test
```

`make test` includes these Phase 10 suites as well as the earlier project
regressions. The dated Phase 10 handoff in `../prompt history/` records the
literal commands, exit statuses, counts, sanitizer observations, and any
environment limitations for the delivered revision.

## Hashing API

`include/sha256.h` exposes:

- `sha256_status_message(status)` returns a static explanation; do not free it.
- `sha256_file(path, hex_out, bytes_read)` reads a borrowed file path
  incrementally. `hex_out` must point to at least 65 bytes. On success it
  contains 64 lowercase hex digits and a terminating NUL. The optional byte
  count is written only on success. Output values are cleared on failure.
  Empty files are hashable by this general helper; the organizer rejects them
  as documents.

The implementation is plain C11 and has no OpenSSL or other hash-library
dependency. It reads in a fixed 16 KiB buffer, so memory use is constant rather
than proportional to the file size. The accepted file size is at most 20 MiB.
Time is O(number of input bytes); SHA-256 has a fixed amount of work per block.
The test suite includes independently checked known vectors, including empty
input, text, binary bytes, one million `a` bytes, and the size boundary.

## Organization API and ownership

`include/organize.h` exposes:

- `organizer_status_message(status)` returns a static string.
- `organize_result_init(result)` initializes a zeroed result without
  allocating. Start with `{0}`.
- `organize_result_free(result)` frees its owned `stored_path`, resets the
  result, and is safe for NULL or repeated cleanup.
- `organizer_prepare_directories(output_root)` requires an existing trusted
  root and creates or verifies only its immediate `pdf`, `docx`, and `zip`
  directories. It does not erase existing files.
- `organize_file(input_path, type, output_root, result)` borrows the input path
  and root for the call. The input must be a nonempty regular file no larger
  than 20 MiB. Initialize `result` as `{0}`. On `ORGANIZER_NEW_FILE`,
  `ORGANIZER_DUPLICATE`, or `ORGANIZER_ERROR_CLEANUP`, `result->stored_path`
  belongs to the result and must be released with `organize_result_free`.
  Other failures do not transfer a path. The caller's input is never modified
  or removed.

A new unique file is named
`<root>/<pdf|docx|zip>/<64-lowercase-SHA-256-digits>.<pdf|docx|zip>`.
The organizer checks all three folders for identical content, so identical
bytes are deduplicated even when a later caller supplies a different document
type. The first stored file's actual folder/type is returned for duplicates.
The hash match is followed by a byte-for-byte comparison. If a file at the
expected hash path has different bytes, the operation returns a conflict and
never overwrites it.

The destination is created exclusively; existing files are not replaced. Copy
uses a fixed-size buffer and then hashes and checks the destination's byte count
again. A mismatch or I/O failure removes only the new partial file. If removing
that new file fails, `ORGANIZER_ERROR_CLEANUP` is returned with its owned path
so the caller can locate it. The source file remains untouched on every path.
The organizer uses O(file bytes) time for hashing/comparison/copy and constant
buffer memory. It checks only three fixed type folders; it is not a general
directory crawler.

On POSIX, symlink output roots, symlink type directories, and symlink input
files are rejected. The Windows implementation uses the available CRT file
checks; it does not promise to detect every junction or reparse point. Native
Windows execution must be verified separately.

The public result owns a path string, not an open file handle. Do not shallow
copy a populated result. The SHA output buffer is caller-provided storage and
the paths passed into the functions are borrowed. The module does not retain
the input path after the function returns.

## Test coverage

- `tests/test_sha256.c`: known-answer vectors and empty, binary, maximum-size,
  and over-limit files.
- `tests/test_organize.c`: API initialization/cleanup, type folders, new files,
  duplicate content across types, returned ownership, and input preservation.
- `tests/test_organize_integration.c`: real temporary files and directories,
  same-size different content, pre-existing correct and conflicting hash paths,
  directory and symlink cases, empty/oversized input, and missing roots.
- `tests/test_organize_alloc.c`: test-only C wrappers fail every observed
  allocation position and inject read, write, close, and cleanup failures. It
  checks cleanup and successful reuse after failures. These objects are
  separate from `libcrawler.a`.

The integration and allocation fixtures use POSIX filesystem facilities and
print `SKIP` on native Windows. The portable SHA and basic organization suites
remain C tests, but this repository's Windows runtime has not been verified
unless the handoff reports an actual Windows run. Allocation counters and
sanitizers are useful checks, not a proof against every possible leak or race.

## What remains outside Phase 10

Phase 11's metadata keyword filtering and CSV export are not implemented here.
Phase 12's bounded end-to-end discovery/download/reporting orchestration is
also pending. Phase 10 handles one caller-selected, completed local file only.
