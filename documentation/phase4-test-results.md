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
| New HTML suite | PASS: all 12 groups, including unquoted query/boundary regressions, truncations and 200-link growth. |
| New local HTTP suite | PASS: 78 checks against the real public APIs, using C-only loopback fixtures. |
| Original HTTP suite | PASS: all 7 groups; live example.com HTTPS returned 200 (577 bytes) and the 404 assertion ran. |
| `make test-html-alloc` | PASS, exit 0: all 143 allocation-failure points; empty output and zero tracked outstanding allocations after each. |
| Strict compilation, all three new test executables | PASS: C11, `-Wall -Wextra -Wpedantic -Werror`. |
| ASan + UBSan, HTML suite | PASS with leak detection disabled; 12 groups, no sanitizer diagnostics. |
| ASan + UBSan, allocation suite | PASS with leak detection disabled; 143 failure points, no sanitizer diagnostics. |
| ASan + UBSan, local HTTP suite | PASS with leak detection disabled; 78 checks, no sanitizer diagnostics. |
| LeakSanitizer | Previously observed unavailable due to ptrace restrictions; not rerun or claimed passing in this correction pass. |
| Windows Makefile branch | DRY-RUN ONLY: retained cmd.exe, `.exe`, Windows cleanup/mkdir and executable paths. POSIX local fixtures explicitly print SKIP on native Windows. |
| `git diff --check` | PASS. |
| Preserved source/API check vs GitHub main | URL/project/queue/visited, original public headers, demos and original tests unchanged. Only the authorized HTTP implementation is corrected. |

No compiler warnings/errors or assertion/test failures were observed in the
clean build, full test run or strict sanitizer runs. The original crawler API
tests are silent on success. The original HTTP tests can behave differently on
other machines because they use real network access. Local fixtures need only
loopback networking, not external services.

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
