# DEEPDOWNLOAD-C — MASTER IMPLEMENTATION SPECIFICATION
## Phases 10, 11 and 12 — Complete the C11 Project

**Authorization:** Implement all three remaining phases sequentially, subject to mandatory acceptance gates. This is one work order, not permission to implement other features.

**Project:** DeepDownload-C — Web Crawler, Public Document Discovery and Downloading in C.

**Primary objective:** Finish a university-level modular C11 program that discovers public document links, safely downloads selected PDF/DOCX/ZIP files, organizes and deduplicates their contents, filters results by metadata keywords, and produces a CSV report containing source-page provenance.

**IMPORTANT: This document is the authoritative technical reference. Follow the specified designs instead of inventing alternative architectures.**

---

# PART A — EXECUTION RULES AND PROJECT BASELINE

## A1. Strict execution sequence

Complete work in this exact order:

1. **Phase 10 — File organization and SHA-256 content deduplication.**
2. **Phase 11 — Metadata keyword filtering and CSV reporting.**
3. **Phase 12 — Complete end-to-end integration, final tests and CLI.**

Do not implement features belonging to a later phase while working on an earlier phase.

### Mandatory reread checkpoint

At the end of EACH phase:

1. Finish coding that phase.
2. Run its unit, integration, error-handling and allocation tests.
3. Rerun all previous regression tests.
4. Review actual code for errors, memory leaks and compatibility.
5. Update documentation and the phase-specific acceptance report.
6. Commit the completed phase to Git.
7. Perform a normal, non-force GitHub push if the remote is safe.
8. Verify the remote commit SHA.
9. **REOPEN AND REREAD THIS COMPLETE MASTER PROMPT before beginning the next phase.**
10. Check the next phase's dependencies, API contracts, resource limits and prohibitions again.
11. Only proceed if the current phase passes its acceptance requirements.

If a phase is incomplete, a mandatory deterministic test fails, GitHub has diverged, or a safety requirement cannot be implemented, **STOP at that phase**. Report the blocker instead of beginning the next one.

Fix ordinary in-scope bugs before attempting the gate again. Do not conceal failures or mark a phase complete based on intentions.

Do not continue beyond Phase 12.

## A2. Mode, spending and scope restrictions

The user has explicitly forbidden unauthorized Replit Power Mode and paid-feature usage.

- Do not select Power Mode.
- Do not select expert or higher-compute Agent modes.
- Do not change account settings or Agent configuration.
- Do not authorize additional spending or upgrades.
- Do not deploy, publish or start hosted services.
- Do not create paid databases or cloud storage.
- If execution requires a mode upgrade, **STOP and request the user's explicit approval**.
- If a popup requests approval for a paid action or a mode change, do not approve it automatically.

The user controls Replit execution and billing settings manually. These written instructions are not a technical spending lock.

All executable application code, library modules, demos, and test fixtures must use **C11 only**.

Permitted: C11, standard C libraries, existing libcurl, GCC, GNU Make, Markdown documentation, and small platform-specific C filesystem compatibility sections.

Prohibited: Python, JavaScript, TypeScript, React, C++, Rust, AI agents, browser automation, web dashboards, deployment, concurrency, and unrelated new dependencies.

Keep the implementation understandable to first-year C-programming students. Favor clearly named functions, simple data structures, explicit ownership and modular files.

## A3. Verified starting repository

Repository:

https://github.com/jatinvimal1506/Web-Crawler-and-Scraper-in-C

Branch: `main`

Verified Phase 9 implementation:

`6f4a3028b4cdd953efe38606938defca5003183a`

Latest Phase 9 handoff receipt:

`b42ccaff650cc94a7db5ccf1f5c1e67d7ac1c8b6`

Before editing, check actual local HEAD, remote HEAD, working-tree cleanliness and the C-only project directory.

Do not overwrite unexpected work or operate on the unrelated Replit React/TypeScript scaffold.

## A4. Existing codebase — preserve these contracts

The existing C11 static library already contains:

| Existing module | Current responsibility |
|---|---|
| `url.c` | Parse, normalize and resolve HTTP(S) URLs |
| `http.c`, `http_scoped.c` | HTML transport, timeout and scoped redirect handling |
| `html.c` | Extract and resolve HTML hyperlinks |
| `queue.c`, `visited.c` | FIFO BFS and normalized URL tracking |
| `project.c` | Bounded BFS crawler |
| `document.c` | Document URL classification and source-page provenance |
| `download.c` | Safe one-document binary streaming |

**Phase 8 APIs:**

`document_type_from_url()` identifies PDF, DOCX and ZIP using URL path suffixes.

`document_discover()` produces `DocumentResults`.

`DocumentResults` owns an array of `DocumentRecord` objects. Each record contains an owned normalized document URL, a `DocumentType`, and an array of `DocumentSource` entries with normalized source-page URLs and crawl depths.

These are freed through `document_results_free()`.

Discovery returns up to 1,000 distinct document/source pairs. The same document may have multiple sources.

**Phase 9 APIs:**

`download_document(const DownloadConfig *, DownloadResult *)` downloads exactly one recognized document.

`DownloadConfig` contains:

- `document_url`
- `seed_url`
- `same_domain`
- `timeout_seconds`
- `output_directory`
- `filename`

`DownloadResult` contains:

