# Phase 11: Metadata Filtering and CSV Reporting

Phase 11 adds a reusable C11 filter for already-collected document metadata and
a bounded CSV writer. It does not fetch pages or files, inspect PDF/DOCX/ZIP
contents, or join the crawler and downloader into an end-to-end workflow. That
integration remains Phase 12.

## What one row means

One `ReportRow` represents one relationship between a document URL and the
HTML page where that URL was found. A document found on two source pages should
normally appear twice, once per source, in the order supplied by the caller.
The report writer does not deduplicate or reorder rows.

Filtering checks only `document_url` and `source_page_url`. It is intended to
run against Phase 8's discovered metadata before a download result is known.
No document body is opened or searched.

## Public types and functions

See `include/report.h` for complete declarations and status values.

- `ReportFilter` contains a borrowed array of zero to eight borrowed keyword
  strings. Each keyword must be 1–64 printable ASCII bytes. Control bytes and
  non-ASCII bytes are rejected. An empty filter selects every metadata row.
- `ReportRow` contains borrowed URL/type/status/hash/path/error strings,
  `source_depth`, and `size_bytes`. `report_row_matches` uses only the two URL
  fields; `report_write_csv` validates every field before creating a file.
- `ReportResult` contains the status, counts, a possible diagnostic, and an
  optional owned `output_path`.
- `report_status_message` and `report_row_status_name` return static text.
  Callers must not free these strings.
- `report_result_init` initializes fresh or previously freed storage without
  allocating. Do not call it while the result owns an output path; call
  `report_result_free` first.
- `report_result_free` frees its owned output path, resets the fields, and is
  safe to call with NULL or more than once.
- `report_row_matches(row, filter, matches)` validates the two metadata URLs,
  clears `*matches`, then sets it to one if any keyword is a case-insensitive
  ASCII literal substring of either URL. With zero keywords it selects the row.
  It allocates no memory and has no filesystem or network effects.
- `report_write_csv(directory, basename, rows, count, filter, result)` writes a
  new report after validating the inputs and the full row contents. Its input
  pointers are borrowed only during the call. It never modifies caller strings.

The keyword matcher uses a fixed-size Knuth–Morris–Pratt prefix table to avoid
rechecking long overlapping prefixes. For a text field of length `n` and a
keyword of length `m`, one match is O(n + m), with at most eight keywords and
two metadata fields. The table is bounded by the 64-byte keyword maximum and
uses no heap memory.

## Row outcome and ownership rules

All row strings are borrowed from the caller and remain unchanged. Use an empty
string when a hash, stored path, or error detail is unavailable; a NULL pointer
is invalid for CSV output.

- `DOWNLOADED_NEW` and `DUPLICATE` rows must have a 64-character lowercase
  SHA-256, nonempty stored path, and nonzero size.
- `DOWNLOAD_FAILED` and `SKIPPED_LIMIT` rows must have empty hash and stored
  path. The error detail may be empty when no useful detail is available.

The matcher ignores download outcome fields by design, so a zero-initialized
row with only its two URL pointers set is enough to filter before downloading.
The CSV writer validates the full row so it cannot emit a success status with
missing hash/path metadata or an error status that falsely claims a stored
copy.

`ReportResult.output_path` is allocated by the writer and owned by the result
on success. If an output failure occurs and cleanup of the partial file also
fails, the result owns the leftover path and returns
`REPORT_ERROR_CLEANUP`. Release either owned path using
`report_result_free`. On ordinary errors there is no owned output path. The
result itself does not own rows or their input strings.

## CSV format and safety

The exact header is:

```text
document_url,document_type,source_page_url,source_depth,status,sha256,size_bytes,stored_path,error
```

Every text field is double-quoted, and embedded double quotes are doubled.
Commas and embedded CR/LF are kept inside their quoted field. Each record ends
with CRLF. Depth and byte counts are decimal and unquoted. Text that could be a
spreadsheet formula is prefixed with an apostrophe if its first byte after
leading ASCII whitespace/control bytes is `=`, `+`, `-`, or `@`. The writer
does not alter the source string.

The writer accepts up to 1,000 input rows and 4,096 bytes per text field. The
row limit applies before filtering. It requires an existing directory and a
safe ASCII basename of at most 128 bytes ending in `.csv`; path separators,
traversal and common Windows device names are rejected. It creates the file
exclusively, so an existing file is never overwritten. If no rows match, the
writer emits the header only. `rows_written` counts complete data rows and
`rows_excluded` counts input rows not selected by the filter; neither count
includes the header.

If writing or closing fails, only the new partial file created by this call is
removed. Cleanup failure is reported distinctly, retaining the owned path for
recovery. The output directory must be trusted by the caller. POSIX builds
reject a symlink directory; native Windows reparse-point behavior is not
verified.

## Build and tests

```sh
make test-report
make test-report-csv
make test-report-alloc
make test
```

The unit suite uses C11 only. CSV and allocator/file-operation failure tests
use POSIX temporary-directory APIs, so they explicitly skip on native Windows.
The allocation-injection wrappers are compiled into test-specific objects and
are not part of `libcrawler.a`. See `testing.md` and
`../prompt history/2026-11-09-phase11-design-and-handoff.md` for actual
acceptance counts, clean-build results, sanitizer observations, and platform
limitations.

## How this prepares Phase 12

Phase 12 can call `report_row_matches` while it still has only a discovered
document URL and source-page URL, then carry one row per relationship through
download and organization. Once an outcome is known, it can fill status, digest,
size, path, and error fields and call `report_write_csv`. This phase provides
only those reusable building blocks; it does not perform that orchestration.
