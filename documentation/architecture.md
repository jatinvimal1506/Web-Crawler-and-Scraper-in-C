# Architecture

The project keeps the library and demonstration application separate. The Makefile compiles each module into `build/`, archives them into `libcrawler.a`, then links the CLI and tests.

## Implemented modules

- `project.c` / `project.h`: Part 1 public crawler API, input validation, statuses, and statistics initialization. A valid crawl request returns not implemented.
- `url.c` / `url.h`: Part 2 URL parsing, validation, normalization, relative-reference resolution, and memory cleanup.
- `http.c` / `http.h`: Part 3 single-page HTTP(S) downloader, using libcurl and the URL parser.
- `html.c` / `html.h`: standalone Phase 4 anchor extraction, practical entity
  decoding, raw-text exclusions and relative HTTP(S) link resolution.
- `examples/demo.c`: Part 1 CLI that validates its arguments and demonstrates the current API status.
- `examples/demo_pipeline.c`: presentation demo that calls the crawler API, URL parser, and downloader in sequence.

## Current integration boundary

`crawler_run` currently validates its configuration and returns
`CRAWLER_NOT_IMPLEMENTED`; it does not orchestrate the modules. The pipeline
demo calls `url_parse` and `http_download` directly after demonstrating the
crawler API. Standalone HTML extraction is implemented and local C integration
tests combine it with HTTP downloads. Queue/visited tracking (Phase 5) and BFS
crawl integration (Phase 6) remain unimplemented; queue/visited sources are
placeholders.

## Ownership and failure boundary

The downloader owns the body while receiving it. On success it transfers that
body and copied metadata to `HttpResponse`. If copying a present metadata value
fails, it frees the body and any earlier copies, resets response fields, and
returns `HTTP_ERR_OUT_OF_MEMORY` with a diagnostic. A missing Content-Type is
different: there is no header to copy, so the existing HTML-body heuristic is
used. Callers free responses before reuse.

Extraction borrows the response body and final URL only for the duration of the
call. Its output owns independent strings and is freed with `html_links_free`.
The HTTP/HTML public signatures are unchanged.

## Planned crawl data flow

Start URL → normalize → queue → download → parse and extract links → resolve and filter → visited set → queue. The root page will have depth zero; same-domain filtering and maximum depth will be applied by the future crawl engine. Concurrency is an optional extension.