- Status
- Document type
- HTTP status
- Bytes written
- Owned `final_url`
- Owned `file_path`
- Diagnostic text

Release owned fields through `download_result_free()`.

Phase 9 uses exclusive file creation, does not overwrite existing files, preserves binary bytes, enforces 20 MiB per document, validates MIME/HTTP responses, and checks redirects before requesting each new destination.

**Do not change these APIs unnecessarily.**

The new phases must use the existing modules, not reproduce the crawler, HTML parser or binary downloader.

## A5. Shared resource and security contracts

Preserve:

- Maximum 100 attempted HTML pages.
- Maximum 1,000 discovered HTML href candidates.
- Maximum 1,000 scheduled/redirect-alias URL keys.
- Maximum 1,000 document/source pairs.
- Maximum 5 MiB per HTML response.
- Maximum 20 MiB per binary document.
- Supported timeout range 1–3600 seconds per HTTP request.
- Maximum five redirects per download.
- HTTP(S)-only requests and redirects.
- Exact seed-host restrictions when same-domain mode is enabled.
- Seed effective-port restrictions in both domain modes.
- HTTPS downgrade rejection.
- Normal TLS certificate/hostname verification.

These limitations are not a complete DNS/IP firewall. Do not claim otherwise.

Use controlled, permissioned websites only. Do not bypass authentication, CAPTCHAs, robots restrictions or access controls.

Never execute, extract, unzip or interpret downloaded files.

---

# PART B — PHASE 10: FILE ORGANIZATION AND SHA-256 DEDUPLICATION

## B1. Exact goal

Given a successfully downloaded Phase 9 document file, calculate its SHA-256 digest, place one unique copy in a type-specific directory and detect whether identical file contents were already saved.

This phase concerns **local file contents**, not URL deduplication.

Two different URLs may download identical bytes. Phase 10 must recognize them as duplicates even when their URLs or filenames differ.

No keyword filtering, CSV reporting or bulk crawl/download orchestration in Phase 10.

## B2. Required source files

Create:

- `include/sha256.h`
- `src/sha256.c`
- `include/organize.h`
- `src/organize.c`

Create tests:

- `tests/test_sha256.c`
- `tests/test_organize.c`
- `tests/test_organize_integration.c`
- `tests/test_organize_alloc.c`, if needed for meaningful fault injection.

Add these modules to the existing static library and Makefile.

## B3. SHA-256 implementation contract

Implement SHA-256 using plain C11 and `<stdint.h>`. Do not add OpenSSL or another dependency solely for hashing.

Use the standard SHA-256 algorithm:

1. Initialize the eight standard 32-bit SHA-256 state words.
2. Read the file incrementally in bounded chunks.
3. Divide the byte stream into 512-bit blocks.
4. Build the 64-word message schedule using SHA-256's defined bit rotations and shifts.
5. Run the standard 64 compression rounds with the 64 fixed round constants.
6. Apply standard `0x80` padding and the 64-bit big-endian message bit length.
7. Convert the final eight state words to exactly 64 lowercase hexadecimal characters.
8. Terminate the digest string with NUL.

Do not write your own alternative hash algorithm.

Suggested public API:

`Sha256Status sha256_file(const char *path, char hex_out[65], size_t *bytes_read);`

Define explicit success, invalid-argument, file-read, overflow and size-limit statuses.

Requirements:

- Use a bounded buffer, such as 16 KiB.
- Process binary data without using `strlen()` on file contents.
- Check file-read errors and close errors.
- Ensure size accounting cannot overflow.
- Permit an empty file to be hashed by the SHA helper, although the organizer must reject empty documents.
- Clear output fields on failure.
- Keep the implementation correct on 32-bit and 64-bit targets.

### Required SHA-256 reference vectors

Verify at minimum:

- Empty input: `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855`
- `abc`: `ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad`
- One million ASCII `a` bytes: `cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0`

Also test binary input containing NUL and high-value bytes, with an independently verified expected hash.

A test that merely compares the hash function to itself is not sufficient.

## B4. Exact directory organization

The user supplies an existing trusted output-root directory.

Phase 10 creates or verifies only these immediate subdirectories:

```text
output-root/
    pdf/
    docx/
    zip/
```

Use lowercase directory names consistently.

Do not generate directory names from untrusted URLs or HTTP headers.

Implement an idempotent helper such as:

`OrganizerStatus organizer_prepare_directories(const char *output_root);`

Behavior:

- Reject NULL/empty paths.
- Require an existing output root.
- Create the three known directories when missing.
- Accept directories that already exist.
- Reject ordinary files occupying directory names.
- Do not follow an untrusted directory symlink or junction when detectable.
- Do not delete existing directories or user files.
- Check filesystem errors.

Use platform-specific C compatibility sections for POSIX `mkdir` and Windows `_mkdir` where needed.

Native Windows verification must be reported honestly.

## B5. Content-addressed filenames

Every newly stored unique document must receive a deterministic filename:

`<64-character-sha256>.<extension>`

Examples:

```text
pdf/ab12...64hex...ef.pdf
docx/ab12...64hex...ef.docx
zip/ab12...64hex...ef.zip
```

The extension comes from the recognized `DocumentType`, not from an untrusted original filename.

No title extraction, original-filename parsing or unsafe path construction is required.

This keeps the organization explainable:

- The folder represents the claimed document type.
- The filename represents the file's contents.
- Identical content produces the same digest.

