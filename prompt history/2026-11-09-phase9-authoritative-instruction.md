# DEEPDOWNLOAD-C — PHASE 9 AUTHORITATIVE WORK ORDER
## Safe Binary Document Downloader in C11

**Status:** Phase 8 accepted. Implement Phase 9 only.

### 1. Objective

Build a safe, reusable **binary file downloader in C11** that downloads the PDF, DOCX and ZIP document URLs discovered in Phase 8.

Unlike the existing HTML downloader, the new module must stream binary bytes directly to a file without treating them as text or storing the entire file in RAM.

**This phase is about downloading one document safely and correctly.** Do not implement file-type folders, SHA-256 deduplication, keyword filtering, CSV reports or final product orchestration yet.

Use simple, modular C code suitable for explanation during a university C-programming viva.

### 2. Nonnegotiable constraints

- All executable code, library logic and tests must be written in C11.
- Use GCC, libcurl, the existing Makefile and standard C functionality.
- Do not implement React, JavaScript, TypeScript, Python, C++, Rust, AI agents or browser automation.
- Do not change Replit's selected Agent mode, enable Power Mode, expert/paid modes, increase compute, upgrade, deploy or publish.
- If a mode change is required, stop and request explicit approval.
- Do not start Phases 10, 11 or 12.
- Do not bypass logins, CAPTCHA, rate limits or access controls.
- Do not introduce concurrency, background workers or unnecessary dependencies.

**Important:** These written restrictions do not technically control Replit billing. The user will select and verify the permitted mode manually before execution.

### 3. Verified starting baseline

Canonical GitHub repository:

https://github.com/jatinvimal1506/Web-Crawler-and-Scraper-in-C

Branch: `main`

Latest Phase 8 commit:

`89e3648f52d9c82c77e91fee68a543f36e4510a7`

Before editing, verify that the local C-only checkout matches the latest remote branch and that no unrelated changes would be overwritten.

Preserve the existing modules:

- `url.c` — URL validation and normalization.
- `http.c` and `http_scoped.c` — HTML requests, timeout handling and scoped redirects.
- `html.c` — HTML link extraction.
- `queue.c` and `visited.c` — BFS data structures.
- `project.c` — bounded BFS crawler and observer API.
- `document.c` — PDF/DOCX/ZIP link discovery, deduplication and source attribution.

The existing `http_download()` returns HTML content. **Do not simply reuse its HTML-only response buffer for document downloads.**

The Phase 8 `DocumentResults` structure already stores document URLs, types and source pages. Preserve that API and ownership contract.

### 4. Required module and public API

Create:

- `include/download.h`
- `src/download.c`

Design a clear public API for downloading one recognized document at a time.

The API should accept the document URL, seed URL, same-domain policy, timeout, existing output directory and safe destination filename.

Use a configuration structure if that makes the function simpler. Suggested data fields:

- Document URL.
- Seed/scope URL.
- Same-domain flag.
- Timeout in seconds.
- Existing output directory.
- Destination basename.

Return structured information including:

- Download status.
- Final URL after redirects.
- HTTP status where available.
- Number of bytes successfully written.
- Final local file path.
- A concise diagnostic message when the operation fails.

The caller must have an explicit function to free all returned heap memory.

Keep status codes distinct for invalid input, unsupported file type, out-of-memory, timeout, network error, rejected redirect, HTTP response error, invalid content type, excessive size, existing destination, filesystem write failure and incomplete-file cleanup failure.

Document exact output ownership and whether any fields remain valid on failure.

### 5. Binary streaming requirements

Use libcurl's write callback to write incoming data incrementally.

Required behavior:

1. Receive binary data in bounded chunks.
2. Write exactly the received bytes using a checked file-writing operation.
3. Do not call `strlen()` or another text function on binary response contents.
4. Preserve embedded NUL bytes and non-ASCII byte values.
5. Detect integer overflow when calculating callback sizes.
6. Handle short or failed writes correctly.
7. Enforce a maximum document size of **20 MiB** per download.
8. Reject anything beyond this limit, including responses with missing or misleading Content-Length.
9. Keep memory usage bounded independently of the total file size.
10. Reject failed or incomplete downloads rather than publishing them as successful files.

Use `size_t` safely and validate multiplication/addition boundaries.

Do not use the existing 5 MiB HTML-response limit for binary documents. Keep that HTML limit unchanged.

### 6. File identification and response validation

Only accept input document URLs recognized by Phase 8 as PDF, DOCX or ZIP.

Validate the actual HTTP response metadata before treating the downloaded file as successful.

Accept appropriate MIME types:

