# Phase 4 verification results

Observed after the targeted Phase 4 and authorized HTTP corrections in Replit
Linux with GCC 16.2.0 and libcurl 8.14.1. These are execution results, not
predictions. No native Windows runtime or browser testing is claimed.

## Builds and tests

| Check | Observed result |
| --- | --- |
| `make clean` | PASS, exit 0. |
| `make` | PASS, exit 0: static library and both original CLI demos built. |
| `timeout 120 make test` | PASS, exit 0: original API/URL tests, HTML tests, local HTTP fixtures and original HTTP suite. |
| New HTML suite | PASS: all 14 groups, including encoded query delimiters, raw-text exclusions, malformed closings, truncations and 200-link growth. |
| New local HTTP suite | PASS: 81 checks against the real public APIs, using C-only loopback fixtures. |
| `make test-http-alloc` | PASS: 128 total fixture checks, including 3 injected metadata-copy failures, absent-header success, cleanup and reuse. |
| Original HTTP suite | PASS: all 7 groups; live example.com HTTPS returned 200 (577 bytes) and the 404 assertion ran. |
| `make test-html-alloc` | PASS, exit 0: all 143 allocation-failure points; empty output and zero tracked outstanding allocations after each. |
| Strict compilation, all four new test executables | PASS: C11, `-Wall -Wextra -Wpedantic -Werror`. |
| ASan + UBSan, HTML suite | PASS with leak detection disabled; 14 groups, no sanitizer diagnostics. |
| ASan + UBSan, allocation suite | PASS with leak detection disabled; 143 failure points, no sanitizer diagnostics. |
| ASan + UBSan, local HTTP suite | PASS with leak detection disabled; 81 checks, no sanitizer diagnostics. |
| ASan + UBSan, HTTP allocation suite | PASS with leak detection disabled; 128 total checks, no sanitizer diagnostics. |
| LeakSanitizer | Previously observed unavailable due to ptrace restrictions; not rerun or claimed passing in this correction pass. |
| Windows Makefile branch | DRY-RUN ONLY: retained cmd.exe, `.exe`, Windows cleanup/mkdir and executable paths. POSIX local fixtures explicitly print SKIP on native Windows. |
| `git diff --check` | PASS. |
| Preserved source/API check | URL/project/queue/visited, demos and original tests unchanged. HTTP public signatures/enums are unchanged; header comments document metadata OOM cleanup. |

No compiler warnings/errors or unexpected assertion/test failures were observed
in the clean build, full test run or strict sanitizer runs. Exact commands were:
`make clean`, `make`, `timeout 120 make test`, `make test-html-alloc`, and
`make test-http-local test-http-alloc`; all exited 0. The original crawler API
tests are silent on success. The original HTTP tests can behave differently on
other machines because they use real network access. Local fixtures need only
loopback networking, not external services.

## New defect-specific regression evidence

- Failed final-URL metadata allocation: returns `HTTP_ERR_OUT_OF_MEMORY`, with
  body/final_url/content_type NULL, size/status zero and a nonempty diagnostic.
- Failed present Content-Type allocation: same consistent OOM state, including
  freeing the earlier final-URL copy and received body.
- Failed final-URL allocation with no Content-Type header: same OOM behavior.
- Missing Content-Type without an allocation failure: valid HTML still succeeds
  using the existing heuristic, with content_type NULL.
- HTTP allocation accounting is zero immediately on each injected failure,
  after repeated free, and after a successful retry using the same structure.
- As a negative control, the new HTTP OOM regression was also linked against
  the previous committed HTTP source in a temporary object. It exited 1 at the
  expected OOM assertion, proving it detects the old false-success bug.
  This deliberate failure is not a failure of the corrected implementation.
- Raw-text tests cover iframe/xmp/noembed/noframes as well as the original four
  exclusions, malformed closing tags and plaintext-to-end behavior.
- Unquoted decimal/hex/named question marks now match quoted/literal output;
  adjacent attributes, duplicates and unsafe schemes remain covered.

## New local HTTP coverage and remaining limits

