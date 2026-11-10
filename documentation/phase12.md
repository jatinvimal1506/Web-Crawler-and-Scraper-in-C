# Phase 12: bounded end-to-end pipeline

Phase 12 connects the existing C modules into one controlled workflow:

1. Validate a seed URL, crawl depth, scope policy, output directory, report
   name, timeout and optional metadata keywords.
2. Use the existing breadth-first crawler and document observer to collect
   normalized PDF, DOCX and ZIP URL/source-page relationships.
3. Select relationships by case-insensitive keyword matches in the document
   URL or its source-page URL. This is metadata matching; document contents
   are never opened for search.
4. Download each selected unique document URL at most once, in first-discovery
   order. At most ten unique URLs are attempted.
5. Pass the completed temporary file to the existing SHA-256 organizer. New
   bytes are stored once; an existing identical file is reported as a
   duplicate.
6. Write a safe CSV row for each selected source/document relationship. Multiple
   source pages for one document remain multiple rows.

This is a real integration: `pipeline_run` calls the existing APIs and passes
their results between modules. It is not a sequence of independent demos.

## Public functions and C pointer ownership

`include/pipeline.h` is the public interface. The function declarations are
plain C11 and do not change `CrawlerConfig`, `CrawlerStats`, or any earlier
module API.

- `pipeline_result_init(PipelineResult *result)` sets fresh or already-freed
  result storage to its empty state. It allocates no memory. Start with
  `PipelineResult result = {0};`; do not initialize a result while it still
  owns paths.
- `pipeline_run(const PipelineConfig *config, PipelineResult *result)` runs the
  bounded workflow. A pointer marked `const` is read-only through that
  pointer. Configuration URLs, output paths, CSV basename, keyword array and
  keyword strings are borrowed: the function reads them but does not free or
  retain them.
- `pipeline_status_message(PipelineStatus status)` returns a static message.
  It is not dynamically allocated and must not be freed.
- `pipeline_result_free(PipelineResult *result)` releases the result's owned
  paths and resets its fields. It accepts `NULL` and can be called repeatedly.

`PipelineResult.report_path` is an owned `char *` when a report exists. The
result may also own up to two `orphaned_paths` if cleanup itself failed; those
files require manual attention. `pipeline_result_free` frees the path strings,
not the files they name. No other allocation escapes `pipeline_run`.
Do not copy a populated result with a plain structure assignment: that would
make two structures appear to own the same pointers.

## Configuration and limits

`PipelineConfig` requires an absolute HTTP(S) seed URL, an existing trusted
output directory, a safe `.csv` basename, and a supported 1–3600 second
per-request timeout. It carries the crawler's `max_depth` and `same_domain`
settings, plus zero to eight borrowed ASCII keywords. The CLI fixes timeout at
three seconds and accepts depth 0–3.

The existing crawler limits remain in force: at most 100 attempted HTML pages,
1000 candidate hrefs, and 1000 scheduled/alias URL keys. Each document download
uses the existing 20 MiB byte ceiling and redirect/scope rules. Phase 12 adds a
limit of ten selected unique document URLs per run. The CSV module retains its
1000-row and field-size limits.

The report path is checked before any network request, so a pre-existing report
is not overwritten. The output root is not created automatically. Downloaded
bytes first go to uniquely named files in an exclusive staging directory.
Ordinary failures remove only files created by the current run. If that cleanup
fails, the remaining path is returned in `orphaned_paths`.

## Status and statistics

- `PIPELINE_OK`: the bounded work completed without a recoverable page or
  document failure and without reaching a configured limit. It does not mean
  the whole website was exhaustively crawled.
- `PIPELINE_PARTIAL`: document discovery had recoverable page failures or one
  or more selected document operations failed; available rows are still
  reported.
- `PIPELINE_LIMIT_REACHED`: a crawl/discovery budget or the ten-document
  download ceiling was reached. This status takes precedence over `PARTIAL`;
  counters and per-row outcomes preserve the other information.
- `PIPELINE_ERROR_*`: invalid input, allocation, filesystem, report, crawl, or
  internal/overflow failure. Fatal failures stop the pipeline and return the
  most useful statistics/results already collected.

`PipelineStats` fields mean:

| Field | Meaning |
|---|---|
| `html_pages_visited` / `html_pages_failed` | Successfully parsed HTML pages / attempted pages that failed. |
| `html_links_discovered` | Hrefs emitted by the extractor, including repeated occurrences. |
| `unique_document_urls_discovered` | Distinct normalized supported document URLs. |
| `document_source_relationships_discovered` | Distinct normalized document/source-page pairs. |
| `unique_document_urls_selected` | Distinct document URLs selected by metadata filtering. |
| `source_document_relationships_selected` | Selected source/document rows. |
| `newly_stored_documents` / `duplicate_content_documents` | Successful documents newly stored / identical bytes already present. |
| `failed_document_downloads` | Selected unique document downloads that failed. |
| `documents_skipped_limit` | Selected unique URLs beyond the ten-attempt limit. |
| `successful_document_bytes_downloaded` | Bytes from successful downloads, including content that was already stored. |
| `bytes_newly_stored` | Bytes copied into new organized files only. |
| `csv_rows_written` | CSV data rows; the header is not counted. |

Counters are overflow-checked. On successful report creation, each selected
source/document relationship has one CSV row, including failure or skipped
outcomes. `PipelineResult.detail` provides a concise explanation when available.

## Command-line use

Build and run:

```sh
make
mkdir -p output
./build/deepdownload START_URL output report.csv [DEPTH 0..3] \
  [SAME_DOMAIN 0|1] [--keyword WORD]...
```

`same_domain=1` keeps the crawler to the seed's exact hostname and effective
port under the existing Phase 7 redirect policy. `same_domain=0` allows other
hostnames but still pins the seed port. The HTTP downloader checks redirect
scope before each redirected request. This is not an IP/DNS firewall, robots
implementation, login/CAPTCHA bypass, or permission check; use only controlled
or otherwise permitted websites.

The CLI prints all statistics, status, report path and any orphaned paths. It
returns exit code 0 for `PIPELINE_OK`, 1 for `PARTIAL` or `LIMIT_REACHED`, and 2
for invalid input or fatal error. Its default depth is 1 and its timeout is
three seconds.

## Tests and implementation boundary

`make test-pipeline` checks API validation and status/stat initialization.
`make test-pipeline-integration` starts a deterministic C-only local fixture and
checks the actual pipeline and CLI, including FIFO page order/depth, filtering,
redirect-relative provenance, duplicate content, existing files, crawl and
download failures, resource limits, output collisions, timeout, CSV results,
and no-network preflight behavior. `make test-pipeline-alloc` injects failures
into the pipeline's own allocations, staging-file removal, and report status
handling, then checks cleanup and a successful retry. Existing report,
downloader, organizer, crawler and URL tests remain part of `make test`.

The local integration and allocation tests use POSIX process/socket/filesystem
facilities and print `SKIP` on native Windows. A successful Linux Makefile build
does not verify the Windows runtime. Exact commands, counts, warnings,
sanitizer observations and Git receipt belong in
[`../prompt history/2026-11-09-phase12-design-and-handoff.md`](../prompt%20history/2026-11-09-phase12-design-and-handoff.md).

Phase 12 does not add document-content searching, bulk downloads beyond the
limit, a GUI, AI, browser automation, or any later phase.
