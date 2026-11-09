# Phase 9 acceptance and handoff

**Date:** 2026-11-09
**Scope:** Safe one-document C11 binary downloader only. No Phase 10 or later
implementation was started.

## Acceptance map

| Work-order section | Result | Evidence / limitation |
|---|---|---|
| 1. Objective | PASS | `download_document` streams one recognized PDF, DOCX or ZIP to a file; it does not bulk-download Phase 8 results. |
| 2. Constraints | PASS | New implementation, CLI, fixture and tests are C11. No new package, AI, browser automation, concurrency, deployment or mode/configuration change. The user stated Free was selected; the selector was not independently inspected. |
| 3. Baseline | PASS | Before edits, branch `main`, local HEAD, tracking main and remote main all matched `89e3648f52d9c82c77e91fee68a543f36e4510a7`. Work stayed inside `.local/github-sync/deepdownload-c`. |
| 4. Public API | PASS | Added `include/download.h`; inputs are borrowed during the call, and the caller frees result-owned metadata with `download_result_free`. Existing crawler/document APIs remain source-compatible. |
| 5. Streaming and size cap | PASS | C callback writes exact bytes using checked `fwrite`, checks `size * count` and cumulative size, and enforces 20 MiB inclusive. The local test verifies embedded NUL/high bytes and exact / +1 limits with Content-Length and chunked framing. |
| 6. HTTP/MIME validation | PASS | Tests accept PDF/DOCX/ZIP, ZIP alternate MIME parameters and the documented single `application/octet-stream` fallback; reject HTML, wrong/missing/duplicate MIME, empty, 206, 4xx/5xx and incomplete bodies. MIME is not content validation. |
| 7. Redirect safety | PASS | Manual bounded loop resolves and checks each target before requesting it; HTTP(S)-only, five redirects, host/seed-port scope, no HTTPS downgrade. Blocked host/port target request counts are zero. This is not an IP/DNS firewall. |
| 8. Filesystem safety | PASS | Basenames, extension, directory and Windows reserved-name checks; exclusive create; existing file preserved. Injected write/close/remove failures return errors and remove only this run's file. Real permission-denied behavior was not separately exercised. |
| 9. Phase 8 integration boundary | PASS | Reuses `document_type_from_url`; Phase 8 discovery/provenance APIs are unchanged and no automatic result downloading was added. |
| 10. CLI | PASS | `build/download-document` was fork/exec'd by the C local integration test. It saved the fixture's 8-byte binary payload; the test compared every byte. |
| 11. Deterministic tests | PASS | New unit, local integration and allocation/file-I/O suites all passed; exact counts and commands are below. |
| 12. Earlier phases | PASS | Full `make test` passed. Existing tests were retained; only the Makefile was extended and the shared HTTP scope predicate was factored for reuse. |
| 13. Build and QA | PASS with checks NOT VERIFIED noted | Clean Linux build, full suite, strict warnings and targeted ASan/UBSan passed. Leak-enabled execution failed in the environment; native Windows was not available. |
| 14. Documentation | PASS | README, API, architecture, progress and testing documents now describe Phase 9 without claiming Phase 10 work. |
| 15. Prompt history | PASS | The full Phase 9 order, design decisions and this acceptance/handoff are retained in `prompt history/`. Earlier files remain untouched. |
| 16. GitHub delivery | PASS | Implementation commit `6f4a3028b4cdd953efe38606938defca5003183a` was pushed normally to `main`; immediately afterward both the Replit GitHub proxy and public GitHub reported that SHA. This document is a docs-only receipt following that implementation commit. |
| 17. Final handoff | PASS | Status, ownership, commands, limitations, Git state and Phase 10 readiness are recorded here and in the final response. |

## What changed

- `include/download.h` — public configuration, result, status values and the
  one-document download/free functions.
- `src/download.c` — URL/type/name validation, manual redirect loop, checked
  libcurl setup, binary callback, size/MIME/status checks, exclusive file
  creation and cleanup.
- `src/http.c`, `src/http_internal.h`, `src/http_scoped.c` — a private
  libcurl-initialization helper and reusable host/port/TLS scope predicate.
  The crawler's public API and standalone `http_download` behavior are retained.
