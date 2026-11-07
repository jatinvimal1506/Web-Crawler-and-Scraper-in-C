# Phase 4: HTML hyperlink extraction, explained

## Scope and source

DeepDownload-C remains the existing modular **C11 static library and C CLI
project**, imported from the `main` branch of
<https://github.com/jatinvimal1506/Web-Crawler-and-Scraper-in-C.git>.
The imported upstream commit was `586eb0a` (`Add Phases 1-3 demo pipeline and
documentation updates`). No GitHub commit or push is needed to use these edits.

Phase 4 extraction is added, along with explicitly authorized HTTP MIME and
redirect safety corrections. The public crawler, URL and HTTP API signatures,
URL module, existing demos and original tests are preserved. Queue and
visited remain placeholders; `crawler_run()` still returns
`CRAWLER_NOT_IMPLEMENTED` for valid configurations.

## What each changed or added file does

| File | Purpose |
| --- | --- |
| `include/html.h` | Public types, function declarations and ownership contract. |
| `src/html.c` | Scans HTML, decodes practical escapes and resolves anchor URLs. |
| `tests/test_html.c` | Deterministic, offline tests of extraction and cleanup. |
| `tests/test_html_alloc.c` | Optional allocation-failure tests using GNU linker wrapping. |
| `src/http.c` | Exact HTML MIME matching and HTTP(S)-only redirect policy. |
| `tests/test_http_local.c` | Small C-only loopback server and real HTTP/HTML integration tests. |
| `Makefile` | Linux/Windows commands, HTML tests, header dependencies and environment build flags. |
| `README.md`, `documentation/progress.md` | Current capabilities and build commands. |
| `documentation/phase4.md` | This guide. |

In the surrounding Replit project, libcurl and pkg-config are installed as system
dependencies. No additional HTML parser library or web frontend is introduced.

## The three public functions

### `html_extract_links(html, base_url, &links)`

Reads a NUL-terminated HTML string and puts discovered URLs into an initially
empty `HtmlLinks` structure. It never changes either input or makes a network
request. The base URL should be the downloaded page's **final URL after
redirects**, not necessarily the URL originally requested.

The result has:

- `urls`: an array of pointers to individual allocated URL strings;
- `count`: how many entries can be read, from index `0` to `count - 1`.

There is no extra NULL sentinel after the last array entry; always use `count`.

The function returns `HTML_OK`, `HTML_ERROR_INVALID_ARGUMENT`,
`HTML_ERROR_INVALID_BASE_URL` or `HTML_ERROR_OUT_OF_MEMORY`. Individual bad links
are skipped rather than failing the entire page. An invalid base fails even if
the page has no links, keeping the API contract consistent.

### `html_links_free(&links)`

Frees each URL string, then the pointer array, then resets `urls` to NULL and
`count` to zero. It does not free the `links` structure itself, which is usually
a local variable. Calling it twice or passing NULL is safe.

### `html_status_message(status)`

Returns a readable, static message for a status. This string belongs to the
program, not to the caller: do **not** free it.

## Minimal usage example

```c
#include "html.h"
#include <stdio.h>

int main(void)
{
    HtmlLinks links = {0};
    HtmlStatus status = html_extract_links(
        "<a href='../notes.pdf'>Notes</a>",
        "https://example.com/course/page.html",
        &links);

    if (status != HTML_OK) {
        fprintf(stderr, "%s\n", html_status_message(status));
        return 1;
    }
    for (size_t i = 0; i < links.count; ++i) {
        puts(links.urls[i]); /* https://example.com/notes.pdf */
    }
    html_links_free(&links);
    return 0;
}
```

The example is documentation, not a replacement for either existing CLI demo.

## Pointers and memory, without assuming prior experience

- A pointer stores the address of data. `char *` points to characters, normally
  a string terminated by `'\0'`. `const char *` means we only read those bytes.
- `&links` means "the address of links". Passing that address lets the function
  fill in the caller's structure. `links->count` inside a function accesses the
  `count` member through a pointer; it means `(*links).count`.
- `char **urls` points to an array whose entries are themselves `char *`
  pointers. The array and the individual strings are separate allocations.
- `{0}` initializes all fields to zero/NULL. This gives cleanup and extraction
  a known starting state. Do not pass an uninitialized structure.
- `malloc(n)` requests `n` bytes on the heap. These bytes survive the function
  that allocated them, but must later be freed. It can return NULL on failure.
- `realloc` grows the pointer array. A temporary `grown` pointer is used so
  failure cannot lose the original allocation. Capacity starts at 8 and doubles
  as necessary; multiplication is checked for overflow.
