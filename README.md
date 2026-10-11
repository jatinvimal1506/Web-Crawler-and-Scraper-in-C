# Web Crawler and Scraper in C

A reusable C library and separate command-line demo for crawling pages and extracting selected HTML content. This repository is an initial scaffold: network, parsing, URL, queue, and visited-set implementations remain TODOs.

## Requirements

- C11 compiler and Make
- Planned dependencies: libcurl for HTTP/HTTPS and libxml2 for HTML parsing (not wired in yet)

## Build

```sh
make
make test
```

## Demo

```sh
./build/crawler-demo https://example.org 2
```

The current crawler API validates arguments and reports `not implemented` until crawl modules are implemented.

## Layout

- `include/project.h`: public library API
- `src/`: URL, HTTP, HTML, queue, visited set, and crawl engine modules
- `examples/demo.c`: independent CLI demonstration
- `tests/`: unit and integration tests
- `documentation/`: design, API, memory, and test documentation

See `documentation/architecture.md` for the implementation plan and `documentation/memory.md` for ownership rules to maintain.