| Type | Expected MIME |
|---|---|
| PDF | `application/pdf` |
| DOCX | `application/vnd.openxmlformats-officedocument.wordprocessingml.document` |
| ZIP | `application/zip`, `application/x-zip-compressed` |

A generic `application/octet-stream` response may be accepted for a recognized document extension, but this policy must be explicitly documented and tested.

Reject `text/html`, unrelated MIME types, missing or ambiguous content types unless a narrowly justified fallback is implemented.

Do not claim that MIME type alone proves the downloaded content is structurally valid.

Reject HTTP 4xx/5xx and incomplete or partial-content responses. Do not silently resume downloads in this phase.

Empty document responses should be rejected and cleaned up.

Do not implement PDF parsing, DOCX extraction, ZIP extraction or content hashing.

### 7. Redirect and network safety

**Do not reintroduce the Phase 6 redirect vulnerability that Phase 7 fixed.**

A binary document may redirect to another URL. Before requesting each redirected destination:

- Normalize and validate the URL.
- Allow only HTTP and HTTPS.
- Respect the seed's existing effective-port restriction.
- Respect exact hostname matching when same-domain is enabled.
- Reject HTTPS-to-HTTP downgrade redirects.
- Keep TLS certificate and hostname verification enabled.
- Enforce the existing maximum of five redirects.
- Enforce the configured timeout range of 1–3600 seconds.

Prefer reusing or carefully adapting the existing checked scoped-request logic, without replacing or weakening the functioning HTML downloader.

Do not enable automatic redirects that bypass pre-request scope checking.

A rejected destination must receive **zero HTTP requests** in local tests.

Document that hostname restrictions are not a complete DNS/IP-level firewall.

### 8. Safe output filenames and directories

Do not directly trust filename text from a URL or HTTP Content-Disposition header.

Use caller-supplied, strictly validated safe basenames such as:

- `document-0001.pdf`
- `document-0002.docx`
- `document-0003.zip`

Validate permitted ASCII characters, maximum filename length and correct extension.

Reject:

- `../` or `..\\` traversal.
- Forward or backward path separators.
- Drive prefixes and absolute paths.
- Empty filenames.
- `.` and `..`.
- Control characters.
- Unsafe Windows-reserved names.
- Filenames ending in problematic spaces or dots.

The output directory must already exist and be supplied explicitly by the user. Do not create arbitrary directories or use untrusted URL paths as filesystem paths.

**Never silently overwrite an existing file.**

Use an exclusive-create approach supported by the target C11 runtime. A straightforward acceptable design is exclusive creation of the destination and deletion of that newly created file on any incomplete transfer.

If choosing temporary `.part` files and renaming, do not use an unchecked rename that could overwrite an existing destination. Explain platform-specific behavior honestly.

Track whether this run created the file so failure cleanup never deletes a preexisting user file.

Check all file close, write and cleanup outcomes. If partial-file removal fails, return a distinct error rather than claiming cleanup succeeded.

Document whether files can be visible during transfer; do not claim atomic publication unless actually guaranteed.

### 9. Integrating with Phase 8

Reuse `DocumentResults` and `document_type_from_url()`.

The new downloader should accept a single document URL returned by Phase 8 without altering any stored provenance information.

For Phase 9, provide a simple one-document workflow:

1. The Phase 8 result identifies a PDF, DOCX or ZIP URL.
2. The Phase 9 downloader receives that URL and an approved output destination.
3. It validates the request and response.
4. It streams the document to disk.
5. It returns a status, file path, final URL and byte count.

**Do not redesign Phase 8 to automatically download every discovered link.**

Bulk orchestration and final integration belong to Phase 12.

Do not implement type-based folders or file-content deduplication now.

### 10. Required C-only CLI demonstration

Add a simple executable, preferably:

`build/download-document`

It should accept a document URL, seed URL, output directory, safe destination filename and the relevant domain policy.

Use a conservative timeout.

Print the status, expected file type, final URL, byte count and local path after successful completion.

On failure, print a useful explanation, return a nonzero process status and ensure no false success file remains.

The demo must work against a controlled local C HTTP fixture. Do not use live third-party sites as automated tests.

### 11. Required deterministic tests

Create focused C tests, preferably:

- `tests/test_download.c`
- `tests/test_download_integration.c`
- `tests/test_download_alloc.c`

Add test targets to the Makefile and include them in the normal test suite.

**A. Binary correctness**

Verify byte-for-byte equality between source fixture data and saved file contents.

Test binary data with embedded NUL bytes, high-value bytes, multiple chunks and a file larger than the old HTML limit.

**B. File-size limits**

Test exactly 20 MiB and 20 MiB plus one byte, both with accurate and missing Content-Length headers.

The oversized download must fail without leaving a successful or partial output file.

**C. HTTP and response failures**