## B6. Organizer API

Implement an API equivalent to:

`OrganizerStatus organize_file(const char *completed_file_path, DocumentType type, const char *output_root, OrganizeResult *result);`

Define `OrganizeResult` to include:

- `OrganizerStatus status`
- `int is_duplicate`
- `DocumentType stored_type`
- `char sha256_hex[65]`
- `size_t bytes`
- `char *stored_path` — owned by the result
- `char detail[256]`

Provide:

- `organize_result_init()`
- `organize_result_free()`
- `organize_status_message()`

Result objects begin as `{0}` and must not be shallow-copied after acquiring owned memory.

On success, `stored_path` identifies the existing or newly stored canonical file.

**The organizer must never remove the caller's input file.**

The caller remains responsible for deleting its staging file after confirming success. This is essential for Phase 12.

## B7. Exact duplicate-detection algorithm

Implement this algorithm:

1. Validate input pointers, type and output root.
2. Require the input to be an existing regular file.
3. Reject empty files and files exceeding 20 MiB.
4. Hash the input file using the Phase 10 SHA-256 implementation.
5. Determine the candidate canonical filenames for that digest in all three supported directories.
6. Examine candidate files in deterministic order: `pdf`, `docx`, then `zip`.
7. If a candidate exists, verify that it is a regular file.
8. Compare candidate bytes against the input file in bounded chunks.
9. If both files are identical, return `DUPLICATE` and the existing canonical path.
10. If a candidate uses the same hash-derived filename but contains different bytes, return a conflict/error. Never overwrite it.
11. If no matching candidate exists, copy the input into a new exclusive-created destination under the directory corresponding to its type.
12. Check every read, write and close.
13. Re-hash the newly written destination and verify both its digest and byte count.
14. Return `NEW_FILE` only after verification succeeds.
15. On failure, remove only a new incomplete destination created by this call.

Never overwrite an existing destination.

Use exclusive file creation (`O_CREAT | O_EXCL` or the appropriate Windows equivalent). Do not use an unchecked rename that can replace an existing file.

Keep one bounded copy buffer; do not load the entire document into RAM.

A SHA-256 filename collision or preexisting-file content mismatch is an error, not permission to replace the file.

### Cross-type duplicates

If the same bytes appear under two different claimed file types, store one physical copy only.

The first stored file remains the canonical copy. Subsequent duplicate results must identify its actual `stored_type` and path.

For a fresh output directory, discovery/store order determines which type is stored first. For an existing directory, use the deterministic search order above when checking candidates.

This is *content deduplication*, not proof that the file's claimed type is correct.

## B8. Required Phase 10 tests

Use temporary trusted directories and deterministic binary test files.

Test:

- Every SHA-256 reference vector.
- Files containing embedded NUL bytes.
- New PDF, DOCX and ZIP organization.
- Correct folder selection and 64-character digest names.
- Same URL/file bytes submitted twice.
- Different source filenames with identical bytes.
- Same bytes under different document types.
- Different bytes of the same size.
- Preexisting canonical destination with correct bytes.
- Preexisting canonical destination with corrupted/different bytes.
- Input file remains present and unchanged on all outcomes.
- Existing destination is never overwritten.
- Read/write/close/allocation failure cleanup.
- Successful retry after a failure.
- Empty and oversized input rejection.
- Missing output directory and directory-name collisions.
- Repeated result cleanup.
- Any platform symlink checks actually supported.

Do not claim all filesystem attacks are prevented on platforms not tested.

## B9. Phase 10 acceptance gate

Before advancing, require:

- SHA-256 reference vectors pass.
- Byte-for-byte duplicate checks pass.
- File organization and no-overwrite behavior pass.
- Incomplete outputs are cleaned up or honestly reported.
- Existing Phases 1–9 pass unchanged.
- Strict C11 build passes.
- Relevant sanitizer and allocator tests pass within verified tool limitations.
- Documentation is updated.
- A phase-specific handoff is committed and pushed to GitHub.

Create:

- `documentation/phase10.md`
- `prompt history/2026-11-09-phase10-design-and-handoff.md`

The handoff must include changed functions, memory ownership, tests, command exits, GitHub SHA, limitations and a `PASS/FAIL/NOT VERIFIED` checklist.

### STOP AND REREAD CHECKPOINT 10

**Do not begin Phase 11 until all mandatory Phase 10 checks pass.**

After successful Phase 10 commit and verified push, reopen `prompt history/2026-11-09-phases10-12-master-instruction.md`, reread all shared rules and Phase 11 completely, then proceed.

---

# PART C — PHASE 11: KEYWORD FILTERING AND CSV REPORTING

## C1. Exact goal

Build an optional keyword filter and a safe CSV report writer.

Filtering must operate on metadata already discovered by Phase 8:

- Normalized document URL.
- Normalized source-page URL.

It must **not** inspect PDF, DOCX or ZIP contents.

Do not implement a PDF parser, DOCX text extractor, ZIP extractor, OCR, AI categorization, semantic search or full-text indexing.

Phase 11 builds reusable filtering/reporting functions. Phase 12 will integrate them into the full workflow.

## C2. Required files

Create:

- `include/report.h`
- `src/report.c`
- `tests/test_report.c`
- `tests/test_report_csv.c`
- `tests/test_report_alloc.c`, if meaningful for file/allocation failures.

