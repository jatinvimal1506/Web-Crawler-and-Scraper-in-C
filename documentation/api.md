# Public API

## Safety contract retained from Phase 7

Both `http_download` and `crawler_run` accept timeout **1..3600 inclusive**;
unsupported values return INVALID_ARGUMENT before any network request.
All curl options are checked before perform: timeouts, follow flag, protocols,
five-hop cap, explicit TLS peer/host verification and response size limits.
Failed option setup returns HTTP_ERR_INIT; OOM remains OUT_OF_MEMORY.

Additive `http_download_scoped(url, timeout, seed_url, same_domain, response)`
borrows inputs, uses existing URL parsing/resolution and follows
301/302/303/307/308 in a bounded C loop. Each Location is resolved against the
actual response final_url and validated BEFORE requesting it. Both modes pin
seed effective port; strict mode also pins exact canonical hostname.
HTTPS->HTTP is blocked; same-port HTTP->HTTPS keeps TLS verification.
Timeout is per hop, not one shared deadline for the whole redirect chain.

HTTP_ERR_SCOPE is appended without renumbering earlier HTTP statuses; maps to
CRAWLER_PARSE_ERROR at root, or failed child/eventual PARTIAL. Metadata describes
the last fetched hop, never an unrequested blocked URL. Malformed/missing/
unsupported Location uses HTTP_ERR_HTTP_STATUS; excessive hops/loops use
HTTP_ERR_TOO_MANY_REDIRECTS; partial transport uses HTTP_ERR_NETWORK.
Scoped OOM empties all owned response fields; always use http_response_free.
Standalone automatic redirects remain supported. This is NOT IP/DNS-rebinding,
robots or authorization enforcement. Existing config/stat layouts are unchanged.

The Phase 6 sections below describe retained APIs. Their historical post-fetch
scope policy is superseded by
`../prompt history/2026-11-09-phase7-decisions-and-fixes.md`.

## Part 1 crawler API

`include/project.h` declares `crawler_run(config, stats)`. It zeroes available
caller-owned statistics and validates config/start URL before doing network
work. Phase 6 performs real bounded BFS. Additive
`crawler_run_with_link_observer(config, stats, observer, context)` calls a C
callback for each resolved, normalized, in-scope href. URL strings are borrowed
only for the callback duration. Set `*crawl_link = 0` to inspect/record the
link without placing it in the BFS queue; `1` preserves normal crawling. Return
`CRAWLER_OK` to continue, or a documented fatal status to stop. Ordinary
`crawler_run` passes no observer and preserves its Phase 6 behavior. Existing config/stat layouts and enum
values are preserved; PARTIAL, LIMIT_REACHED and INTERNAL_ERROR are appended.
`crawler_status_string` returns a static string. No allocated data escapes.

The central stats/error/scope contract is in
`../prompt history/2026-11-08-phase6-design-decisions.md`.
HTTP error bodies are not counted. Known final aliases are tracked even when
the download/parsing fails. Independent queue/visited APIs remain unchanged.
The original HTML extractor is unchanged in behavior; the additive
`html_extract_links_limited` provides bounded temporary output for the engine.

## Phase 8 document discovery API

`include/document.h` declares:

- `document_type_from_url(url, &type)` parses an absolute HTTP(S) URL and
  classifies only the path's final `.pdf`, `.docx` or `.zip` suffix,
  case-insensitively. Query and fragment are ignored. A valid unmatched suffix
  returns `DOCUMENT_OK` with `DOCUMENT_TYPE_UNKNOWN`; it never probes content.
- `document_discover(config, &results, &stats)` runs the existing bounded BFS
  with the additive link observer. It retains normalized document URLs and
  distinct normalized source-page/depth pairs. A recognized document link is
  observed but filtered from the page queue, so the crawler does not fetch it.
- `document_results_init` initializes an empty/zeroed object without allocating.
  `document_results_free` releases all owned document strings, source arrays,
  source-page strings and the top-level array; it is NULL-safe and repeatable.