Test PDF/DOCX/ZIP MIME handling, generic binary MIME policy, HTML disguised as a document, missing MIME, empty response, 404/500, truncated transfer, interrupted connection, timeout and unsupported document URL.

**D. Redirect safety**

Test permitted same-host redirects, relative Location values, blocked other-host and other-port targets, HTTP(S)-only policy, redirect loops and maximum redirect count.

Assert that blocked destinations received no request.

**E. Filesystem safety**

Test invalid basenames, traversal attempts, existing file collision, permission/write errors when reproducible, failed close/cleanup where safely injectable, and successful reuse after a failed download.

Verify that existing files remain byte-for-byte unchanged.

**F. Memory management**

Use test-only fault injection where appropriate.

Verify out-of-memory handling, response metadata ownership, cleanup, repeatable free functions and absence of test allocator hooks in the release static library.

Do not claim a full leak check if the environment cannot perform one.

### 12. Preserve all previous work

All Phases 1–8 must continue to pass.

Do not remove, weaken or rewrite old test assertions merely to achieve green results.

Do not alter the existing document discovery, URL normalization, queue, visited or HTML parser behavior unless a demonstrated compatibility defect requires a minimal documented correction.

Do not change the established crawler statistics meanings.

### 13. Build and quality verification

Run a clean build and complete regression suite.

Also execute the dedicated downloader unit, local integration and allocation-failure tests.

Use strict C11 compilation with `-Wall -Wextra -Wpedantic -Werror`.

Run AddressSanitizer and UndefinedBehaviorSanitizer where available.

Record exact commands, exit statuses, warnings, test counts and skips.

Attempt a leak-enabled run if supported, but clearly distinguish a tool/environment failure from a successful leak scan.

Keep the Makefile's existing Windows/MSYS2 branches. If native Windows is unavailable, mark Windows runtime **NOT VERIFIED**, not passed.

Execute a real local binary-download demonstration and verify saved file bytes.

### 14. Mandatory documentation

Update:

- `README.md`
- `documentation/api.md`
- `documentation/architecture.md`
- `documentation/progress.md`
- `documentation/testing.md`

Provide a student-friendly explanation of the new functions, especially:

- Why a binary download cannot be treated as a C string.
- What the libcurl write callback does.
- How `FILE *` works.
- Why `fwrite()` return values must be checked.
- How file-size limits are enforced.
- How safe filenames are generated or validated.
- How ownership and cleanup work.
- Why pre-request redirect validation matters.
- What happens when an interrupted download leaves partial data.

Keep previous phase documentation as historical evidence.

### 15. Mandatory prompt history

Retain the exact folder name:

`prompt history`

Preserve every earlier phase's Markdown documents.

Add:

**2026-11-09-phase9-authoritative-instruction.md**

The complete accessible copy of this entire work order, not an abbreviated summary.

**2026-11-09-phase9-design-decisions.md**

Explain API selection, streaming, response validation, file limits, safe filenames, collision handling, redirects, memory ownership and rejected alternatives.

**2026-11-09-phase9-acceptance-and-handoff.md**

Include requirement-by-requirement PASS/FAIL/NOT VERIFIED outcomes, changed files, test evidence, command exit codes, remaining limitations and Phase 10 readiness.

Do not invent previous prompts, timestamps or test results.

### 16. GitHub commit and upload

After successful implementation and testing:

1. Review exactly which C-project files changed.
2. Commit only the downloader source/header, C tests, examples, Makefile and Markdown documentation.
3. Exclude saved document samples, build artifacts, credentials, cache files and outer React/TypeScript scaffolding.
4. Check that the GitHub remote has not diverged.
5. Perform one normal non-force push if authenticated and authorized.
6. Verify that the remote branch SHA matches the pushed commit.
7. Report the commit link and SHA.

If permission is unavailable, stop and report it. Do not force-push or invent upload success.

### 17. Mandatory final handoff

Provide:

1. **COMPLETE, PARTIAL or BLOCKED.**
2. Exact changed files and their purposes.
3. All new public functions and memory/file ownership rules.
4. A verified binary-download example, including actual saved byte count and byte-for-byte file comparison.
5. New test results and all previous regression outcomes.
6. Failure/cleanup and security test evidence.
7. Sanitizer and Windows verification limitations.
8. New `prompt history` files.
9. GitHub commit and verified remote SHA.
10. An explicit **YES or NO** for readiness to start Phase 10, explaining why.
11. Confirmation that **no Phase 10 or later development was started**.

**STOP after Phase 9. No file hashing, type-based organization, keyword filtering, CSV reporting, AI, browser automation, paid mode changes or deployments.**

---

**END OF PHASE 9 WORK ORDER.**