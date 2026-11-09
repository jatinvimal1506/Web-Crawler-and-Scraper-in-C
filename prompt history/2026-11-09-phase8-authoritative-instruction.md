# DEEPDOWNLOAD-C — PHASE 8 AUTHORITATIVE IMPLEMENTATION PROMPT

## 1. Objective and strict scope

Implement **Phase 8: Public Document Link Discovery and Source Tracking** in the existing DeepDownload-C university C11 project.

This is an extension of our existing working web crawler. It must locate links to publicly accessible PDF, DOCX and ZIP documents across crawled HTML pages, classify the document type and record the source page containing each link.

**Phase 8 discovers document URLs. It does not download binary documents.**

Do not implement Phase 9 binary downloads, Phase 10 file organization/hash deduplication, Phase 11 keyword filtering/CSV reporting, or Phase 12 final integration.

Do not create an AI agent, web app, React interface, browser automation or unrelated features.

All executable project logic and tests must be written in **C11 only**. Keep the existing Makefile, GCC and libcurl toolchain.

**Cost restriction:** Do not switch to Power Mode, expert mode, higher-compute modes, paid features or deployment. Do not change Agent settings. If the work requires a mode change or upgrade, stop and request explicit user approval. Written instructions are not a technical billing lock.

## 2. Starting GitHub baseline

Repository:

https://github.com/jatinvimal1506/Web-Crawler-and-Scraper-in-C

Branch: `main`

Latest verified Phase 7 documentation commit:

`6dc654b1186863e6aaf155ecf24a96466649fdff`

Phase 7 implementation commit:

`29988d7f3676b16b415e59a57ba1aaf2f7b15c3b`

Before changing code:

1. Check the local Git branch, status and remote commit.
2. Preserve existing Phases 1–7.
3. Work inside the C-only project, not unrelated React/TypeScript scaffolding.
4. Do not overwrite divergent GitHub changes.
5. Run existing tests to establish a baseline.

The current project contains functional URL normalization, HTML downloading, hyperlink extraction, FIFO queue, visited tracking, bounded BFS crawling, timeout safety, and scoped redirect handling.

**Reuse these modules. Do not rebuild the crawler from scratch.**

## 3. Required Phase 8 functionality

### A. Identify document links

Recognize links to these formats:

- PDF — `.pdf`
- Microsoft Word — `.docx`
- ZIP archive — `.zip`

Detection must be case-insensitive.

Example URLs:

- `https://example.com/docs/report.pdf`
- `https://example.com/docs/NOTES.DOCX`
- `https://example.com/files/archive.zip`
- `https://example.com/docs/report.pdf?download=1`

For the final example, classify using the URL path extension, ignoring the query string.

Do not classify misleading filenames such as `report.pdfx`, `archive.zip.html` or `notes.doc`.

A URL such as `/download?id=123` has an unknown document type from its path alone. Record it as unsupported/unknown only if needed for diagnostics; do not falsely label it PDF or fetch it to guess.

Do not perform binary downloads, content sniffing or file-type inference from unverified extensions.

### B. Track the source page

Each discovered document occurrence must record:

- Absolute normalized document URL.
- The final absolute URL of the HTML page where the link was found.
- Document type: PDF, DOCX or ZIP.
- Source page crawl depth.

Example:

Document URL: `https://university.edu/notes/unit1.pdf`

Source page: `https://university.edu/departments/cse/resources`

Type: `PDF`

Source depth: `2`

The source URL is essential to our project's uniqueness. It allows us to explain where each public document was discovered.

The program must retain source information even when the same document appears on multiple different pages.

### C. Handle duplicates correctly

Distinguish **URL-level duplicate detection** from future binary-content deduplication.

During Phase 8:

- Normalize document URLs using the existing URL module.
- Treat identical normalized document URLs as one unique document URL.
- Preserve distinct source-page references for that URL.
- Avoid duplicate `(document URL, source page URL)` pairs.
- Keep records in deterministic discovery order.

Do not calculate SHA-256 hashes yet.

Two different document URLs might eventually return identical file contents; this will be handled in Phase 10, not Phase 8.

### D. Integrate with existing BFS

The existing crawler should discover links across pages according to its configured depth and site restrictions.