- `document_results_count`, `document_results_source_count` and
  `document_results_at` provide read-only access. The returned record pointer
  is borrowed and valid only until the results are freed or reused.
- `document_type_name` and `document_status_message` return static strings;
  callers must not free them.

Initialize `DocumentResults` as `{0}` or call its init function on a zeroed
object. Do not shallow-copy it or modify its public arrays. All result strings
are owned by the result object and no result allocation escapes it. On fatal
OOM/overflow the operation releases partial results and returns an empty
object; on a non-root page failure it returns `DOCUMENT_PARTIAL` while keeping
successful discoveries; on crawl/discovery budget exhaustion it returns
`DOCUMENT_LIMIT_REACHED` with bounded results retained. The result limit is
1000 unique document/source pairs. Linear URL/pair searches are intentionally
simple (O(n) lookup, O(n²) worst-case accumulation) at this bounded scale.

`document_discover` does not download files, sniff content, hash, save or report.
An unrecognized extension stays an ordinary HTTP(S) page candidate. Phase 9
adds a separate one-document downloader; it does not bulk-download discovery
results or modify their provenance.

## Phase 9 one-document download API

`include/download.h` declares:

- `download_result_init(&result)` accepts an empty/zeroed result and allocates
  nothing. Start with `{0}`.
- `download_document(&config, &result)` validates and downloads exactly one
  URL recognized by `document_type_from_url`. Input URL, seed URL, output
  directory and filename are borrowed for the duration of the call.
- `download_result_free(&result)` frees the owned final-URL and output-path
  strings, resets the result and is safe to call repeatedly or with NULL.
- `download_status_message(status)` and `document_type_name(type)` return
  static strings; do not free them.
- After the program has finished all HTTP work, call the existing
  `http_cleanup()` once to release libcurl's process-wide state.

`DownloadConfig` contains the document URL, seed URL, `same_domain` flag,
timeout, existing output directory and a safe basename. The supported timeout
is 1–3600 seconds for each individual request/redirect hop. Both scope modes
pin the seed's effective port; when `same_domain` is 1 the exact seed hostname
must also match. HTTPS-to-HTTP redirects are rejected. Redirect targets are
resolved and checked before the next request; only HTTP(S) and at most five
redirects are allowed. TLS certificate and hostname verification remain
enabled. This hostname policy is not an IP/DNS firewall or robots/access
policy.

The write callback receives a pointer plus a byte count, checks multiplication
and size limits, then writes those bytes through `FILE *` with `fwrite`. It
never calls `strlen` on a response body, so embedded zero and high-valued bytes
are preserved. Memory use does not grow with the full document size. The cap
is 20 MiB inclusive; callback checks also protect chunked responses without
`Content-Length`.

Exactly one appropriate MIME media type is required: PDF, DOCX, ZIP or
`application/x-zip-compressed` for ZIP. MIME parameters are accepted. A single
`application/octet-stream` value is an explicit fallback for a recognized URL
extension. Missing/duplicate MIME, HTML, other types, 206, non-success HTTP
status, empty bodies, truncation, timeout, short writes and oversize bodies
fail. A MIME label does not prove that bytes form a valid PDF, DOCX or ZIP.

The caller chooses the destination basename. It must be at most 128 bytes,
use only ASCII letters/digits/dot/dash/underscore and end in the matching
extension. Path separators, drive/path forms, `.`/`..`, empty names, reserved
Windows device names, and trailing spaces/dots are rejected. The directory
must already exist. Exclusive create prevents overwriting a pre-existing file.
The output is visible during streaming; it is not atomically published. On
failure only a file created by this call is removed. If removal fails, the
distinct `DOWNLOAD_ERROR_CLEANUP` is returned and the owned `file_path`
identifies the leftover.

On success, `DownloadResult.final_url` and `file_path` are owned strings,
`bytes_written` is the complete saved size and `http_status` is the final
response code. On failure, known URL/status/byte metadata is retained when
available; ordinary failure clears `file_path` after removing the partial
file. The result owns no file handle; it must still be freed by the caller.
Status codes distinguish invalid input, unsupported type, allocation/init,
timeout, network, redirect, scope, HTTP status, MIME, size, empty/incomplete,
existing-file, write/file, cleanup and overflow errors.

