# Architecture

The project keeps the library and demonstration application separate. The Makefile compiles each module into `build/`, archives them into `libcrawler.a`, then links the CLI and tests.

## Implemented modules

- `project.c` / `project.h`: Part 1 public crawler API, input validation, statuses, and statistics initialization. A valid crawl request returns not implemented.
- `url.c` / `url.h`: Part 2 URL parsing, validation, normalization, relative-reference resolution, and memory cleanup.
- `examples/demo.c`: Part 1 CLI that validates its arguments and demonstrates the current API status.

## Reserved module boundaries

`http.c`, `html.c`, `queue.c`, and `visited.c` are placeholders for Parts 3–5. Their intended future roles are HTTP downloading, HTML extraction, breadth-first queueing, and duplicate tracking. They are not linked as functional implementations today.

## Planned data flow

Start URL → normalize → queue → download → parse and extract links → resolve and filter → visited set → queue. The root page will have depth zero; same-domain filtering and maximum depth will be applied by the future crawl engine. Concurrency is an optional extension.
