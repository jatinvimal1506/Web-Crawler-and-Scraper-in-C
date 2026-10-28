# Part 1 and Part 2 URL Test Report

## 1. Project information

- **Project:** Web Crawler and Scraper in C
- **Tested modules:** Part 1 API/CLI foundation; Part 2 URL parsing, validation, normalization, resolution, and ownership
- **Test date:** 2026-10-28 (Asia/Kolkata)
- **Environment:** Windows PowerShell; MSYS2 UCRT64 GCC 14.1.0; mingw32-make 4.4.1; C11
- **Scope:** No HTTP, HTML, queue, visited-set, or crawler integration was implemented or tested.

## 2. Commands and outcomes

Executed from the project directory:

```powershell
mingw32-make clean
mingw32-make
mingw32-make test
.\build\test-url.exe
```

All returned exit code 0. The clean C11 build compiled the static library, demo, and test programs using `-Wall -Wextra -Wpedantic` without compiler warnings. `make test` ran `test-crawler.exe` and `test-url.exe`; both passed. The URL executable also passed when run independently.

Two temporary deterministic C harnesses were compiled under `%TEMP%` against the current URL implementation. The first ran 21 named probes, all passed. The second ran four limit/path probes, all passed. Seven CLI probes were repeated: six passed; the FTP CLI case is not applicable because the Part 1 placeholder does not validate URL syntax. The additional scenario probes therefore total 32: **31 PASS, 0 FAIL, 1 N/A**. The permanent URL test program additionally contains the regressions listed below.

## 3. Detailed supplemental test results

| ID | Category | Input/scenario | Expected | Actual | Result |
|---|---|---|---|---|---|
| N1 | Normal | `http://example.org/path` | HTTP scheme, host and path parsed | Correct components | PASS |
| N2 | Normal | `HTTPS://Example.COM:443/a?q=1#f` | HTTPS components parsed; scheme/host lowercase | Correct components and optional fields | PASS |
| B1 | Boundary | Ports 1, 65535, 0, 65536 | 1 and 65535 accepted; others rejected | As expected | PASS |
| I1 | Invalid | FTP, malformed separators, missing host, invalid host character | Reject invalid/unsupported URL | All rejected | PASS |
| E1 | Empty | `https://a.test/?` and `https://a.test/#` | Empty query/fragment remain present empty strings | As expected | PASS |
| E2 | Empty | URL with no explicit path | Path becomes `/` | `/` | PASS |
| B2 | Boundary | URL lengths 2047, 2048, 2049 | Accept through 2048; reject above | As expected | PASS |
| B3 | Boundary | DNS hostname lengths 253, 254 | 253 accepted; 254 rejected | As expected | PASS |
| B4 | Boundary | Label lengths 63, 64 | 63 accepted; 64 rejected | As expected | PASS |
| N3 | Normal | Mixed case, default port, dot segments, query, fragment | Canonical host/scheme/path; query retained; fragment dropped | `http://example.com/a/c?q=1` | PASS |
| B5 | Boundary | Resolve `../../../../x` from nested path | Clamp at host root | `https://example.com/x` | PASS |
| N4 | Normal | Query-only `?q=2` | Keep base path and apply new query | `https://example.com/a/page.html?q=2` | PASS |
| E3 | Empty/reference | `#part` against `/a/page.html?old=1` | Keep base query; omit fragment identity | `https://example.com/a/page.html?old=1` | PASS |
| E4 | Empty/reference | Empty relative reference | Base URL, minus fragment | Base path and query retained | PASS |
| F1 | Failure | Invalid base URL | Specific format error; output NULL | `URL_ERROR_INVALID_FORMAT`; NULL | PASS |
| F2 | Failure | Malformed absolute reference `http://[bad` | Format error; output NULL | `URL_ERROR_INVALID_FORMAT`; NULL | PASS |
| F3 | Failure | NULL normalize/resolve output parameter | Invalid-argument status | `URL_ERROR_INVALID_ARGUMENT` | PASS |
| I2 | Invalid | NULL/empty parse input and NULL result | Appropriate error status, no crash | Invalid-argument/format statuses | PASS |
| M1 | Memory | NULL cleanup, zeroed cleanup, repeated cleanup | Safe and idempotent for cleared result | No crash | PASS |
| M2 | Memory | 10,000 parse/free/normalize/relative-resolve iterations | Complete without crash | All iterations completed | PASS |
| N5 | Normal | Valid Part 1 crawler config | Placeholder status and zero stats | `CRAWLER_NOT_IMPLEMENTED`; stats zeroed | PASS |
| D1 | Large | 2047-byte URL with a large query | Accept | Accepted | PASS |
| D2 | Large | 2048-byte URL with a large query | Accept at cap | Accepted | PASS |
| D3 | Large/failure | 2048-character relative value expands beyond URL limit | Too-long error and NULL output | `URL_ERROR_TOO_LONG`; NULL | PASS |
| D4 | Normalization | Normalize `https://a.test/a/` | Preserve trailing slash | `https://a.test/a/` | PASS |
| CLI1 | Invalid CLI | No arguments | Usage error, nonzero exit | Usage; exit 2 | PASS |
| CLI2 | Empty CLI | Empty start URL argument | Usage/error, nonzero exit | Usage; exit 2 | PASS |
| CLI3 | Invalid CLI | Depth `abc` | Clear error, nonzero exit | Error; exit 2 | PASS |
| CLI4 | Invalid CLI | Depth `-1` | Clear error, nonzero exit | Error; exit 2 | PASS |
| CLI5 | Invalid CLI | Extra argument | Usage error, nonzero exit | Usage; exit 2 | PASS |
| CLI6 | Normal | `https://example.org 2` | Explain placeholder behavior; exit 0 | Placeholder message; exit 0 | PASS |
| CLI7 | Scope | `ftp://example.org 1` passed to Part 1 CLI | URL syntax is not validated by Part 1 stub | Placeholder; exit 0; direct URL parser rejects FTP | N/A |