Keep executable code and tests in C11.

## C3. Exact keyword matching contract

The filter accepts zero to eight keywords, each one to 64 ASCII characters.

Matching is:

- Case-insensitive for ASCII letters.
- Literal substring matching.
- No regular expressions.
- No wildcard expansion.
- No semantic interpretation.
- No stemming.
- No document-content extraction.

A row matches if **any keyword** occurs in either its document URL or its source-page URL.

Example:

Keyword: `exam`

Match:

`https://university.edu/resources/exam-notes.pdf`

Also match:

Source page: `https://university.edu/examinations/resources`

Do not require both fields to match.

An empty keyword list means **include everything**.

Reject malformed filter arrays, NULL keyword entries, empty keywords, overlong keywords and unsupported control characters with a clear status.

Keywords are borrowed; the filter must not modify them.

## C4. Report row structure

Define a `ReportRow` containing borrowed fields:

- `const char *document_url`
- `DocumentType document_type`
- `const char *source_page_url`
- `size_t source_depth`
- `ReportRowStatus outcome`
- `const char *sha256_hex`
- `size_t size_bytes`
- `const char *stored_path`
- `const char *error_detail`

Define outcome values corresponding to:

- `DOWNLOADED_NEW`
- `DUPLICATE`
- `DOWNLOAD_FAILED`
- `SKIPPED_LIMIT`

The row is one `(document URL, source-page URL)` relationship.

Thus, a document discovered on three pages produces three CSV rows, but Phase 12 will download that unique document URL only once.

Do not discard source provenance when organizing files.

Use an empty string for unavailable hash/path/error fields, rather than inventing values.

Define a `ReportFilter` containing a borrowed keyword array and count.

Provide a small function equivalent to:

`ReportStatus report_row_matches(const ReportRow *row, const ReportFilter *filter, int *matches);`

This function performs no network or filesystem operations.

## C5. CSV schema

Create exactly these columns, in this order:

```text
document_url,document_type,source_page_url,source_depth,status,sha256,size_bytes,stored_path,error
```

Each output data row corresponds to one source-document relationship passing the keyword filter.

Document type values: `PDF`, `DOCX`, `ZIP`.

Status values: `DOWNLOADED_NEW`, `DUPLICATE`, `DOWNLOAD_FAILED`, `SKIPPED_LIMIT`.

The `sha256` field contains 64 lowercase hexadecimal characters only when content was successfully stored or matched as an existing duplicate.

The file path identifies the actual stored copy, including when multiple document URLs point to identical contents.

Use decimal integer output for depth and bytes.

Preserve deterministic order: Phase 8 document first-discovery order, then each document's stored source-page order.

## C6. Safe CSV encoding

Implement RFC 4180-style CSV encoding:

1. Write the header once.
2. Enclose all text fields in double quotes.
3. Replace each embedded `"` with `""`.
4. Preserve embedded commas inside quoted fields.
5. Handle embedded CR/LF inside quoted fields correctly.
6. Use CRLF as the output record separator.
7. Check every write and close.
8. Use bounded field-length validation.

Prevent spreadsheet formula injection: when a text field begins with `=`, `+`, `-`, `@`, or leading control/whitespace that could precede these, prefix an apostrophe before CSV quoting.

Do not assume wrapping a dangerous value in double quotes alone prevents formula evaluation.

Do not modify the caller's original strings.

Test commas, quotes, newlines, formula-like input, empty strings and long permitted fields.

## C7. Safe report-file writing

Provide an API equivalent to:

`ReportStatus report_write_csv(const char *existing_output_directory, const char *safe_csv_basename, const ReportRow *rows, size_t row_count, const ReportFilter *filter, ReportResult *result);`

`ReportResult` should contain:

- Status.
- Rows written.
- Rows excluded by the filter.
- Owned output path on success or cleanup failure.
- Diagnostic message.

Use `report_result_free()` for owned memory.

Rules:

- Output directory must already exist.
- Report filename must be a safe basename ending in `.csv`.
- Maximum filename length 128 bytes.
- Never accept traversal or absolute paths as a basename.
- Create the report exclusively; never overwrite an existing file.
- Limit the input to 1,000 rows.
- On write or close failure, remove only the new report created by this call.
- If cleanup fails, return a distinct cleanup error and the leftover path.
- Do not append to an existing report.
- A filter matching no rows produces a valid header-only CSV.

Report code must have no network dependency and must not modify files created by Phases 9–10.

## C8. Required Phase 11 tests

Test:

- No-keyword inclusion.
- One matching keyword.
- Multiple keywords with any-match semantics.
- Case-insensitive matching.
- Match in document URL.
- Match in source-page URL.
- No match.
- Duplicate source relationships represented correctly.
- Exact CSV headers and row order.
- Quotes, commas, CR/LF, empty fields.
- Formula-injection protection.
- Safe decimal formatting.
- Existing CSV collision.
- Invalid filename and directory.
- Empty report.
- 1,000-row limit.
- Fault-injected allocation, write and close errors.
- Cleanup and retry.
- No changes to caller-owned input strings.

Use deterministic temporary files and compare expected CSV bytes.

## C9. Phase 11 acceptance gate

Require all new tests and previous regression tests to pass.

Update:

- `documentation/phase11.md`
- README/API/architecture/progress/testing documents.
- `prompt history/2026-11-09-phase11-design-and-handoff.md`

