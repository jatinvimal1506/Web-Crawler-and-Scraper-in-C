# Web Crawler and Scraper in C

## Overview

A college project to build a reusable C library and a separate command-line demonstration for crawling web pages and extracting selected HTML information.

The assignment expects modular C code, a public API, error reporting, careful memory ownership, tests, documentation, and a demo application. This repository is currently the Part 1 foundation. It does **not** download pages, parse URLs or HTML, extract links, or crawl.

## Project layout

```text
include/project.h       Public library API
src/project.c           API validation and status messages
src/url.c                Reserved URL module (placeholder)
src/http.c               Reserved HTTP module (placeholder)
src/html.c               Reserved HTML module (placeholder)
src/queue.c              Reserved crawl-queue module (placeholder)
src/visited.c            Reserved visited-set module (placeholder)
examples/demo.c          Separate CLI demonstration
tests/test_crawler.c     Part 1 API tests
documentation/           Design, API, memory, tests, and progress notes
.vscode/                 Workspace build tasks and terminal environment
tools/make.cmd           PowerShell shim for MSYS2 mingw32-make
Makefile                 Static library, demo, tests, and clean targets
```

## Windows prerequisites

This workspace is configured for Windows, VS Code, PowerShell, MSYS2 UCRT64 GCC, and GNU Make. The current machine already has GCC 14.1.0 and MSYS2 `mingw32-make` 4.4.1 installed under `C:\msys64`; no package installation or system-wide PATH change was made. Workspace settings add the tool locations to VS Code's integrated terminal.

If setting up another machine, install MSYS2, then in its UCRT64 terminal install the compiler and Make:

```sh
pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-make
```

See the [MSYS2 UCRT64 environment documentation](https://www.msys2.org/docs/environments/) and [MSYS2 Make package](https://packages.msys2.org/packages/mingw-w64-ucrt-x86_64-make).

Open this project folder in VS Code and start a new PowerShell integrated terminal. The workspace adds `tools`, MSYS2 UCRT64, and MSYS2's utility directory to that terminal's PATH. If MSYS2 is installed somewhere other than `C:\msys64`, update `.vscode/settings.json` and the PATH entry in `.vscode/tasks.json`.

## Build and test

From the VS Code PowerShell terminal or a PowerShell terminal with the project tools on PATH:

```powershell
make clean
make
make test
```

The Makefile compiles every C module separately, archives them into `build/libcrawler.a`, and links the demo and test program against that library. Generated files are kept under `build/` and ignored by Git. VS Code also provides **Crawler: Build**, **Crawler: Test**, and **Crawler: Clean** tasks.

## CLI usage

```powershell
.\build\crawler-demo.exe https://example.org 2
```

The depth argument is optional and must be a non-negative integer. At this stage, a valid request prints the requested values and says crawling is not implemented yet. This is expected until later project parts are completed. Missing arguments, an empty URL, or an invalid depth are reported safely.

## Current implementation status

- **Part 1 foundation:** public configuration and statistics structures; status codes and readable messages; argument validation; safe statistics initialization; separate CLI; Windows GCC/Make build; static library; foundational tests and documentation.
- **Not implemented:** URL handling, HTTP downloading, HTML parsing, extraction, queue and visited-set behavior, and crawling. The related source files remain placeholders.

## Upcoming project parts

1. Project setup and foundation — this repository's current scope.
2. URL parsing, validation, normalization, and relative-link resolution.
3. HTTP download support.
4. HTML parsing and extraction.
5. Crawl queue, duplicate tracking, depth and domain rules.
6. CLI crawl output and statistics.
7. Expanded tests, memory checks, and final documentation.

See [architecture](documentation/architecture.md), [API](documentation/api.md), [memory rules](documentation/memory.md), [testing](documentation/testing.md), and [progress](documentation/progress.md).