### Permanent URL regression coverage

The updated `tests/test_url.c` now permanently checks:

- Fragment-only resolution retains the base query; empty-reference resolution preserves base path and query.
- Trailing slash, `/a/.`, `/a/b/..`, `/a/../`, and repeated separators `/a//b/` normalization.
- Absolute HTTP and HTTPS references, protocol-relative, root-relative, directory-relative, query-only, fragment-only, and deep parent references.
- 10,000 absolute-reference resolutions to exercise the former early-return leak path.
- Default-port removal, duplicate-equivalent canonical URLs, valid percent-escape preservation, and malformed percent rejection.
- Public error status distinctions, status messages, output NULL guarantees, and rejection of a populated parse result without losing its existing pointers.
- Port, full URL, hostname, DNS label, large-query, and empty-component boundaries.

## 4. Previous failures and fixes

| Previous ID | Original issue and root cause | Code change | Regression input | Expected result | Actual result |
|---|---|---|---|---|---|
| E3 | Fragment-only branch assembled from base path but omitted base query. | Resolver now retains the parsed base query when appending a fragment reference; normalization removes the fragment. | Base `https://example.com/a/page.html?old=1#oldfrag`, reference `#part` | `https://example.com/a/page.html?old=1` | PASS |
| E4 | Empty reference fell into directory-relative handling and returned the containing directory. | Empty reference now normalizes the base itself. | Base `https://example.com/a/page.html?old=1#frag`, reference empty | `https://example.com/a/page.html?old=1` | PASS |
| D4 | Previous path tokenization discarded trailing slash and repeated separators. | Dot-segment removal now moves path segments while preserving non-dot separators and terminal slash semantics. | `/a/`, `/a/.`, `/a/b/..`, `/a/../`, `/a//b/` | `/a/`, `/a/`, `/a/`, `/`, `/a//b/` | PASS |
| M3 | Absolute-reference fast path returned immediately after parsing base, skipping cleanup. | The fast path now exits through shared cleanup for the parsed base and intermediates. | Resolve `https://other.test/x` against `https://example.com/base/`, repeated 10,000 times | Correct URL each time; all temporary base allocations released by cleanup path | PASS by regression and source review; leak checker unavailable |

