# Architecture and implementation plan

## Library boundary

The public API lives in `include/project.h`; its initial implementation is in `src/project.c`. The separate program in `examples/demo.c` calls that API and owns command-line parsing and output. The test executable in `tests/test_crawler.c` links to the same static library as the demo.

The Makefile compiles each source file independently into `build/`, archives the objects as `build/libcrawler.a`, then links the demo and tests against the archive.

## Planned data flow

Start URL -> parse/normalize -> FIFO queue -> HTTP download -> HTML extraction -> resolve and filter links -> visited set -> queue. The CLI will call the reusable library and print extracted information and crawl statistics after these later parts are implemented.

## Module responsibilities

- `project.c`: validate API arguments, coordinate future modules, enforce depth/domain rules, and report statistics.
- `url.c`: later implement URL validation, parsing, relative resolution, and normalization.
- `http.c`: later implement downloading, timeout/status handling, and byte accounting.
- `html.c`: later implement title, headings, links, paragraphs, and metadata extraction.
- `queue.c`: later implement a breadth-first FIFO of URL/depth entries.
- `visited.c`: later track normalized URLs and reject duplicates.

At this stage, the five future module files are comments/placeholders. They do not parse or download anything. A valid configuration returns `CRAWLER_NOT_IMPLEMENTED`.

## Crawl rules for later parts

The root URL has depth 0. Do not enqueue links beyond `max_depth`. In same-domain mode, skip links whose normalized host differs from the start host. Concurrent workers are an optional extension after the single-threaded crawler is complete.

See [progress.md](progress.md) for current scope and [api.md](api.md) for the Part 1 API contract.