- `examples/download_document.c` — C CLI for one chosen URL.
- `tests/download_fixture.c`, `tests/download_fixture.h` — deterministic
  loopback server with binary, MIME, redirect, error, timeout and size routes.
- `tests/test_download.c` — API, invalid-input, filename and pure scope-policy
  cases.
- `tests/test_download_integration.c` — local HTTP, filesystem, byte, redirect,
  timeout and real CLI checks.
- `tests/test_download_alloc.c` — test-only allocator plus `fwrite`, `fclose`
  and `remove` fault injection.
- `Makefile` — library, CLI and test integration; test wrappers are built as
  separate objects.
- `README.md`, `documentation/api.md`, `documentation/architecture.md`,
  `documentation/progress.md`, `documentation/testing.md` — current behavior,
  beginner-facing ownership and testing guidance.
- `prompt history/2026-11-09-phase9-authoritative-instruction.md` — complete
  accessible work order. Its line endings were normalized only; text was
  compared against the uploaded source after removing CR characters.
- `prompt history/2026-11-09-phase9-design-decisions.md` — design choices,
  tradeoffs and known scope limitations.

No build output, downloaded document, secret, credential or outer Replit
React/TypeScript scaffold was included in the C repository commit.

## Beginner-facing API and ownership

- `download_result_init(result)` accepts an empty, zero-initialized
  `DownloadResult`; it does not allocate.
- `download_document(config, result)` borrows all input strings for the call.
  It first uses Phase 8's `document_type_from_url` to accept only `.pdf`,
  `.docx` or `.zip` paths. The output directory must already exist.
- `DownloadResult.final_url` and, on success, `file_path` are heap-owned
  strings. On an ordinary error the partial file is removed and `file_path` is
  NULL. If removal itself fails, `DOWNLOAD_ERROR_CLEANUP` is returned and the
  owned path remains so the leftover can be found.
- `bytes_written` records bytes accepted by `fwrite`; it can be nonzero on an
  interrupted/error result. `http_status` and `final_url` describe the last
  fetched hop, not a blocked target.
- `download_result_free(result)` releases the owned strings and resets the
  result; it is safe to call repeatedly. The caller also calls the existing
  `http_cleanup()` once after its process is finished with HTTP work.
- The callback receives a pointer and a byte count, not a C string. It writes
  bytes directly through `FILE *`, checks short writes and caps total size at
  20 MiB. This preserves `0x00` and high-valued bytes without allocating a
  document-sized RAM buffer.
- The destination is exclusively created and never overwrites an existing
  file. It is visible during the transfer, so the implementation does not
  claim atomic publication. Failures close and remove only the file created by
  this invocation.

The CLI form is:

```text
build/download-document DOCUMENT_URL SEED_URL OUTPUT_DIRECTORY SAFE_FILENAME SAME_DOMAIN [TIMEOUT_SECONDS]
```

The timeout is 1–3600 seconds per request hop (default 3 in the CLI). Both
scope modes pin the seed's effective port; `SAME_DOMAIN=1` also requires the
exact host. HTTPS downgrade and non-HTTP(S) redirect schemes are rejected.
This does not prevent DNS rebinding, private-IP resolution of an otherwise
permitted hostname, or replace robots/access policy. Use only authorized sites.

## Exact Linux verification

Environment: Linux Replit/Nix, GCC, libcurl 8.14.1. All application and
test logic is C11. The full commands were:

```sh
make clean
make
make test
```

Observed exits: `make clean=0`, `make=0`, `make test=0`. The full regression
run emitted no compiler warning or test failure. The original network-dependent
HTTP suite also ran and reported **7/7 passed**; new Phase 9 integration cases
contacted only the C loopback fixture.

New Phase 9 suite output:

| Command / suite | Outcome |
|---|---|
| `make test-download` | Exit 0; 140 checks, 0 failures. |
| `make test-download-integration` | Exit 0; 277 checks, 0 failures. Includes the real C CLI invocation and byte-for-byte fixture comparison. |
| `make test-download-alloc` | Exit 0; 159 checks, 0 failures; 22 injected application allocation positions. |

