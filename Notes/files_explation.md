# Project files: explanation and purpose

This page is a learning guide to the files in `web-crawler-c`. The tree at the top shows the project layout. The sections below explain what each tracked/source file is, what it is used for, and why the project needs it.

## Project structure

```text
web-crawler-c/
├── .vscode/
│   ├── settings.json
│   └── tasks.json
├── Notes/
│   └── files_explation.md
├── build/                         # Generated output; ignored by Git
│   ├── crawler-demo.exe
│   ├── html.o
│   ├── http.o
│   ├── libcrawler.a
│   ├── project.o
│   ├── queue.o
│   ├── test-crawler.exe
│   ├── url.o
│   └── visited.o
├── documentation/
│   ├── api.md
│   ├── architecture.md
│   ├── memory.md
│   ├── progress.md
│   └── testing.md
├── examples/
│   └── demo.c
├── include/
│   ├── project.h
│   └── url.h
├── src/
│   ├── html.c
│   ├── http.c
│   ├── project.c
│   ├── queue.c
│   ├── url.c
│   └── visited.c
├── tests/
│   └── test_crawler.c
├── tools/
│   └── make.cmd
├── .gitignore
├── Makefile
└── README.md
```

`.git/` is Git's internal version-control database and is not listed above. It is managed by Git rather than being part of the crawler implementation. The `build/` contents shown are generated examples from the current workspace; compiling can add or replace object files and executables there.

## Root files

### `README.md`

The project introduction and getting-started guide. It describes the scope, prerequisites, build and test commands, CLI usage, current implementation status, and links to the detailed documentation. It is useful as the first page for someone opening the repository.

### `Makefile`

The build recipe used by GNU Make. It compiles the C source modules, combines their object files into `build/libcrawler.a`, links the demo and test programs, and provides `all`, `test`, and `clean` targets. A Makefile keeps build steps consistent and repeatable instead of requiring long compiler commands by hand.

### `.gitignore`

Lists generated build products such as `build/`, object files, libraries, and executables that Git should not track. These files can be recreated by building, so excluding them keeps source control focused on the files needed to develop the project.

## Editor and build support

### `.vscode/settings.json`

Adds the project tools directory and the MSYS2 UCRT64 compiler directory to the integrated terminal's `PATH`. This lets VS Code terminals find the Windows Make wrapper and GCC without changing the machine-wide environment.

### `.vscode/tasks.json`

Defines the VS Code tasks **Crawler: Build**, **Crawler: Test**, and **Crawler: Clean**. These tasks call the corresponding Make targets from the project directory, so common commands are available through VS Code's task interface.

### `tools/make.cmd`

A small Windows command wrapper that checks whether `mingw32-make.exe` is available, then forwards its arguments to it. It lets the project invoke `make` from PowerShell/VS Code while using the MSYS2 Make executable.

## Public headers

### `include/project.h`

Declares the public library API: `CrawlerStatus`, `CrawlerConfig`, `CrawlerStats`, `crawler_run`, and `crawler_status_string`. The demo and other client programs include this header to use the library. A public header is needed to define a stable contract between the library implementation and its callers.

### `include/url.h`

Reserved public declarations for URL-related functionality as the URL module is implemented. It is currently empty of declarations; the placeholder header makes the planned module boundary visible and gives future URL code a place for interfaces shared across modules.

## Library source (`src/`)

### `src/project.c`

Implements the current public API. It clears the caller's statistics, validates configuration values, and maps status codes to readable messages. At present valid configurations return `CRAWLER_NOT_IMPLEMENTED`; this file is the coordinator where later crawl behavior will be connected.

### `src/url.c`

Placeholder for URL parsing, validation, canonicalization, and resolution of relative links. The crawler needs this module to interpret links consistently, create absolute URLs, and identify equivalent URLs.

### `src/http.c`

Placeholder for downloading a page over HTTP(S), including timeout and response handling. A crawler needs page contents before it can parse and follow links.

### `src/html.c`

Placeholder for parsing downloaded HTML and extracting information such as titles, headings, paragraphs, links, and metadata. This keeps document parsing separate from networking and crawl control.

### `src/queue.c`

Placeholder for the FIFO queue of URLs waiting to be crawled, together with each URL's depth. A queue supports breadth-first traversal and helps enforce the configured maximum depth.

### `src/visited.c`

Placeholder for tracking normalized URLs already visited or scheduled. It prevents duplicate work and repeated visits to the same page.

These five module files currently contain placeholders only. Their descriptions explain planned responsibilities, not completed features.

## Demo and tests

### `examples/demo.c`

A standalone command-line example that accepts a start URL and optional maximum depth, builds a configuration, calls the public API, and prints a status or crawl statistics. Keeping this program separate demonstrates how a client uses the reusable library.

### `tests/test_crawler.c`

Checks the current API foundation with assertions: invalid argument handling, statistics initialization, the not-implemented status for valid input, and readable messages for known and unknown status values. Tests help detect regressions as the API and implementation grow.

## Project documentation

### `documentation/api.md`

Explains the API contract, configuration fields, statistics, function behavior, and status codes. It helps library users understand valid inputs and ownership expectations.

### `documentation/architecture.md`

Describes module boundaries and the planned flow of data through the crawler, plus the responsibilities and future crawl rules for each module. It guides implementation decisions as the project grows.

### `documentation/memory.md`

Documents memory ownership and allocation rules for the project. C requires explicit decisions about who allocates and frees memory, so these rules help prevent leaks, invalid frees, and dangling pointers.

### `documentation/progress.md`

Tracks the project parts and current completion status. It distinguishes implemented work from planned work so contributors can see what is ready and what remains.

### `documentation/testing.md`

Records the testing approach and instructions for checking the project. It gives contributors a repeatable way to validate changes and explains what the tests cover.

## Generated build output (`build/`)

Files in this directory are created by the Makefile and are ignored by Git. They are listed in the tree to show the current workspace, but they are not hand-maintained source files.

- `project.o`, `url.o`, `http.o`, `html.o`, `queue.o`, and `visited.o` are compiled object files, one per C module.
- `libcrawler.a` is the static library archive made from those object files. The demo and tests link against it.
- `crawler-demo.exe` is the built command-line demo.
- `test-crawler.exe` is the built test program.

## Keeping this guide current

Whenever a project file or directory is added, removed, renamed, or moved, update the project structure and the relevant explanation in this page as part of the same change. For a new source, header, test, tool, or documentation file, explain its definition, use, and why the project needs it. Generated files can change during builds; keep the `build/` examples representative and continue to treat that directory as generated output.
