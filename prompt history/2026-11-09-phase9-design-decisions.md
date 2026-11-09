# Phase 9 design decisions

**Date:** 2026-11-09
**Scope:** One safe C11 PDF, DOCX, or ZIP download. This records the choices
made for Phase 9 only; Phases 10 and later are not authorized here.

## API and relationship to Phase 8

The additive `DownloadConfig` gathers the document URL, a seed URL for scope,
the same-domain setting, timeout, an already-existing destination directory,
and a caller-selected basename. `download_document` accepts one URL at a time.
It reuses `document_type_from_url`; it does not change `DocumentResults`, its
source-page provenance, or Phase 8's discovery behavior.

`DownloadResult` reports a status, detected extension type, last fetched HTTP
status and URL, number of successfully written bytes, a destination path when
the file is usable (or when failed cleanup must be diagnosed), and a bounded
diagnostic string. The caller initializes it as `{0}` and always calls
`download_result_free`. Input strings are borrowed for the call. Returned URL
and path strings are owned by the result. Static status/type names are borrowed
static strings and must not be freed.

**Why:** A small one-document API keeps discovery separate from saving and
avoids changing established crawler and provenance APIs.

## Streaming and size checks

The libcurl write callback receives `(pointer, size, count)` and first checks
that `size * count` fits in `size_t`. It writes those exact bytes with
`fwrite(data, 1, byte_count, file)`, checks for a short write, and checks that
the cumulative total stays at or below 20 MiB before accepting more data.
Nothing treats the response as a C string, so embedded NUL and high-valued
bytes are preserved. Application memory stays bounded by libcurl's callback
chunk; the complete binary body is never assembled in RAM.

Both a libcurl advertised-size option and the callback limit are used. The
callback is still necessary for chunked responses without `Content-Length`.
Exactly 20 MiB is allowed; byte 20 MiB + 1 is rejected. A transfer error or
short file write is not reported as success.

**Why:** Buffering a full document would waste memory and would mishandle
binary bytes if string functions were used.

## Response type and status validation

The URL extension must be PDF, DOCX or ZIP before networking. The response must
have exactly one recognized `Content-Type`: `application/pdf`, the DOCX MIME,
`application/zip`, or ZIP's `application/x-zip-compressed`. MIME parameters
are allowed. A single `application/octet-stream` header is an explicitly
documented fallback for a URL whose extension is recognized. Missing,
duplicate, HTML and unrelated types are rejected. HTTP 206 is rejected as an
HTTP response error; transport truncation is reported separately as
`DOWNLOAD_ERROR_INCOMPLETE`. Empty bodies are rejected.

MIME is only a server-provided label. This phase does not parse PDF, DOCX, or
ZIP structure and does not claim that a MIME header proves the bytes are valid.

**Why:** URL classification alone is not enough to accept a server response,
but content sniffing or format parsing would exceed the one-file downloader
scope.

## Redirect and request-scope rules

The downloader uses a manual loop rather than libcurl automatic redirects.
Each 301/302/303/307/308 `Location` is resolved against the last actually
fetched URL, parsed and checked before another request is made. At most five
redirects are followed. Only HTTP(S) is supported. Both scope modes pin the
seed's effective port; `same_domain=1` additionally pins the exact parsed seed
hostname. HTTPS-to-HTTP is blocked. A cross-host redirect on the same port is
permitted only when `same_domain=0`.

The redirect loop reuses the crawler's checked scope predicate and the
single-transfer libcurl setup. TLS peer and host verification remain enabled.
A blocked host, port, or downgrade is returned as `DOWNLOAD_ERROR_SCOPE`;
malformed, unsupported, looping or excessive redirects use
`DOWNLOAD_ERROR_REDIRECT`. The attempted redirect target is not requested.

**Why:** Automatically following redirects would fetch an out-of-scope target
before application code could inspect it.

**Limitation:** Exact hostname and port checks are not an IP firewall. DNS
rebinding, a permitted host resolving to a private address, robots policy,
authorization, and site-specific restrictions are not solved by this code.
Use only sites and test systems for which access is authorized.

## Output names, exclusive creation, and cleanup

The caller supplies a basename; the downloader does not trust a URL path or
`Content-Disposition` value as a filesystem path. Names are limited to ASCII
letters, digits, dot, dash and underscore, must have the URL's matching
`.pdf`, `.docx`, or `.zip` extension, and must fit the documented 128-byte
limit. Separators, path/drive forms, Windows device names, empty names, `.` /
`..`, trailing spaces/dots and control characters are rejected. The output
directory must already exist.

The file is created with exclusive-create flags (`O_EXCL` or the Windows
CRT equivalent). A pre-existing file is never opened or removed. The file is
visible while bytes are streaming; this is not atomic publication. If transfer,
validation, writing or close fails, the implementation closes and removes only
the file created by this call. A failed removal returns the distinct
`DOWNLOAD_ERROR_CLEANUP` and retains the owned path so the caller can locate
the leftover. Close and write failures have their own statuses.

**Why:** Exclusive creation prevents accidental overwrite without relying on a
rename operation whose replacement behavior differs between platforms.

## Tests and test-only hooks

All new executable code, fixtures and tests are C. The local fixture uses
loopback sockets and serves deterministic binary bytes, redirects, HTTP errors,
MIME variants, truncated/delayed responses, and exact/oversized bodies. The
allocation target compiles separate instrumented library objects; the
production archive contains no allocator or file-I/O fault hooks. Windows
build compatibility is retained in the Makefile, but POSIX local-server tests
explicitly skip under native Windows.

The acceptance/handoff file records measured commands and outcomes after the
full regression run. This design note is not itself evidence that a command
passed.