Commit and safely push Phase 11 independently.

Document clearly that keyword filtering searches **metadata**, not the contents of downloaded documents.

### STOP AND REREAD CHECKPOINT 11

**Do not begin Phase 12 unless Phase 11 is verified complete.**

After passing all tests and verifying the Phase 11 GitHub commit, reopen the original master instruction file and reread the shared contracts and Phase 12 requirements.

Do not reinterpret Phase 12 as permission to add unrequested features.

---

# PART D — PHASE 12: COMPLETE END-TO-END PROGRAM

## D1. Exact final objective

Connect all completed modules into one usable C11 application.

The workflow must be:

```text
Starting URL
    |
    v
Bounded BFS crawler
    |
    v
PDF / DOCX / ZIP link discovery
    |
    v
Optional metadata keyword filtering
    |
    v
Unique document URL selection
    |
    v
Safe binary downloads
    |
    v
SHA-256 file hashing
    |
    v
Content deduplication and type folders
    |
    v
CSV report with source-page provenance
    |
    v
Final statistics and completion status
```

This must be a real program, not a simulated pipeline or a sequence of unrelated demos.

Reuse the previously implemented public APIs.

Do not rewrite the crawler or downloader.

## D2. Required files

Create:

- `include/pipeline.h`
- `src/pipeline.c`
- `examples/deepdownload.c`

Create tests:

- `tests/test_pipeline.c`
- `tests/test_pipeline_integration.c`
- `tests/test_pipeline_alloc.c`, with fault injection where practical.

Add a final executable named:

`build/deepdownload`

or `build/deepdownload.exe` on Windows.

Add an appropriate Makefile target.

## D3. Pipeline configuration API

Define `PipelineConfig` with these borrowed fields:

- `const char *start_url`
- `const char *output_root`
- `const char *report_filename`
- `size_t max_depth`
- `int same_domain`
- `long timeout_seconds`
- `const char *const *keywords`
- `size_t keyword_count`

Do not add other user-configurable fields without a demonstrated need.

Define `PipelineStats` containing at minimum:

- HTML pages visited.
- HTML pages failed.
- HTML links discovered.
- Unique document URLs discovered.
- Document/source relationships discovered.
- Unique document URLs selected by the filter.
- Source/document relationships selected.
- Newly stored documents.
- Duplicate-content documents.
- Failed document downloads.
- Documents skipped due to the run limit.
- Successful document bytes downloaded.
- Bytes newly stored.
- CSV rows written.

Define `PipelineStatus` with distinct outcomes for:

- Complete success.
- Partial crawl or failed documents.
- Resource limit reached.
- Invalid argument.
- Memory failure.
- Filesystem failure.
- Report creation failure.
- Other fatal internal failure.

Provide:

`PipelineStatus pipeline_run(const PipelineConfig *config, PipelineResult *result);`

Also provide result initialization, cleanup and status-message functions.

A successful result may own the final CSV path. All owned strings must be freed by `pipeline_result_free()`.

No other heap allocation should escape the API.

## D4. Exact resource budgets

Phase 12 must preserve all previous limits and additionally cap each run at:

**Maximum 10 unique document download attempts.**

Each document remains limited to 20 MiB, so the maximum successful binary payload under this policy is 200 MiB.

A redirect hop is part of its document attempt, not an additional selected document.

Do not automatically download hundreds or thousands of discovered files.

If more than ten unique document URLs match the filter:

- Attempt only the first ten in discovery order.
- Mark remaining selected documents `SKIPPED_LIMIT`.
- Include these rows in the report.
- Return a limit-reached/incomplete outcome.
- Never claim all selected documents were downloaded.

Do not perform aggressive retries or parallel downloads.

## D5. Required output layout

The user supplies an existing output root.

Prepare this layout:

```text
output-root/
    staging/
    pdf/
    docx/
    zip/
    report.csv
```

`report.csv` is the example filename; the actual safe CSV basename is supplied by `PipelineConfig`.

The `staging` directory contains temporary completed Phase 9 downloads awaiting organization.

The `pdf`, `docx` and `zip` directories contain the canonical SHA-256-named stored files.

Create only these known child directories under the trusted output root.

Never use an untrusted URL as a filesystem path.

Never overwrite existing files.

## D6. Complete processing algorithm

Implement the following exact sequence.

### Step 1 — Validate before networking

Validate configuration pointers, seed URL, depth, domain flag, timeout, existing output root, safe report filename and keyword array.

If the requested CSV file already exists, fail before contacting any website.

Use the existing URL, filename and report validators.

Prepare only the required output directories.

### Step 2 — Discover documents

Create a zeroed `DocumentResults` and `CrawlerStats`.

Call `document_discover()` once.

Interpret its result:

- `DOCUMENT_OK`: continue normally.
- `DOCUMENT_PARTIAL`: continue with existing discovered records, but final pipeline result cannot be full success.
- `DOCUMENT_LIMIT_REACHED`: continue only with bounded already-discovered records and mark the final run incomplete.
- Invalid argument, OOM or fatal crawler error: stop before binary downloads, clean up and return the corresponding fatal status.

Do not throw away valid discovered provenance merely because a child HTML page failed.

### Step 3 — Apply keyword filtering before downloads

For each `DocumentRecord`, inspect all its `DocumentSource` entries.