- Relative href extraction uses the final page URL after a real HTTP redirect.
- Valid HTML and XHTML MIME tokens accept case variations and parameters.
- `application/x-text/html-evil`, XHTML suffix lookalikes and a parameter-only
  `text/html` mention are rejected.
- Binary PDF and missing-type binary responses return `HTTP_ERR_NOT_HTML`.
- File/FTP redirects fail with protocol-rejection diagnostics, not merely a
  missing-file or failed-connection error.
- A redirect loop still reaches the existing five-hop limit.
- Exactly 5,242,880 response bytes succeed. One byte over fails both with
  Content-Length and without it, covering the streaming write callback.
- Local fixtures are plain HTTP. There is no local HTTPS/certificate fixture;
  original live HTTPS still passes and TLS verification settings are unchanged.
- This is not universal binary sniffing: a server falsely declaring binary as
  `text/html` is still trusted by the existing downloader policy.
- The extractor remains a NUL-terminated-string API, without an independent
  5 MiB parser input limit, a full HTML5 tokenizer, or complete Unicode entities.

## Initial environment/build issues, resolved

1. The initial environment lacked `curl/curl.h`. Installing the curl and
   pkg-config system packages supplied the development files.
2. Environment `CFLAGS` hid the original Makefile's `?=` default flags, so
   `project.h` was not found. Appending the project's C11/warning/include flags
   retains both project flags and environment dependency paths.
3. The original link recipes ignored environment `LDFLAGS`, so `-lcurl` could
   not be located. Link recipes now honor `LDFLAGS`.

No existing C module had to be changed to resolve these build issues.

## Reproduce the additional sanitizer checks

From `DeepDownload-C/` after `make`:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror \
  -fsanitize=address,undefined -fno-omit-frame-pointer -g -Iinclude \
  tests/test_html.c src/html.c src/url.c -o build/test-html-sanitized
ASAN_OPTIONS=detect_leaks=0 ./build/test-html-sanitized

cc -std=c11 -Wall -Wextra -Wpedantic -Werror \
  -fsanitize=address,undefined -fno-omit-frame-pointer -g -Iinclude \
  tests/test_html_alloc.c src/html.c src/url.c \
  -Wl,--wrap=malloc -Wl,--wrap=realloc -Wl,--wrap=free \
  -o build/test-html-alloc-sanitized
ASAN_OPTIONS=detect_leaks=0 ./build/test-html-alloc-sanitized

cc $CFLAGS -std=c11 -Wall -Wextra -Wpedantic -Werror \
  -fsanitize=address,undefined -fno-omit-frame-pointer -g -Iinclude \
  tests/test_http_local.c src/http.c src/html.c src/url.c \
  $LDFLAGS -lcurl -o build/test-http-local-sanitized
ASAN_OPTIONS=detect_leaks=0 ./build/test-http-local-sanitized

cc $CFLAGS -std=c11 -Wall -Wextra -Wpedantic -Werror \
  -fsanitize=address,undefined -fno-omit-frame-pointer -g -Iinclude \
  -Dmalloc=http_test_malloc -Drealloc=http_test_realloc \
  -Dcalloc=http_test_calloc -Dfree=http_test_free \
  -c src/http.c -o build/http-alloc-sanitized.o
cc $CFLAGS -std=c11 -Wall -Wextra -Wpedantic -Werror \
  -fsanitize=address,undefined -fno-omit-frame-pointer -g -Iinclude \
  -DHTTP_ALLOC_TEST tests/test_http_local.c build/http-alloc-sanitized.o \
  src/html.c src/url.c $LDFLAGS -lcurl -o build/test-http-alloc-sanitized
ASAN_OPTIONS=detect_leaks=0 ./build/test-http-alloc-sanitized
```

Leak detection was first attempted with `detect_leaks=1`; it failed at process
exit due to the environment restriction. Disabling that one unsupported check
allows the address/undefined-behavior checks to finish. Independently, the
allocation test wraps malloc/realloc/free from the HTML and URL objects and
counts outstanding allocations, asserting zero after success cleanup and
every simulated failure. That is a targeted cleanup test, not a claim of an
unrestricted whole-program leak scan.

The generated executables stay in `build/`, already ignored by the imported
repository.