The C CLI is `build/download-document`:

```sh
./build/download-document DOCUMENT_URL SEED_URL OUTPUT_DIRECTORY \
  safe-name.pdf 1 [TIMEOUT_SECONDS]
```

It handles one document only. Phase 10 separately hashes and organizes an
already-completed local file; this downloader CLI does not invoke that API.
Phase 12 composes discovery, selection, downloads, organization and reporting;
see the pipeline API below.

## Phase 10 SHA-256 and file organization API

`include/sha256.h` declares:

- `sha256_status_message(status)` returns a static string; do not free it.
- `sha256_file(path, hex_out, bytes_read)` hashes a borrowed file path using
  plain C11. `hex_out` must have room for 64 lowercase hex digits and a NUL;
  `bytes_read` is optional. Outputs are cleared on failure. Empty input is
  valid to the hash helper, but not to the document organizer.

Hashing is incremental, with a fixed 16 KiB buffer and a 20 MiB inclusive
limit. No OpenSSL or other hashing library is required. Hashing takes O(file
bytes) time and constant buffer memory.

`include/organize.h` declares:

- `organizer_status_message(status)` returns a static string.
- `organize_result_init(result)` initializes a zeroed result without
  allocating. Use `{0}` before the first call.
- `organize_result_free(result)` frees `stored_path`, resets the structure,
  and is safe for NULL or repeated cleanup.
- `organizer_prepare_directories(root)` requires an existing trusted output
  root, then creates/verifies its immediate `pdf`, `docx` and `zip` folders.
- `organize_file(input, type, root, result)` borrows the input and root paths
  for the duration of the call. Input must be a nonempty regular file no larger
  than 20 MiB. The caller must initialize `result` empty and must not shallow
  copy it after it owns memory.

A new unique destination is exclusively created as
`root/type/sha256.ext`. The organizer searches all three type folders and
byte-compares matching digest names, so identical bytes are stored only once
even if the caller later supplies a different type. The returned
`stored_type`/path describe the first actual stored copy. A hash-name collision
whose bytes differ returns a conflict; it never overwrites the existing path.

The result owns `stored_path` on `ORGANIZER_NEW_FILE`,
`ORGANIZER_DUPLICATE`, and `ORGANIZER_ERROR_CLEANUP`; release it with
`organize_result_free`. On ordinary failure the result owns no path. The caller
retains and owns the input file, which is never removed or changed. Copying and
verification use bounded buffers; after writing, the destination is rehashed
and its exact byte count checked. Only a new partial file from this call may be
removed. If that cleanup fails, the distinct cleanup status preserves its path
for diagnosis.

POSIX builds reject symlink input/root/type directories. Windows junction or
reparse-point detection is not guaranteed by the current CRT implementation.
The module handles one local file only; it does not crawl directories or invoke
the standalone Phase 11 report API. Phase 12 composes it with the other modules.
See [`phase10.md`](phase10.md) for beginner-oriented examples and test descriptions.

## Phase 11 metadata filter and CSV API

`include/report.h` provides `ReportFilter`, `ReportRow`, `ReportResult` and
these functions:

- `report_status_message(status)` and `report_row_status_name(status)` return
  static strings; do not free them.
- `report_result_init(result)` initializes fresh or previously freed storage
  without allocating. Do not call it while the result owns a path; free first.
- `report_result_free(result)` releases the result-owned `output_path`, resets
  the structure, and is safe for NULL/repeated cleanup.
- `report_row_matches(row, filter, matches)` checks only the borrowed document
  URL and source-page URL. `matches` is set to zero before validation. It does
  no I/O or allocation, so Phase 12 calls it before download outcomes are known.
- `report_write_csv(directory, basename, rows, count, filter, result)` checks
  the full row values and writes a new report file. Inputs and all row/keyword
  strings are borrowed for the call.

