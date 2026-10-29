# Part 1 and Part 2 Test Report

## 1. Project information

- **Project:** Web Crawler and Scraper in C
- **Tested scope:** Part 1 API/CLI foundation and Part 2 URL parsing, validation, normalization, resolution, and ownership
- **Testing date:** 2026-10-28 (Asia/Kolkata)
- **Environment:** Windows PowerShell; MSYS2 UCRT64 GCC 14.1.0; mingw32-make 4.4.1; C11; repository at `C:\Manthan\Three muskteers\web-crawler-c`
- **Source changes:** None. Only this report was added. Additional probes were compiled from temporary C harnesses under `%TEMP%` and were not added to the repository.

## 2. Testing overview

Executed, in order:

```powershell
mingw32-make clean
mingw32-make
mingw32-make test
.\build\test-url.exe
```

All four commands completed with exit code 0. `make test` ran `test-crawler.exe` and `test-url.exe`; the URL executable was also run independently and passed. The build compiled the Part 1/2 library, demo, and tests successfully with `-std=c11 -Wall -Wextra -Wpedantic`.

Temporary standalone harnesses compiled against `src/url.c` and `src/project.c` exercised additional edge cases. The first harness ran 21 checks (19 passed, 2 failed). A second harness ran 4 checks (3 passed, 1 failed). Seven CLI argument probes were run (six passed for expected Part 1 behavior; one was not applicable as URL syntax validation is not part of the Part 1 stub). Thus, **32 explicit additional scenario probes** produced 28 pass, 3 fail, and 1 not applicable. Existing test suites passed too; suite invocations are reported separately from scenario counts.

## 3. Detailed test results

| Test ID | Category | Input / Scenario | Expected Result | Actual Result | Status |
|---|---|---|---|---|---|
| N1 | Normal | `http://example.org/path` | HTTP scheme, host, path parsed | Parsed correctly | PASS |
| N2 | Normal | `HTTPS://Example.COM:443/a?q=1#f` | HTTPS components parsed; scheme and host lowercase | All checked components matched | PASS |
| B1 | Boundary | Ports 1, 65535, 0, 65536 | Endpoints accepted; out-of-range rejected | 1 and 65535 accepted; 0 and 65536 rejected | PASS |
| I1 | Invalid | FTP, malformed separators, missing host, hostname with space | Reject unsupported/malformed URL | All rejected | PASS |
| E1 | Empty | Empty query and empty fragment | Present as allocated empty strings | Both parsed as non-NULL empty strings | PASS |
| E2 | Empty | URL has no explicit path | Path is `/` | Path was `/` | PASS |
| B2 | Boundary | Total URL lengths 2047, 2048, 2049 bytes | Accept through documented 2048 limit; reject above | 2047 and 2048 accepted; 2049 rejected | PASS |
| B3 | Boundary | DNS hostname lengths 253 and 254 | Accept maximum hostname; reject over maximum | 253 accepted; 254 rejected | PASS |
| B4 | Boundary | Label lengths 63 and 64 | Accept 63; reject 64 | 63 accepted; 64 rejected | PASS |
| N3 | Normal | Mixed-case host, `:80`, dot segments, query, fragment | Lowercase host/scheme, remove default port and dot segments, keep query, omit fragment | `http://example.com/a/c?q=1` | PASS |
| B5 | Boundary | Resolve `../../../../x` from a deep path | Collapse beyond-root parents at `/` | Resolved to `https://example.com/x` | PASS |
| N4 | Normal | Query-only reference `?q=2` | Retain base path and replace query | `https://example.com/a/page.html?q=2` | PASS |
| E3 | Empty/reference | Resolve `#part` against a base with `?old=1` | Fragment omitted from crawl identity; base query retained | Returned `https://example.com/a/page.html`; query was lost | FAIL |
| E4 | Empty/reference | Resolve empty relative reference | Return base URL identity | Returned containing directory `https://example.com/a` | FAIL |
| F1 | Failure | Invalid base URL | Return `-1`, output remains NULL | Returned `-1`; output NULL | PASS |
| F2 | Failure | Malformed absolute HTTP reference | Return error without crash | Returned error; output NULL | PASS |
| F3 | Failure | NULL output pointers for normalize/resolve | Return error | Both returned `-1` | PASS |
| I2 | Invalid | NULL URL, empty URL, NULL parse output | Return error without crash | All returned `-1` | PASS |
| M1 | Memory | `url_free(NULL)` and free zero-initialized `ParsedUrl` | Safe cleanup | Completed without crash | PASS |
| M2 | Memory | 10,000 parse/free/normalize/resolve cycles | Complete without crash | All cycles completed | PASS |
| N5 | Normal | Valid `CrawlerConfig` | Return `CRAWLER_NOT_IMPLEMENTED`; zero stats | Returned placeholder status and zeroed stats | PASS |
| D1 | Large input | 2047-byte URL with a large query | Accept within limit | Accepted | PASS |
| D2 | Large input | 2048-byte URL with a large query | Accept at limit | Accepted | PASS |
| D3 | Large/failure | 2048-character relative reference expands beyond URL limit | Return error and leave output NULL | Failed safely with output NULL | PASS |
| D4 | Normalization semantics | Normalize `https://a.test/a/` | Preserve trailing slash because it can affect resource identity | Returned `https://a.test/a` | FAIL |
| CLI1 | Invalid CLI | No arguments | Usage error and nonzero exit | Usage printed; exit 2 | PASS |
| CLI2 | Empty CLI | Empty start URL argument | Usage/error and nonzero exit | Usage printed; exit 2 | PASS |
| CLI3 | Invalid CLI | Depth `abc` | Clear depth error; nonzero exit | Error printed; exit 2 | PASS |
| CLI4 | Invalid CLI | Depth `-1` | Clear depth error; nonzero exit | Error printed; exit 2 | PASS |
| CLI5 | Invalid CLI | Extra argument | Usage error; nonzero exit | Usage printed; exit 2 | PASS |
| CLI6 | Normal | `https://example.org 2` | Explain crawler placeholder; successful exit | Placeholder message printed; exit 0 | PASS |
| CLI7 | Scope check | `ftp://example.org 1` passed to Part 1 CLI | Part 1 stub does not promise URL validation; Part 2 parser should reject FTP | CLI printed placeholder and exited 0; direct URL parser rejects FTP | N/A |

