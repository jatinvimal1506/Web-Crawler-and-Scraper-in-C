# Project progress

## Completed: Part 1 foundation

- Reusable static-library layout and separate demo.
- Public config, stats, status/error codes, and API declarations.
- Argument validation and safe zero-initialization of output statistics.
- Windows MSYS2 UCRT64 GCC/Make workspace configuration.
- Make targets for build, test, and clean.
- Foundational API tests and Part 1 documentation.
- Git ignore rules for generated build output.

## Pending

- Part 2: URL parsing, validation, normalization, and relative URL resolution.
- Part 3: HTTP downloader.
- Part 4: HTML parser and extraction API.
- Part 5: crawl queue, visited set, depth/domain filtering, and crawl engine.
- Part 6: working crawl CLI output and statistics.
- Part 7: broader failure, boundary, and memory tests; complete documentation and contribution details.

Files `src/url.c`, `src/http.c`, `src/html.c`, `src/queue.c`, and `src/visited.c` are placeholders. No crawling feature is represented as complete yet.

## Next step

Part 1 establishes the API and build foundation. Part 2 URL handling can begin against this public interface.
