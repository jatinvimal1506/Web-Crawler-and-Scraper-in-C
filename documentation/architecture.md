# Architecture and implementation plan

## Data flow

Start URL -> parse/normalize -> FIFO queue -> HTTP download -> HTML extraction -> resolve and filter links -> visited set -> queue. The CLI calls the reusable library and prints extracted information and crawl statistics.

## Modules

- `url.c`: validate, parse, resolve relative links, and normalize URLs.
- `http.c`: download HTML, enforce timeout/status handling, and count bytes.
- `html.c`: extract title, headings, links, paragraphs, and metadata.
- `queue.c`: breadth-first FIFO of URL/depth entries.
- `visited.c`: track normalized URLs and reject duplicates.
- `project.c`: coordinate modules, enforce depth/domain rules, and report statistics.

Root URL depth is 0. Never enqueue a link beyond `max_depth`. In same-domain mode, skip links whose normalized host differs from the start host. Concurrent workers are an optional extension after the single-threaded crawler is complete.

## Acceptance and testing

Cover normal pages, relative and duplicate links, depth and domain boundaries, empty/malformed HTML, invalid URLs, unreachable servers, HTTP errors, timeouts, large input, and cleanup on failures. Run memory checks with AddressSanitizer or Valgrind where available.