Build a selection flag for each source/document pair using the Phase 11 metadata filter.

A unique document URL is selected if at least one of its source relationships matches.

When no keywords were supplied, select every record and source relationship.

Download a selected unique document URL only once, regardless of its number of sources.

Only matching source relationships should appear in the filtered report.

Do not use downloaded-file contents for keyword matching.

### Step 4 — Apply the ten-document limit

Iterate selected unique documents in first-discovery order.

Only the first ten are eligible for download.

Mark every later selected document as `SKIPPED_LIMIT` without making a network request.

Retain its URL/type/source information so it can appear in the report.

### Step 5 — Create a safe staging filename

For each eligible selected document, generate a simple unique staging basename.

Suggested format:

- `stage-000001.pdf`
- `stage-000002.docx`
- `stage-000003.zip`

Use a bounded sequential candidate search in the `staging` directory.

Do not overwrite or delete any preexisting staging file.

Rely on Phase 9's exclusive-create behavior to handle file-creation races. If a candidate already exists, choose another within a documented finite attempt limit.

### Step 6 — Download the document

Construct a `DownloadConfig` using:

- The selected Phase 8 normalized URL.
- The original seed URL.
- The original same-domain policy.
- The validated timeout.
- The staging directory.
- The generated safe basename.

Call `download_document()`.

On a normal HTTP/timeout/MIME/size failure:

- Record `DOWNLOAD_FAILED`.
- Preserve the error explanation.
- Continue to the next selected document.
- Do not create a fake hash or saved path.
- Keep the final run status partial.

On out-of-memory, unrecoverable filesystem failure or failed partial-file cleanup:

- Stop safely.
- Retain and report any path that could not be cleaned up.
- Do not claim successful completion.

Do not retry failed URLs automatically.

### Step 7 — Organize and deduplicate

After a successful Phase 9 download, pass its completed staging path to Phase 10's `organize_file()`.

If the organizer returns a newly stored file:

- Record `DOWNLOADED_NEW`.
- Retain the digest, actual stored path and verified byte count.

If it returns a duplicate:

- Record `DUPLICATE`.
- Retain the existing canonical stored path and digest.
- Do not create another saved copy.

The organizer must leave the staging input untouched.

**Only after organizer success**, the pipeline may delete its own completed staging file using a checked removal operation.

Never delete a preexisting staging file.

If staging cleanup fails, record an explicit error and the orphaned path instead of silently ignoring it.

Do not delete a successfully organized canonical file because a later report write fails.

### Step 8 — Preserve provenance for reporting

For every matching `(document URL, source page)` pair, construct one `ReportRow`.

A document's download/organization result is shared across all its source rows.

Thus a document with three matching source pages produces:

- One document-download attempt.
- One organized-content result.
- Three CSV rows referencing the same result.

For duplicate content from two different document URLs, their rows must share the same canonical stored path and SHA-256 digest.

Keep the `DocumentResults`, per-document download/organization results and borrowed row pointers alive until CSV writing finishes.

Do not shallow-copy owned result structures.

### Step 9 — Write the CSV

Pass the rows to Phase 11's `report_write_csv()`.

Do not overwrite an existing report.

Produce one row per matching source-document relationship, including selected download failures and skipped-limit documents.

If the keyword filter matched nothing, generate a header-only CSV with no document downloads.

If CSV writing fails:

- Return a report/fatal filesystem error.
- Do not falsely claim the pipeline completed.
- Keep already organized canonical files intact.
- Report any incomplete CSV file that cleanup could not remove.

### Step 10 — Finish and free memory

Free all borrowed-lifetime dependencies in the correct order:

1. Finish writing the CSV while row pointers remain valid.
2. Free report-owned allocations after transferring or copying any final path needed by `PipelineResult`.
3. Free every per-document `DownloadResult`.
4. Free every per-document `OrganizeResult`.
5. Free report-row arrays and temporary selection arrays.
6. Free `DocumentResults`.
7. Release any other pipeline allocations.

Do not call `http_cleanup()` while other HTTP operations remain active. The CLI may call it once at program exit.

Support a new `pipeline_run()` call after a previous call succeeds or fails.

## D7. Final statistics and return semantics

Define and test these rules:

- `downloaded_new`: successfully stored new unique physical files.
- `duplicates`: selected URLs whose bytes matched an existing canonical file.
- `download_failed`: selected URLs attempted but not successfully downloaded/organized.
- `skipped_limit`: selected URLs beyond the ten-document attempt budget.
- `bytes_downloaded`: successful document payload bytes transferred, including successful downloads later recognized as duplicates.
- `bytes_newly_stored`: bytes placed in newly created canonical files only.
- `report_rows`: matching source/document relationships written to CSV.

Do not count a duplicate as a new physical file.

Do not count a failed or skipped download as successful.

A complete pipeline run requires a completed crawl within its configured bounds, all selected attempted documents successfully handled, no skipped-limit documents and a successfully created CSV.

If individual documents fail but the report is written, return `PARTIAL`.

If a deliberate phase/crawl/download budget is reached, return `LIMIT_REACHED`, even if the bounded work produced useful files.

A fatal memory or filesystem failure returns an error, not success.

Keep partially completed but valid canonical files. Never falsely claim atomic rollback of an entire multi-document run.

## D8. Required final CLI

Implement a CLI similar to:

```text
deepdownload START_URL OUTPUT_ROOT REPORT_FILENAME [DEPTH] [SAME_DOMAIN] [--keyword WORD]...
```

Defaults:

- Depth: 1.
- Same-domain: 1.
- Timeout: 3 seconds.
- No keywords: process all discovered document metadata within the documented limits.
- At most eight keyword arguments.

Limit CLI depth to a conservative range such as 0–3, while keeping the underlying existing crawler API unchanged.

Validate every CLI argument and show a short usage message on invalid input.

The program must print a clear final summary, including:

- Crawl status.
- Pages visited and failed.
- Discovered unique document count.
- Selected document count.
- Documents newly downloaded/stored.
- Content duplicates.
- Download failures.
- Documents skipped due to the run limit.
- Saved bytes.
- Report path.
- Overall completion status.

Return process exit code 0 only for full success.

Use a distinct nonzero exit for partial/limit results and another nonzero exit for fatal errors.

No GUI or web dashboard.

## D9. Mandatory end-to-end controlled fixture

**Build one C-only local HTTP fixture that serves both HTML pages and binary document responses on the SAME listening port.**

This matters because the existing scope policy pins the seed's effective port. Do not accidentally use separate HTML and document fixture ports in the end-to-end test.

The fixture should contain:

```text
/site
    -> /courses
    -> /research
    -> /docs/rules.pdf
    -> /files/material.zip

/courses
    -> /docs/rules.pdf
    -> /docs/lecture.docx

/research
    -> /docs/rules.pdf
    -> /docs/research.pdf
```

Provide valid binary responses with recognized MIME types.

For a duplicate-content test, make `rules.pdf` and `research.pdf` return identical bytes.

Expected unfiltered behavior with sufficient depth:

- Three HTML pages visited.
- Four unique document URLs discovered.
- Six unique document/source relationships.
- Four selected document URLs.
- Four successful binary downloads.
- Three newly stored physical files.
- One content duplicate.
- Two physical PDF/Word/ZIP-related type distributions according to the first stored copy.
- Six CSV data rows with correct provenance.
- The two duplicate-content URLs reference one stored path and one SHA-256 digest.
- No source-page relationship is lost.

The exact byte count and digest must be calculated from controlled fixture content and asserted, not guessed.

### Additional end-to-end scenarios

Test:

1. Root depth zero.
2. Multiple crawl depths.
3. No documents.
4. Keyword selecting one document.
5. Keyword matching a source page rather than the document URL.
6. Keyword matching nothing.
7. Same document found on multiple pages.
8. Two different document URLs with identical contents.
9. Same-size documents with different contents.
10. MIME mismatch and HTTP 404.
11. Failed child HTML page but useful document discoveries.
12. Redirected HTML page with correct source attribution.
13. Binary redirect within the allowed scope.
14. Blocked off-host and off-port redirects with zero target requests.
15. An existing canonical duplicate file from a prior run.
16. Existing report filename: no overwrite and no network requests.
17. Existing staging file: no overwrite.
18. More than ten eligible documents: only ten download attempts.
19. Missing root directory and invalid CLI arguments.
20. Report write/close errors.
21. Cleanup failure and preserved path diagnostics.
22. Out-of-memory during metadata selection, download results, organization and report preparation.
23. Successful second run after a failed run using a new report filename.
24. Preservation of byte-for-byte file contents.
25. Existing Phases 1–11 regression suites still passing.

Verify request counters, filesystem contents, hashes, CSV data, actual statuses and all relevant counters.

Do not simulate a successful download without inspecting the resulting file bytes.

## D10. Final project demonstration

Produce a real controlled local demonstration using the complete `deepdownload` CLI.

Record:

- The actual invocation.
- The actual exit code.
- Number of crawled HTML pages.
- Discovered document URLs.
- Selected documents.
- Files newly stored.
- Duplicate detection result.
- Output directory tree.
- Actual hashes and file sizes.
- Exact CSV header and representative rows.
- Results of a keyword-filtered run.

Use the local C fixture, not a public website.

Do not fabricate a transcript, screenshot or test result.

## D11. Final quality verification

Run from a clean build:

- `make clean`
- `make`
- `make test`
- Every new Phase 10 target.
- Every new Phase 11 target.
- Every new Phase 12 target.
- Optional existing HTML allocation test.
- Strict `-std=c11 -Wall -Wextra -Wpedantic -Werror` compilation.
- Targeted ASan/UBSan runs.
- Leak-enabled testing if the environment supports it.
- Actual controlled CLI demonstration.

All deterministic tests must pass.

Do not claim native Windows success without a real native Windows run.

Preserve Windows/MSYS2 build support and explain fixture portability limitations.

Confirm the production static library contains no test fault-injection hooks or transport stubs.

No Python/JS tests or executable project helpers.

## D12. Documentation and project handoff

Update:

- `README.md`
- `documentation/api.md`
- `documentation/architecture.md`
- `documentation/progress.md`
- `documentation/testing.md`

Create:

- `documentation/phase12.md`
- `prompt history/2026-11-09-phase12-design-and-handoff.md`

Explain the full workflow in beginner-friendly terms.

For every new public C function, explain:

- Why it exists.
- Its parameters.
- Pointer usage.
- What memory it allocates.
- Who owns the result.
- How its error handling works.
- How it connects to other phases.

Include one clear, nontechnical explanation of the project's value:

**A user enters a public website address. The C program follows permitted links, discovers PDF/DOCX/ZIP documents, records where each document was found, downloads selected files, organizes them by type, avoids storing identical content twice, and generates a searchable CSV inventory.**

Distinguish this from ordinary one-page scraping, and distinguish URL deduplication from actual file-content deduplication.

Do not claim full-text document search, unrestricted website compatibility or perfect network security.

## D13. Phase 12 final acceptance gate

Mark Phase 12 complete only when:

- All existing Phases 1–11 tests pass.
- Phase 12 end-to-end fixtures pass.
- Actual saved files match their fixture bytes.
- SHA-256 hashes match independently verified expected values.
- Duplicate-content files are not stored twice.
- Source attribution survives end-to-end.
- Keyword filtering is correct.
- CSV output is valid and safe.
- Existing files cannot be overwritten by ordinary pipeline operations.
- Failed downloads do not create false success files.
- Resource limits stop operations safely.
- Every owned allocation and result has a clear cleanup path.
- Documentation and complete prompt history are present.
- The final GitHub push is verified.

If any of these requirements fails, report **PARTIAL or BLOCKED** and do not call the project complete.

---

# PART E — MASTER PROMPT HISTORY AND GIT DELIVERY

## E1. Save this complete instruction

Preserve the existing folder named exactly:

`prompt history`

Save the COMPLETE accessible text of this master work order as:

`prompt history/2026-11-09-phases10-12-master-instruction.md`

Do not replace it with a short summary.

All three phase-specific decision/handoff files must reference this same master instruction.

Earlier Phase 1–9 history must remain intact.

If text is unavailable or truncated, disclose that honestly rather than inventing missing instructions.

## E2. Separate phase commits

Create at least one independently identifiable code-and-documentation commit for each successfully completed phase:

- `Phase 10: add SHA-256 organization and content deduplication`
- `Phase 11: add metadata keyword filtering and CSV reporting`
- `Phase 12: integrate complete DeepDownload-C pipeline`

Each commit must have its own acceptance report.

After each phase, perform the checks specified in Part A.

Review the diff to ensure only intended C source/header files, C tests, Makefile and Markdown documentation/history are committed.

Exclude binaries, downloaded documents, output CSV files, caches, credentials, secrets and the unrelated outer Replit scaffold.

Only attempt a normal non-force push if the remote is compatible and the user has already authorized GitHub delivery.

On a push failure, remote divergence or authentication denial, stop and report the exact problem. Never force-push or pretend the remote matches local HEAD.

Verify the live GitHub `main` SHA after every successful push.

Avoid unnecessary documentation-only receipt commits. When a receipt commit is unavoidable, keep it documentation-only and verify it independently.

## E3. Final response after each phase

Every phase handoff must state:

1. COMPLETE, PARTIAL or BLOCKED.
2. Precisely what code was added or changed.
3. Which earlier APIs were preserved.
4. How important pointers and allocations are owned.
5. The exact test commands, exit statuses and assertion counts.
6. Any failed/skipped/not-verified tests.
7. Sanitizer and Windows verification status.
8. New documentation and prompt-history files.
9. GitHub commit SHA and verified remote SHA.
10. Unresolved problems.
11. Whether the next phase's acceptance gate is satisfied.

Do not merge three incomplete phases into one optimistic completion report.

## E4. Required reread before EVERY new phase

After completing Phase 10, reread this file before starting Phase 11.

After completing Phase 11, reread this file before starting Phase 12.

Confirm that no earlier phase requirement was weakened.

If new work reveals a contradiction, prefer preserving the tested existing API and the explicit safety requirements over adding unnecessary complexity. If an essential design choice is genuinely unresolved, stop and report the blocker; do not silently change project scope.

## E5. Final Phase 12 completion response

When all three phases are actually complete, provide:

- **FINAL STATUS: COMPLETE, PARTIAL or BLOCKED**
- GitHub repository and final SHA.
- Phase 10 commit SHA.
- Phase 11 commit SHA.
- Phase 12 commit SHA.
- Complete implemented module/file inventory.
- Full real controlled-demo results.
- Final output directory structure.
- Saved-file SHA-256 evidence.
- Actual CSV example.
- Total unique discovered documents and provenance relationships.
- New stored files, duplicates, failed downloads and skipped files.
- All regression tests and sanitizer outcomes.
- Native Windows status.
- Remaining known limitations.
- Beginner explanation of how the whole project works.
- Confirmation that all master prompt and handoff records were preserved.
- Confirmation that no higher-cost execution mode, paid feature or deployment was intentionally enabled or changed.
- Explicit declaration: **NO FURTHER PHASES STARTED.**

**STOP after Phase 12.**

This assignment is complete only when the final integrated C program is verified—not merely when the three module names exist.

---

# FINAL NONNEGOTIABLE INSTRUCTION

**Implement Phase 10 → test → document → commit → verify GitHub → reread this master prompt → implement Phase 11 → test → document → commit → verify GitHub → reread this master prompt → implement Phase 12 → test → document → commit → verify GitHub → final verified handoff → STOP.**

Never advance through a failed acceptance gate.

Never change Power Mode, paid execution settings, deployment status or the project's language requirements without explicit user approval.

**END OF MASTER WORK ORDER — DEEPDOWNLOAD-C PHASES 10–12.**