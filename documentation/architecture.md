# Architecture

The project keeps the library and demonstration application separate. The Makefile compiles each module into `build/`, archives them into `libcrawler.a`, then links the CLI and tests.

## Implemented modules

- `project.c` / `project.h`: Part 1 public crawler API, input validation, statuses, and statistics initialization. A valid crawl request returns not implemented.
- `url.c` / `url.h`: Part 2 URL parsing, validation, normalization, relative-reference resolution, and memory cleanup.
- `http.c` / `http.h`: Part 3 single-page HTTP(S) downloader, using libcurl and the URL parser.
- `examples/demo.c`: Part 1 CLI that validates its arguments and demonstrates the current API status.
- `examples/demo_pipeline.c`: presentation demo that calls the crawler API, URL parser, and downloader in sequence.

## Current integration boundary

`crawler_run` currently validates its configuration and returns `CRAWLER_NOT_IMPLEMENTED`; it does not invoke URL parsing or downloading. The pipeline demo therefore calls `url_parse` and `http_download` directly after demonstrating the crawler API. HTML extraction, breadth-first queueing, duplicate tracking, and crawl integration remain future work.

## Planned crawl data flow

Start URL → normalize → queue → download → parse and extract links → resolve and filter → visited set → queue. The root page will have depth zero; same-domain filtering and maximum depth will be applied by the future crawl engine. Concurrency is an optional extension.