Prefer a small, documented additive observation/filter mechanism inside the existing crawler engine so the new document-discovery module can inspect successfully extracted HTML links.

Important rules:

1. Preserve the existing public `crawler_run()` behavior and API.
2. Use already extracted absolute links rather than writing a second HTML parser.
3. Use the final page URL after redirects for source attribution.
4. Follow ordinary eligible HTML pages using the existing BFS.
5. Recognized document links must be recorded but **not fetched as HTML pages** during the new document-discovery run.
6. Do not include document-link HTTP requests in the HTML-page crawl statistics.
7. Apply existing site restrictions and the seed's effective-port policy to discovered documents.
8. Deduplicate document URLs and preserve their source-page relationships.
9. Respect maximum crawl depth and existing page, link and scheduling budgets.
10. Do not weaken Phase 7's timeout and pre-request redirect safety.

Do not duplicate the entire BFS implementation in a separate document crawler.

If an additive callback or filter is needed, keep ownership simple and document when borrowed data becomes invalid.

### E. Memory ownership

Create a clearly documented document-discovery result structure with owned strings.

Use separate source/header modules, preferably:

- `include/document.h`
- `src/document.c`

Provide an API that accepts existing crawler configuration and returns structured discovery results and crawl statistics.

Include:

- Document type classification.
- Document discovery.
- Result count and access.
- Explicit cleanup of every returned allocation.
- Clear status codes for invalid input, memory failure, crawl failure and successful/partial completion.

Results must be safely initialized and freed using documented functions.

Every string must have exactly one documented owner.

Handle partial `malloc`/`realloc` failures without leaks, double-free or incorrectly reporting success.

Use simple C structures and dynamic arrays or linked lists; explain the selected design and its complexity. Do not introduce a database, JSON dependency or third-party collection framework.

## 4. Example expected behavior

Consider this controlled example website:

Home page:
- Links to `/courses`
- Links to `/research`
- Links to `/documents/rules.pdf`
- Links to `/files/material.zip`

Courses page:
- Links to `/documents/rules.pdf`
- Links to `/documents/lecture.docx`

Research page:
- Links to `/documents/rules.pdf`
- Links to `/documents/research.pdf`

Expected result:

- Three HTML pages processed, subject to requested depth.
- Four unique document URLs.
- Multiple source-page relationships retained for `rules.pdf`.
- Correct PDF, DOCX and ZIP classifications.
- No PDF, DOCX or ZIP body downloaded.
- Normalized document URLs and stable source references.

This example must be covered by deterministic automated tests.

## 5. Required tests

Create C-only tests, preferably:

- `tests/test_document.c`
- `tests/test_document_integration.c`
- `tests/test_document_alloc.c`

Reuse the existing local C HTTP fixture where possible.

### Classification tests

Test lowercase/uppercase extensions, path/query combinations, fragment removal, invalid extensions, unsupported schemes, relative URL resolution and malformed inputs.

### BFS integration tests

Use a controlled local site to verify:

- Documents found at depth 0, 1 and 2.
- Ordinary HTML pages continue to be crawled.
- Document bodies are never requested.
- Duplicate links on one page do not create duplicate source pairs.
- The same document linked by multiple pages keeps all distinct sources.
- Redirected HTML pages use their final page URL as the source.
- Same-domain and effective-port restrictions are respected.
- Outside-scope document links are not included as eligible results.
- Empty pages and pages without documents.
- Failed child pages and partial crawls.
- Maximum page/link/document-result budgets.
- Consistent crawl statistics and output order.

### Memory and error tests

Test NULL arguments, empty/invalid URLs, repeatable cleanup, input ownership, failures during result growth, duplication of source strings, and allocation-failure rollback.

Use test-only allocation injection when practical. Never include test hooks in the production static library.

All prior Phase 1–7 tests must continue to pass.

Do not weaken previous assertions or remove existing tests to obtain a passing build.

## 6. Resource and safety limits

Preserve:

- Existing maximum HTML response body size.
- Supported timeout range.
- HTTP(S)-only URL and redirect policies.
- Exact-host restriction when same-domain is enabled.
- Effective-port restriction.
- Existing page, link and scheduled-key budgets.
- No concurrent or unlimited network requests.