- `free` returns allocated storage. Do not free string literals, free the same
  pointer twice, or use a string after freeing it.
- Input HTML and the base URL remain caller-owned. Output strings are independent
  copies. Freeing the HTML response after extraction does not invalidate them.
- The extractor first builds a temporary result. On failure it cleans up all
  partial output. On success ownership transfers to the caller, who calls
  `html_links_free`. Passing an already populated result is rejected without
  overwriting it; free it before reuse.

## Internal helper functions in `src/html.c`

These are `static`: private implementation details, not public library APIs.

| Function | Purpose |
| --- | --- |
| `html_space` | Recognizes the five HTML whitespace characters without locale dependence. |
| `ascii_lower` | Lowercases an ASCII letter so names can be compared regardless of case. |
| `name_is` | Compares a bounded input slice with a known name, without allocating. |
| `tag_end` | Finds `>` outside quotes; detects a new `<` as a malformed-tag recovery point. |
| `entity_byte` | Converts recognized named/ASCII numeric entities to one byte; bounds numeric arithmetic. |
| `entity_at` | Reads one bounded entity spelling for both scanner context and decoding, without allocation. |
| `decode_href` | Allocates a decoded copy of one attribute value; never decodes recursively. |
| `find_href` | Walks attributes within one tag and returns a borrowed slice of the first `href`. |
| `append_href` | Trims, resolves and validates a candidate, then grows the owned output array. |

The main extraction loop skips comments and raw-text elements, recognizes exact
`a` tag names, finds `href`, and calls `append_href`. Cleanup is centralized in
`html_links_free`.

## Parsing policy and limitations

- Single quotes, double quotes, case variations, whitespace around `=`, and
  straightforward unquoted attributes are supported. An unquoted query string
  may contain `=`, as in `href=/search?q=one`. Decimal/hexadecimal question-mark
  references and `&quest;` work too. The scanner interprets those references
  for context without treating the `#` inside a numeric spelling as a fragment.
  Whitespace in the source ends the value, so a
  following `class=...` or duplicate `href=...` cannot be swallowed into it.
  Quotes/backticks inside unquoted values, `=` before the query, stray attribute
  delimiters and missing separators after a preceding quoted attribute are
  conservatively rejected. This is not full browser error recovery.
- Recognized semicolon-terminated names: `amp`, `AMP`, `quot`, `QUOT`, `apos`,
  `lt`, `LT`, `gt`, `GT`, `colon`, `sol`, `num`, `quest`, `Tab` and `NewLine`.
  Decimal (`&#47;`) and hexadecimal (`&#x2F;`) numeric ASCII entities are
  supported. Unknown names remain literal. Invalid, zero or non-ASCII numeric
  references cause that href to be skipped. Full Unicode/HTML5 decoding is not
  attempted because the existing URL module supports ASCII URLs.
- URL validation happens **after** decoding. Encoded JavaScript schemes are
  therefore rejected as well. Only HTTP(S) URLs accepted by the existing
  `url_resolve()` are returned. This is scheme filtering, not a permission check
  to fetch any address or an SSRF protection system.
- Relative paths, root-relative paths, protocol-relative links, query references
  and fragments follow existing URL-module behavior. Fragments are removed.
  The URL module's 2048-byte limit still applies.
- Empty and whitespace-only href values are skipped; fragment-only values
  resolve to the current normalized page.
- Duplicates are deliberately preserved in document order. Extraction does not
  implement the future visited/deduplication phase. Duplicate `href` attributes
  on one anchor use the first occurrence, even if it is empty or unsafe.
- Anchors appearing in comments, other attributes, `script`, `style`, `title`,
  `textarea`, `iframe`, `xmp`, `noembed` or `noframes` contents are not treated
  as real links. `plaintext` consumes the rest, even after a closing spelling.
  Malformed raw-text closing tags do not prematurely expose fake anchors.
  `noscript` depends on browser scripting state; this scanner treats it as
  ordinary markup and does not attempt scripting-dependent HTML5 behavior.
- Unterminated comments, quoted tags and raw-text blocks consume the remainder
  of the input. This favors not inventing links over browser-style recovery.
- There is no HTML5 tree building, `<base>` processing, JavaScript execution,
  non-anchor discovery, document classification, provenance storage or crawling.
- Input must be a valid, readable NUL-terminated C string. Embedded NUL ends
  parsing. This API does not accept arbitrary unterminated byte buffers.

## How the existing modules fit together