A filter accepts zero through eight keyword strings. Each must be one through
64 printable ASCII bytes; control and non-ASCII bytes are rejected. Matching is
ASCII case-insensitive literal substring matching. Any keyword found in either
URL selects the row; an empty filter includes every row. No document contents
are opened, searched, or extracted.

A `ReportRow` is exactly one document/source-page relationship. Each pointer is
borrowed, and unavailable hash, stored path, and error text are represented by
an empty string. A new or duplicate stored-file outcome must carry a 64-digit
lowercase SHA-256, nonempty path, and nonzero byte size. Failed/skipped rows
must not claim a stored hash or path. The writer preserves input order, so
separate source relationships for the same document remain separate rows.

The CSV header is exactly:

```text
document_url,document_type,source_page_url,source_depth,status,sha256,size_bytes,stored_path,error
```

Text values are always double-quoted; embedded quotes are doubled; commas and
embedded CR/LF stay inside the quoted field. Record separators are CRLF.
Potential spreadsheet formulas are protected by prefixing an apostrophe to a
text field whose first non-control/non-whitespace byte is `=`, `+`, `-` or `@`.
The original borrowed strings are not changed. Numeric depth and byte counts
are decimal and unquoted.

Text fields are limited to 4,096 bytes and the writer accepts at most 1,000
input rows, including rows later filtered out. The output directory must
already exist. The safe ASCII basename must end in `.csv` and be at most 128
bytes; traversal, separators and common Windows device names are rejected. The
file is exclusively created and never overwrites an existing report. An empty
match set writes only the header.

`ReportResult.rows_written` counts complete data rows (not the header), and
`rows_excluded` counts input rows not selected by the filter. On an I/O error,
the writer removes only the new file created by this call; if removal fails,
`REPORT_ERROR_CLEANUP` keeps the owned path for diagnosis. On success the result
also owns its path. Release either with `report_result_free`. CSV filesystem and
fault-injection tests use POSIX temporary directories; native Windows execution
and Windows junction behavior are not verified here.

## Phase 12 bounded pipeline API

`include/pipeline.h` declares the orchestration API:

- `pipeline_result_init(result)` initializes fresh or already-freed storage
  without allocating. Begin with `{0}`. Do not reinitialize a live result.
- `pipeline_run(config, result)` performs bounded document discovery,
  metadata filtering, selected downloads, file organization/deduplication, and
  CSV reporting. Configuration strings and keyword strings are borrowed for the
  duration of the call.
- `pipeline_result_free(result)` releases `report_path` and any
  `orphaned_paths`, clears the result, and is safe for NULL/repeated cleanup.
- `pipeline_status_message(status)` returns a static string; do not free it.

`PipelineConfig` requires an absolute HTTP(S) seed URL, an existing trusted
output directory, and a safe `.csv` basename. It carries the unchanged crawler
depth and exact-host/seed-port scope policy, a 1–3600 second per-request timeout,
and zero through eight borrowed keywords. The CLI uses a fixed three-second
timeout and accepts depths 0–3.

The pipeline retains first-discovery order. It filters source/document
relationships by literal, case-insensitive metadata URL matching before any
document download. It downloads a selected unique document URL at most once,
up to ten unique URLs, then passes the temporary file to the existing
content-based organizer. Matching source relationships remain separate CSV
rows. `successful_document_bytes_downloaded` includes successful downloads
whose bytes were already stored; `bytes_newly_stored` counts only newly created
organized files. `csv_rows_written` counts data rows, not the header.

The other `PipelineStats` fields count successful/failed HTML pages, emitted
hrefs, unique document URLs, document/source relationships, selected URLs and
relationships, new files, byte-identical duplicates, failed document
downloads, and documents skipped after the ten-document cap. Counts are checked
for overflow. The HTML crawl also has its existing 100-page, 1000-candidate and
1000-key bounds. `PIPELINE_LIMIT_REACHED` takes precedence when a configured
budget is reached; otherwise recoverable crawl or document failures produce
`PIPELINE_PARTIAL`. Fatal allocation, report, filesystem, crawl-invariant, or
overflow failures return explicit error statuses. A partial/limit result is not
a claim of exhaustive coverage.

