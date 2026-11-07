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
```

`make` also builds the pipeline demo. Run `.\build\demo_pipeline.exe` for the default URL or pass a URL argument. The Part 1 test checks invalid arguments, statistics initialization, status messages, and the valid not-implemented response. The URL suite checks normal HTTP/HTTPS parsing, each parsed component, status values, host and port validation, URL-size and DNS label/hostname boundaries, query and fragment handling, normalization, reference resolution, output state on errors, repeated cleanup, and the previously failing resolution/trailing-slash cases.

`make test` includes the original API/URL tests, standalone HTML tests, local
HTTP fixtures, Phase 5 tests, HTTP metadata allocation-failure tests and original live HTTP
tests. The live suite may skip probes or fail when public network access is
blocked. Local fixtures use only a C server bound to loopback on a free port.

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

Current Linux ASan/UBSan commands and measured results are in
`phase4-test-results.md`. LeakSanitizer is restricted in this environment;
allocation accounting is not a complete leak scan. There is no comprehensive
HTML5 parser, fuzzing campaign, or deterministic local TLS fixture.

Standalone Phases 4 and 5 are implemented. BFS (Phase 6) and the integrated
`crawler_run` engine remain unimplemented.

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