Set a documented safe maximum for stored document occurrences, preferably 1,000, and distinguish a limit-reached result from exhaustive success.

Do not perform downloads from arbitrary third-party services without permission.

Do not bypass login, CAPTCHA, robots restrictions or access controls.

Use controlled local fixtures for deterministic tests.

## 7. CLI demonstration

Add a simple C-only document-discovery demo executable, for example `build/discover-documents`.

It should accept:

- Starting URL.
- Maximum crawl depth.
- Same-domain option, if appropriate.

It should display:

- Crawl completion status.
- HTML pages visited/failed.
- Unique document URLs.
- Number of source-document relationships.
- PDF, DOCX and ZIP counts.
- Each document's URL, type and source page.

A concise, readable terminal output is sufficient.

Do not create a GUI, web dashboard or binary downloader.

CSV export and keyword filtering belong to Phase 11, not Phase 8.

## 8. Build and validation requirements

Update the Makefile to compile the new C module into the existing static library and include the new tests in normal testing.

Run:

- Clean build.
- All existing test targets.
- Document unit tests.
- Local document integration tests.
- Allocation-failure tests.
- Strict C11 warning checks.
- AddressSanitizer/UndefinedBehaviorSanitizer where available.
- An actual controlled local CLI demonstration.

Report literal commands, exit codes, test counts, failures, warnings and skips.

Do not claim Windows execution or a complete leak check passed unless it actually did.

Preserve Windows/MSYS2 compatibility and document the commands needed for a real Windows run.

## 9. Required documentation

Update:

- `README.md`
- `documentation/architecture.md`
- `documentation/progress.md`
- `documentation/testing.md`
- Any relevant API guide.

Explain what Phase 8 implements and what remains for Phases 9–12.

Include student-friendly explanations of:

- Each new public C function.
- Important structures and pointers.
- `malloc`, `realloc`, `free` and ownership transfers.
- How document URLs are classified.
- How source-page relationships are stored.
- Why this is different from a normal web crawler.
- Why URL deduplication is not the same as file-content deduplication.

## 10. Mandatory prompt history

Preserve the existing folder named exactly:

`prompt history`

Do not delete or rewrite previous phase instructions and handoffs.

Add these Markdown documents:

**2026-11-09-phase8-authoritative-instruction.md**

Store the complete accessible text of this instruction, not a shortened summary.

**2026-11-09-phase8-design-decisions.md**

Explain classification rules, crawler integration, data structures, source tracking, duplicate handling, memory ownership, safety constraints and rejected alternatives.

**2026-11-09-phase8-acceptance-and-handoff.md**

Include a checklist covering every major requirement with PASS, FAIL or NOT VERIFIED, exact commands/results, source changes, bugs fixed, GitHub commit, and readiness for Phase 9.

Do not invent past prompts, test outcomes or timestamps.

## 11. GitHub delivery

After successful implementation and testing:

1. Review the intended changes.
2. Commit only relevant C source/header files, C tests, Makefile and Markdown documentation.
3. Exclude build executables, temporary data, secrets and unrelated Replit scaffolding.
4. Verify remote branch state.
5. Attempt one normal non-force push to the existing GitHub repository, only with available permission.
6. Verify the GitHub remote commit matches the intended local commit.
7. Report the commit SHA and URL.

If authentication fails, report the actual blocker. Do not force-push, retry indefinitely, or claim a successful upload without evidence.

## 12. Mandatory completion response and stop condition

When Phase 8 is complete, report:

1. **COMPLETE, PARTIAL or BLOCKED.**
2. Exact new and modified C files.
3. How document discovery and provenance tracking work.
4. A concrete multi-page document-discovery example.
5. Unit/integration/allocation test results and previous regression results.
6. Compilation, memory-tool and Windows verification status.
7. The exact Markdown files in `prompt history`.
8. Current GitHub commit and verified upload status.
9. Outstanding known bugs or limitations.
10. Explicit **YES or NO** on readiness for Phase 9, with reasons.

**STOP after Phase 8. Do not begin Phase 9 or any later phase without fresh user approval. Do not switch to Power Mode or change paid settings.**

The final deliverable must be an understandable, tested C11 document-discovery feature that preserves the existing working web crawler and proves where each document link was found.

---

**END OF PHASE 8 WORK ORDER.**