`PipelineResult` owns no caller configuration strings. It owns `report_path`
when a report was created (including a cleanup-failure case) and owns any
`orphaned_paths` that could not be removed; inspect and remove those manually
when appropriate. No other allocation escapes `pipeline_run`. Output-root and
report collisions are checked before the first request. Download staging names
are created exclusively, and failed operations remove only files created by
that run.

The C CLI is `build/deepdownload`:

```sh
./build/deepdownload START_URL OUTPUT_ROOT REPORT.csv [DEPTH 0..3] \
  [SAME_DOMAIN 0|1] [--keyword WORD]...
```

Exit code 0 means `PIPELINE_OK`, 1 means partial results or a configured limit,
and 2 means invalid input or a fatal error. See
[`phase12.md`](phase12.md) for examples, full beginner explanations, resource
ceilings, and deterministic test coverage. The pipeline does not inspect
document contents, download beyond the existing ten-document cap, or implement
any later phase.

## Part 2 URL API

`include/url.h` declares `UrlStatus` and the following functions:

- `url_parse(input, result)` parses an absolute HTTP(S) URL into scheme, host, optional port, path, query, and fragment.
- `url_free(url)` releases component strings and clears the structure; accepts NULL.
- `url_normalize(input, output)` creates an allocated canonical URL.
- `url_resolve(base, relative, output)` resolves the supported common URL reference forms and normalizes the result.
- `url_status_message(status)` returns a static message; do not free it.

URL functions return one of:

| Status | Meaning |
|---|---|
| `URL_OK` | Operation succeeded |
| `URL_ERROR_INVALID_ARGUMENT` | Required pointer missing or parse output is already populated |
| `URL_ERROR_INVALID_FORMAT` | Malformed URL/reference syntax |
| `URL_ERROR_UNSUPPORTED_SCHEME` | Scheme is not HTTP or HTTPS |
| `URL_ERROR_INVALID_HOST` | Missing or invalid ASCII DNS hostname |
| `URL_ERROR_INVALID_PORT` | Port is empty, nonnumeric, zero, or greater than 65535 |
| `URL_ERROR_TOO_LONG` | URL exceeds 2048 bytes |
| `URL_ERROR_OUT_OF_MEMORY` | Allocation failed |

Initialize `ParsedUrl` as `{0}` before parsing. `url_parse` only writes the result after all components have been allocated, and rejects a structure containing non-NULL fields to avoid overwriting caller-owned pointers. The caller frees successful results with `url_free`. `url_normalize` and `url_resolve` set a non-NULL output pointer to NULL before work; successful strings belong to the caller and must be released with `free`.

Normalization lowercases the scheme and host, removes HTTP port 80 and HTTPS port 443 (including leading-zero spellings), ensures an absolute path, removes dot segments, preserves repeated and trailing slash separators, retains query bytes, and removes fragments. Valid percent escapes are preserved byte-for-byte; incomplete or non-hex escapes are rejected. Resolution supports HTTP(S) absolute references, protocol-relative, root-relative, directory-relative, query-only, fragment-only, and empty references. Fragment-only and empty references retain the base query; fragments are removed from crawl identity.

The implementation supports ASCII DNS names only. IPv6 literals, internationalized names, and exhaustive RFC 3986 behavior are outside scope.

## Part 3 HTTP API

`include/http.h` declares `http_download(url, timeout_seconds, response)`, which uses the URL parser to validate an HTTP(S) URL and downloads one HTML response through libcurl. It returns `HTTP_OK` only for a successful 2xx response with HTML content. On success, `HttpResponse` contains the allocated body and its byte size, HTTP status code, final URL, and content type. On failure it returns a specific `HttpStatus`; the response may contain the status code and a detail string when available.

Initialize `HttpResponse` as `{0}` before use. Release response-owned strings with `http_response_free`, and call `http_cleanup` when the process is done with libcurl. HTTP status failures (such as 404) are distinct from transport failures (such as DNS or connection errors). The downloader does not extract HTML or crawl links.
