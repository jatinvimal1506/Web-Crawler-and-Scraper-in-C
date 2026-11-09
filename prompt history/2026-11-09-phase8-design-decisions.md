# Phase 8 design decisions

**Date:** 2026-11-09
**Scope:** public PDF/DOCX/ZIP link discovery and source-page provenance only.

## Keep the existing crawler as the engine

Phase 8 adds `crawler_run_with_link_observer`, an additive API in the existing
C crawler module. The observer receives a resolved, normalized, in-scope URL
before the engine decides whether to put it on the FIFO page queue. The
document module recognizes a supported extension, records the link, and sets
`crawl_link` to zero. All other HTTP(S) links continue through the unchanged
breadth-first engine.

This was preferred over copying the BFS loop into `document.c`: one engine
continues to own page fetching, depth, scope, redirect, and resource-limit
rules. The original `crawler_run` calls the same internal engine without an
observer, so its Phase 6 behavior remains unchanged. The observer sees
successful parsed pages only, and the source URL is the effective final page
URL after a permitted redirect.

## Identify documents from the URL only

`document_type_from_url` uses the project's `url_parse` implementation and
checks the last suffix in the parsed path, case-insensitively:

- `.pdf` → PDF
- `.docx` → DOCX
- `.zip` → ZIP
- any other valid path → UNKNOWN

Query strings and fragments do not affect classification. No request, file
content inspection, response-header guess, or speculative extension inference
is performed. This is deliberately only link discovery, not file downloading.
An unknown extension remains an ordinary crawler candidate and is handled by
the existing HTML-only downloader.

## Canonical keys and provenance

Document and source-page URLs are copied through the existing `url_normalize`
function. That keeps Phase 8 aligned with crawler identities: scheme and host
case, default ports, and fragments follow existing library behavior. Distinct
paths or meaningful queries remain distinct; Phase 8 does not add a second
URL parser or alter URL normalization rules.

`DocumentResults.items` preserves first-discovery order. A document URL occurs
once; its owned source array contains one entry for each distinct normalized
source-page URL. Each source entry also stores the page's BFS depth. A repeated
link from the same source, including a fragment-only variant that normalizes
to the same key, does not add another pair. A document discovered from another
page adds a new provenance entry without duplicating the document record.

## Simple bounded storage

The public results are dynamic C arrays, not a hash table or a new package.
Lookup scans the current result records and their source arrays, so a lookup
is O(n) and a full accumulation can be O(n²). This is acceptable for the
existing 100-page / 1000-candidate crawl limits and is easier for a C student
to inspect. `DOCUMENT_MAX_OCCURRENCES` bounds unique document/source pairs at
1000. Capacity multiplication and allocation-size arithmetic are checked.

The order of alternatives rejected:

1. **Hash table:** faster lookup but adds collision, bucket, and ownership
   mechanics unnecessary at the current explicit limit.
2. **Download to identify files:** outside Phase 8, creates binary and
   response-size handling that belongs to a later approved phase.
3. **Guess from MIME or a filename query parameter:** less deterministic and
   can misclassify links; only the parsed URL path suffix is accepted.

## Ownership and errors

The caller starts results as `{0}` or initializes an empty result. On success,
the results object owns the top-level array, each canonical document URL, each
source array, and every canonical source-page string. Accessor pointers are
borrowed and are invalid after `document_results_free` or a later operation
that changes the result. `document_results_free` releases all nested
allocations and resets the object.

Each new record is fully prepared before it is published in the array. An
allocation or size-overflow failure in the observer stops the BFS. OOM and
overflow clear all partial results so no partial record is mistaken for a
complete result. A normal child-page error returns `DOCUMENT_PARTIAL` and
retains successful discoveries; a crawler budget returns
`DOCUMENT_LIMIT_REACHED` with its bounded discoveries retained. Root/config
errors remain errors. The existing crawler owns and releases its queue,
visited sets, HTTP responses, and HTML links.

The allocation test compiles all application C modules into separate
test-only objects that route `malloc`, `realloc`, and `free` through a
deterministic failure/counting wrapper. These hooks are not linked into the
release static library.

## Validation choices and boundaries

New executable tests and fixtures are C11. The deterministic integration
suite extends the project's existing C loopback server instead of contacting
public websites. It checks depth, redirects/final URL bases, scope, duplicate
normalization, distinct queries, partial failures, the result ceiling, and
that document URLs are not fetched.

The normal `crawler_run` API and all Phases 1–7 tests remain in the regression
suite. Native Windows/MSYS2 execution is not available in this environment;
the Makefile retains its existing Windows branch, but Windows results must
not be claimed from Linux. No Phase 9 binary downloading, file saving,
hashing, reporting, or other later feature is included.