The two known resolution failures and trailing-slash normalization behavior were observed directly by the temporary harness. No source or committed tests were changed to correct them, per the task restriction.

## 4. Test coverage against the seven categories

| Category | Coverage | Assessment |
|---|---|---|
| Normal cases | HTTP/HTTPS parsing, components, normalization, common relative resolution, valid crawler placeholder | Adequate for current implemented scope; includes one known normalization semantic issue below |
| Boundary cases | URL lengths 2047/2048/2049, hostname 253/254, labels 63/64, ports 1/65535/0/65536, deep `../` | Covered for requested limits |
| Invalid input | NULL/empty pointers, unsupported scheme, malformed separators, missing/invalid host, bad ports, bad references, invalid CLI/config | Covered; allocation-failure injection remains unavailable |
| Empty input | Empty URL/query/fragment/relative and missing path/CLI arguments | Covered; empty relative reference currently returns the wrong URL |
| Large input | Limit-sized paths/query and oversized relative input | Covered around the documented URL cap |
| Failure conditions | Invalid parse, normalize/resolve output pointers, malformed base/reference, CLI arguments | Covered for representative failures; allocation failure is not injectable here |
| Memory management | Ownership/source review, empty cleanup, 10,000 repeated cycles | Partial. Runtime repetition passed; leak/double-free proof unavailable because sanitizers could not run |

## 5. Error handling review

**Error handling is not fully implemented for now.**

- `crawler_run` validates NULL pointers, empty start URL, domain flag, and positive timeout. It clears caller statistics before validation and returns `CRAWLER_INVALID_ARGUMENT` or `CRAWLER_NOT_IMPLEMENTED`. It does not parse or validate the URL in this Part 1 placeholder.
- `url_parse`, `url_normalize`, and `url_resolve` return `0` for success and `-1` for errors. This avoids crashing for the tested invalid inputs, but callers cannot distinguish malformed input from allocation failure.
- Crawler statuses are documented in `project.h`; URL return codes and ownership have a brief comment in `url.h`, with additional details in `documentation/api.md`.
- `url_parse` starts by zeroing the output structure. Its source comment requires the caller to pass a zero-initialized or previously freed structure. Calling it on an already populated structure without `url_free` first would lose the old pointers and leak them.
- The `url_resolve` absolute-reference fast path returns `url_normalize(relative, output)` after parsing and allocating the base components, without freeing the parsed base. This is an evident leak on successful absolute-reference resolution.
- Invalid base/reference/output cases tested return errors safely. Allocation-failure cleanup is partly visible in source (for example, partial `url_parse` allocations are released), but could not be forced in a runtime test.
- The CLI reports malformed argument counts and depth syntax with exit code 2. It does not check URL syntax; that is outside the Part 1 stub behavior.

The library reports errors rather than exiting or crashing for the exercised invalid inputs, but the undifferentiated URL error code and the identified cleanup/semantic defects mean the assignment’s error-handling requirement is only partially satisfied.

## 6. Memory-management review

`url_parse` allocates each component; callers release them with `url_free`. `url_normalize` and `url_resolve` return heap strings that callers release with `free`. The tests follow this ownership rule. `url_free` checks NULL and clears all fields after freeing. Repeated parse/free and returned-output cleanup completed 10,000 cycles without an observed crash.

AddressSanitizer was attempted with the installed MSYS2 GCC, but linking failed because the MinGW installation lacks `-lasan` (`ld: cannot find -lasan`). `valgrind` is not installed/on PATH. Allocation-failure injection was not performed. Therefore memory safety and absence of leaks are **not fully verified**; source review found the absolute-reference `url_resolve` leak described above.

## 7. Failed or incomplete tests

- **Failed:** fragment-only resolution drops the base query.
- **Failed:** empty relative resolution returns the containing directory rather than the base URL.
- **Failed:** normalization removes a trailing slash (`/a/` becomes `/a`).
- **Not tested:** deterministic allocation-failure injection, successful ASan/Valgrind run, fuzzing, or exhaustive URL/RFC conformance.
- **Not applicable to Part 1:** CLI accepting a supplied FTP URL is consistent with the crawler placeholder not invoking the Part 2 URL parser; direct parser validation rejects FTP.

## 8. Final conclusion

Part 1 passed the available build, API, and CLI checks. Part 2 passed the existing test suite and most additional parser/boundary checks, but the three failed URL behavior probes must be resolved before claiming complete correctness. The APIs report errors safely for the cases exercised, yet error handling is not fully implemented for now because URL errors are not differentiated and static review found a leak in absolute URL resolution. Memory safety has not been fully verified due unavailable sanitizer/leak tools.