The complete `make test` also reported:

- Queue 3,667 checks; visited set 3,670 checks.
- Phase 5 allocation suite 3,281 checks; zero tracked allocations.
- Crawler engine 372 checks; 25 site scenarios.
- Crawler allocation suite 223,355 checks; 331 failure positions.
- HTTP metadata allocation suite: 3 metadata-copy OOM cases and cleanup/reuse.
- Phase 7 safety 303 checks; Phase 7 allocation suite 271,334 checks and 727
  allocation positions.
- Phase 8 document unit 41 checks, integration 127 checks, and allocation
  suite 528,140 checks across 719 injected positions.
- The standalone HTML suite printed 14 passing scenario groups; it does not
  print one aggregate assertion count.
- The legacy HTTP downloader reported all 7/7 test groups passed.

Strict compilation and new-target command:

```sh
make clean
make CFLAGS="$(pkg-config --cflags libcurl) -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude"
make CFLAGS="$(pkg-config --cflags libcurl) -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude" test-download test-download-integration test-download-alloc
```

The strict build and targeted suites exited 0 with no warnings. Targeted
ASan/UBSan build and execution also exited 0 using:

```sh
make clean
ASAN_OPTIONS=detect_leaks=0:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 make CFLAGS="$(pkg-config --cflags libcurl) -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude -fsanitize=address,undefined -fno-omit-frame-pointer" LDFLAGS="$(pkg-config --libs-only-L libcurl) -fsanitize=address,undefined" LDLIBS="$(pkg-config --libs-only-l libcurl)" test-download test-download-integration test-download-alloc
```

Leak detection was attempted separately:

```sh
ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 make test-download test-download-integration test-download-alloc
```

It exited 2 after the unit checks with LeakSanitizer's fatal environment
message: `LeakSanitizer has encountered a fatal error` and
`LeakSanitizer does not work under ptrace`. Therefore this is **not** a
successful process-wide leak scan. The deterministic allocator accounting
tests passed; they do not replace a full leak tool.

`nm -g --defined-only build/libcrawler.a` found no
`download_test_malloc/realloc/free/fwrite/fclose/remove` symbols. The C-only
project inventory found no Python, JavaScript, TypeScript, C++ or Rust files.
Native Windows/MSYS2 execution is **NOT VERIFIED**; POSIX local fixtures
explicitly skip there. No Windows result is claimed.

## GitHub receipt

- Repository: `https://github.com/jatinvimal1506/Web-Crawler-and-Scraper-in-C`
- Branch: `main`
- Phase 9 implementation commit: `6f4a3028b4cdd953efe38606938defca5003183a`
- Parent before push: `89e3648f52d9c82c77e91fee68a543f36e4510a7`
- Remote was rechecked before push and matched the parent; no divergence was
  overwritten.
- One normal, non-force push succeeded. Immediately afterward both
  `origin/main` through Replit's GitHub proxy and public GitHub `main` returned
  `6f4a3028b4cdd953efe38606938defca5003183a`.
- This acceptance document is a Markdown-only receipt commit after the
  implementation commit. The final branch head after that receipt is stated
  in the final Replit response.

## Limitations and readiness

- No native Windows runtime, deterministic TLS handshake, DNS-rebinding test,
  content-structure parser, resume/retry logic or atomic file publication.
- Real permission-denied behavior was not exercised; write, close and cleanup
  failures were injected deterministically.
- Phase 9 downloads only one URL per call. It does not automatically save all
  `DocumentResults`, create type-based folders, hash/deduplicate file content,
  parse document formats, filter keywords or emit CSV.
- **Phase 10 readiness: YES, technically.** The Phase 9 API, integration,
  regressions and local tests pass. This is not authorization to begin Phase 10;
  that phase requires a separate user instruction.
- **NO PHASE 10 OR LATER STARTED.** No mode change, paid feature, deployment or
  publication was made.
- No agent-mode setting was read or changed. The latest user confirmation was
  that Free is selected; the active UI selector was not independently verified.