```text
http_download() -> response.body + response.final_url
                           |
                           v
html_extract_links(body, final_url, &links)
                           |
                           v
url_resolve() -> validated, normalized absolute URLs
                           |
                           v
caller reads links -> html_links_free()
```

After using the response itself, the caller also calls
`http_response_free(&response)` and eventually `http_cleanup()`.
This describes how a caller can combine the modules; it does **not** connect
them to `crawler_run()` or create a recursive crawler.

## Building and testing

From the Replit shell:

```sh
cd DeepDownload-C
make                  # static library and the two existing demos
make test-unit        # existing API/URL tests and new HTML tests, no network
make test-html        # only the new deterministic HTML suite
make test-http        # existing HTTP suite, including live network checks
make test-http-local  # deterministic C-only loopback HTTP fixtures (POSIX)
make test-http-alloc  # same fixtures plus HTTP metadata OOM regressions (POSIX)
make test             # both unit and HTTP suites
make test-html-alloc  # optional GNU-linker allocation-failure test
```

On native Windows/MSYS2, `OS=Windows_NT` keeps cmd.exe, `.exe` names, Windows
directory commands and backslash execution paths. Linux uses POSIX equivalents.
`CFLAGS` retains dependency include paths; link recipes now honor `LDFLAGS`.
Header changes also trigger rebuilding the library objects. The local fixture
suite uses POSIX fork/sockets and explicitly prints SKIP on native Windows; the
production library does not gain a POSIX dependency.

## Targeted HTTP safety corrections

The Content-Type check now trims surrounding space/tab from the media-type
token (before the first semicolon), compares it case-insensitively, and accepts
only `text/html` or `application/xhtml+xml`. Parameters such as charset remain
allowed. A lookalike token or `text/html` mentioned only inside a parameter does
not qualify.

Both initial and redirect curl protocol options are limited to HTTP/HTTPS.
New curl headers use the string options; older headers use the equivalent
protocol bitmask. If setting that restriction fails, the request fails before
performing network I/O. TLS peer/hostname verification is not disabled or
changed. The public HTTP API and status enum are unchanged; rejected redirect
protocols map through the existing network-error status.

The new C fixture creates a loopback listener on an OS-assigned port and forks
a small server. The parent uses the real public APIs; responses are not mocked
inside production code. `send_all` handles partial writes; a cleanup function
terminates and waits for the server child. No Python, Node or other-language
fixture runner is used.

The fixtures test redirect-then-relative-resolution using the final URL,
case/parameters in valid MIME types, misleading MIME tokens, binary PDF and
missing-type binary rejection, forbidden file/FTP redirects, the redirect-hop
limit, and exactly 5 MiB versus one byte over. Oversized responses are tested
both with Content-Length and with a connection-close stream, exercising the
write-callback limit as well as the announced-size limit.

The HTTP downloader still trusts a valid declared HTML MIME type; these tests
do not claim binary sniffing for falsely labelled `text/html`. The extractor
still accepts NUL-terminated strings, not arbitrary byte buffers, and has no
independent 5 MiB input limit. TLS/HTTPS redirect fixtures are not provided;
the local server is plain HTTP, while the original suite retains live HTTPS.

### Metadata allocation failures

`copy_string` needs heap memory to preserve metadata after curl is cleaned up.
If either a final-URL copy or a present Content-Type copy fails, a shared
`metadata_oom` cleanup label frees the body and earlier metadata copies, resets
owned fields/status/size, and returns `HTTP_ERR_OUT_OF_MEMORY` with a message.
The label is a single error-cleanup destination, not a loop or a new API.
An absent Content-Type does not call `copy_string`; it still permits the
existing HTML-body heuristic. Always free a previous response before reuse.

The HTTP allocation test compiles only a separate test HTTP object with
allocator names replaced by C test functions. Those functions simulate the
first/second metadata malloc failure and count HTTP-owned allocations,
including realloc/calloc of the body. Production library objects remain
uninstrumented. Tests check cleanup before caller free, repeated free and a
successful request using the same response afterwards.

The HTML suite checks normal links, duplicates, entities, unsafe schemes,
false positives, malformed input, empty/invalid arguments, ownership, URL
boundaries, array growth, every truncation of a mixed document, and statuses.
The optional allocation test fails every malloc/realloc point in a representative
10-link extraction, including allocations inside the existing URL module. It
also wraps free and verifies zero tracked outstanding allocations after cleanup
and each failure.

Actual execution results are recorded in `documentation/phase4-test-results.md`.
No native Windows execution is claimed from Linux.
