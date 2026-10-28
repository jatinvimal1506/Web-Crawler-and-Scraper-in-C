# Public API

## Part 1 crawler API

`include/project.h` declares `crawler_run(config, stats)`. It clears caller-owned statistics when possible, validates required pointers, start URL non-emptiness, the domain flag, and positive timeout. A valid call currently returns `CRAWLER_NOT_IMPLEMENTED` without networking. `crawler_status_string` returns a static string.

## Part 2 URL API

`include/url.h` declares `UrlStatus` and the following functions:

- `url_parse(input, result)` parses an absolute HTTP(S) URL into scheme, host, optional port, path, query, and fragment.
- `url_free(url)` releases component strings and clears the structure; accepts NULL.
- `url_normalize(input, output)` creates an allocated canonical URL.
- `url_resolve(base, relative, output)` resolves the supported common URL reference forms and normalizes the result.
- `url_status_message(status)` returns a static message; do not free it.

URL functions return one of:

| Status | Meaning |
|---|---|
| `URL_OK` | Operation succeeded |
| `URL_ERROR_INVALID_ARGUMENT` | Required pointer missing or parse output is already populated |
| `URL_ERROR_INVALID_FORMAT` | Malformed URL/reference syntax |
| `URL_ERROR_UNSUPPORTED_SCHEME` | Scheme is not HTTP or HTTPS |
| `URL_ERROR_INVALID_HOST` | Missing or invalid ASCII DNS hostname |
| `URL_ERROR_INVALID_PORT` | Port is empty, nonnumeric, zero, or greater than 65535 |
| `URL_ERROR_TOO_LONG` | URL exceeds 2048 bytes |
| `URL_ERROR_OUT_OF_MEMORY` | Allocation failed |

Initialize `ParsedUrl` as `{0}` before parsing. `url_parse` only writes the result after all components have been allocated, and rejects a structure containing non-NULL fields to avoid overwriting caller-owned pointers. The caller frees successful results with `url_free`. `url_normalize` and `url_resolve` set a non-NULL output pointer to NULL before work; successful strings belong to the caller and must be released with `free`.

Normalization lowercases the scheme and host, removes HTTP port 80 and HTTPS port 443 (including leading-zero spellings), ensures an absolute path, removes dot segments, preserves repeated and trailing slash separators, retains query bytes, and removes fragments. Valid percent escapes are preserved byte-for-byte; incomplete or non-hex escapes are rejected. Resolution supports HTTP(S) absolute references, protocol-relative, root-relative, directory-relative, query-only, fragment-only, and empty references. Fragment-only and empty references retain the base query; fragments are removed from crawl identity.

The implementation supports ASCII DNS names only. IPv6 literals, internationalized names, and exhaustive RFC 3986 behavior are outside scope.