## 5. Error handling review

The URL API now returns `UrlStatus` consistently:

| Status | Meaning |
|---|---|
| `URL_OK` | Success |
| `URL_ERROR_INVALID_ARGUMENT` | Missing required pointer or already-populated parse output |
| `URL_ERROR_INVALID_FORMAT` | Malformed URL/reference |
| `URL_ERROR_UNSUPPORTED_SCHEME` | Scheme other than HTTP/HTTPS |
| `URL_ERROR_INVALID_HOST` | Missing or invalid ASCII DNS host |
| `URL_ERROR_INVALID_PORT` | Empty, nonnumeric, zero, or out-of-range port |
| `URL_ERROR_TOO_LONG` | More than 2048 bytes |
| `URL_ERROR_OUT_OF_MEMORY` | Allocation failure |

`url_status_message()` maps these statuses to readable static strings. Parse, normalize, and resolve failures return a code; output strings are NULL on failure whenever the output parameter exists. `url_parse` requires a zero-initialized `ParsedUrl` and rejects non-empty output rather than overwriting its allocations. It constructs into a temporary structure and cleans partial component allocations on failure. Part 1 `crawler_run()` retains its own status API and placeholder behavior.

Invalid-input and failure-path checks passed. Allocation failure could not be induced deterministically, so that branch has been reviewed but not forced at runtime.

## 6. Memory-management review

`url_parse` returns individually allocated components owned by the caller; release them with `url_free`. Initialize output as `{0}`. A populated structure is rejected, preserving the existing allocations. `url_free(NULL)` is safe, and `url_free` clears fields after freeing, allowing a second cleanup of that cleared object.

`url_normalize` and `url_resolve` return heap strings owned by the caller, released with `free`. Both initialize valid output parameters to NULL before work. Source review confirmed the absolute-reference path and common failure exits reach cleanup for parsed base structures and intermediate buffers. Regression loops passed, but they do not prove leak freedom.

AddressSanitizer compilation was attempted; the installed MSYS2 GCC could not link because `-lasan` is missing. Valgrind is unavailable. Allocation-failure injection was not performed. Independent leak freedom is therefore **not proven**.

## 7. Seven assignment testing categories

| Category | Coverage/result |
|---|---|
| Normal | HTTP/HTTPS components; normalization; absolute, root, directory, query, fragment, protocol-relative, and empty references; Part 1 placeholder. Passed. |
| Boundary | Ports, 2048-byte URL limit, hostname and label limits, deep parent traversal. Passed. |
| Invalid input | NULL, malformed, unsupported scheme, invalid host/port/characters/percent escape and relative values. Passed. |
| Empty input | Empty URL/query/fragment/reference and missing path/CLI args. Passed. |
| Large input | Maximum and over-limit URL/query; oversized resolved reference. Passed. |
| Failure conditions | Typed errors, invalid output/base, malformed absolute reference, NULL outputs, CLI usage errors. Passed. |
| Memory management | NULL/empty cleanup, populated result protection, 10,000 repeated operations, source path review. Partial: no sanitizer, Valgrind, or forced OOM run. |

## 8. Remaining limits and conclusion

The suite does not test network behavior because Parts 3–6 are placeholders. Allocation-failure injection, fuzzing, ASan, and Valgrind remain unavailable. URL handling is intentionally limited to ASCII DNS names and common HTTP(S) references; IPv6, IDNs, and exhaustive RFC conformance are out of scope.

All four reported defects now have permanent regression coverage and pass. The build and Part 1 tests remain passing. Functional Part 2 verification is successful within the documented scope, but independent leak checking and OOM-path runtime verification remain incomplete.

**Final status: Part 2 Implemented, Verification Incomplete.**

