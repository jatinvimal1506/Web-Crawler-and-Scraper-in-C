# Public API

## Safety contract retained from Phase 7

Both `http_download` and `crawler_run` accept timeout **1..3600 inclusive**;
unsupported values return INVALID_ARGUMENT before any network request.
All curl options are checked before perform: timeouts, follow flag, protocols,
five-hop cap, explicit TLS peer/host verification and response size limits.
Failed option setup returns HTTP_ERR_INIT; OOM remains OUT_OF_MEMORY.

Additive `http_download_scoped(url, timeout, seed_url, same_domain, response)`
borrows inputs, uses existing URL parsing/resolution and follows
301/302/303/307/308 in a bounded C loop. Each Location is resolved against the
actual response final_url and validated BEFORE requesting it. Both modes pin
seed effective port; strict mode also pins exact canonical hostname.
HTTPS->HTTP is blocked; same-port HTTP->HTTPS keeps TLS verification.
Timeout is per hop, not one shared deadline for the whole redirect chain.

HTTP_ERR_SCOPE is appended without renumbering earlier HTTP statuses; maps to
CRAWLER_PARSE_ERROR at root, or failed child/eventual PARTIAL. Metadata describes
the last fetched hop, never an unrequested blocked URL. Malformed/missing/
unsupported Location uses HTTP_ERR_HTTP_STATUS; excessive hops/loops use
HTTP_ERR_TOO_MANY_REDIRECTS; partial transport uses HTTP_ERR_NETWORK.
Scoped OOM empties all owned response fields; always use http_response_free.
Standalone automatic redirects remain supported. This is NOT IP/DNS-rebinding,
robots or authorization enforcement. Existing config/stat layouts are unchanged.

The Phase 6 sections below describe retained APIs. Their historical post-fetch
scope policy is superseded by
`../prompt history/2026-11-09-phase7-decisions-and-fixes.md`.

## Part 1 crawler API

`include/project.h` declares `crawler_run(config, stats)`. It zeroes available
caller-owned statistics and validates config/start URL before doing network
work. Phase 6 performs real bounded BFS. Additive
`crawler_run_with_link_observer(config, stats, observer, context)` calls a C
callback for each resolved, normalized, in-scope href. URL strings are borrowed
only for the callback duration. Set `*crawl_link = 0` to inspect/record the
link without placing it in the BFS queue; `1` preserves normal crawling. Return
`CRAWLER_OK` to continue, or a documented fatal status to stop. Ordinary
`crawler_run` passes no observer and preserves its Phase 6 behavior. Existing config/stat layouts and enum
values are preserved; PARTIAL, LIMIT_REACHED and INTERNAL_ERROR are appended.
`crawler_status_string` returns a static string. No allocated data escapes.

The central stats/error/scope contract is in
`../prompt history/2026-11-08-phase6-design-decisions.md`.
HTTP error bodies are not counted. Known final aliases are tracked even when
the download/parsing fails. Independent queue/visited APIs remain unchanged.
The original HTML extractor is unchanged in behavior; the additive
`html_extract_links_limited` provides bounded temporary output for the engine.

## Phase 8 document discovery API

`include/document.h` declares:

- `document_type_from_url(url, &type)` parses an absolute HTTP(S) URL and
  classifies only the path's final `.pdf`, `.docx` or `.zip` suffix,
  case-insensitively. Query and fragment are ignored. A valid unmatched suffix
  returns `DOCUMENT_OK` with `DOCUMENT_TYPE_UNKNOWN`; it never probes content.
- `document_discover(config, &results, &stats)` runs the existing bounded BFS
  with the additive link observer. It retains normalized document URLs and
  distinct normalized source-page/depth pairs. A recognized document link is
  observed but filtered from the page queue, so the crawler does not fetch it.
- `document_results_init` initializes an empty/zeroed object without allocating.
  `document_results_free` releases all owned document strings, source arrays,
  source-page strings and the top-level array; it is NULL-safe and repeatable.
- `document_results_count`, `document_results_source_count` and
  `document_results_at` provide read-only access. The returned record pointer
  is borrowed and valid only until the results are freed or reused.
- `document_type_name` and `document_status_message` return static strings;
  callers must not free them.

Initialize `DocumentResults` as `{0}` or call its init function on a zeroed
object. Do not shallow-copy it or modify its public arrays. All result strings
are owned by the result object and no result allocation escapes it. On fatal
OOM/overflow the operation releases partial results and returns an empty
object; on a non-root page failure it returns `DOCUMENT_PARTIAL` while keeping
successful discoveries; on crawl/discovery budget exhaustion it returns
`DOCUMENT_LIMIT_REACHED` with bounded results retained. The result limit is
1000 unique document/source pairs. Linear URL/pair searches are intentionally
simple (O(n) lookup, O(n²) worst-case accumulation) at this bounded scale.

There is no binary download, content-type sniffing, hash, file save or report.
An unrecognized extension stays an ordinary HTTP(S) page candidate.

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

## Part 3 HTTP API

`include/http.h` declares `http_download(url, timeout_seconds, response)`, which uses the URL parser to validate an HTTP(S) URL and downloads one HTML response through libcurl. It returns `HTTP_OK` only for a successful 2xx response with HTML content. On success, `HttpResponse` contains the allocated body and its byte size, HTTP status code, final URL, and content type. On failure it returns a specific `HttpStatus`; the response may contain the status code and a detail string when available.

Initialize `HttpResponse` as `{0}` before use. Release response-owned strings with `http_response_free`, and call `http_cleanup` when the process is done with libcurl. HTTP status failures (such as 404) are distinct from transport failures (such as DNS or connection errors). The downloader does not extract HTML or crawl links.
