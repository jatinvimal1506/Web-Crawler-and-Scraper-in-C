# Testing

Run from the project folder in the configured Windows PowerShell/MSYS2 environment:

```powershell
make clean
make
make test
.\build\test-url.exe
```

The Part 1 test checks invalid arguments, statistics initialization, status messages, and the valid not-implemented response. The URL suite checks normal HTTP/HTTPS parsing, each parsed component, status values, host and port validation, URL-size and DNS label/hostname boundaries, query and fragment handling, normalization, reference resolution, output state on errors, repeated cleanup, and the previously failing resolution/trailing-slash cases.

The suite uses deterministic inputs and does not use network access. Parts 3–6 are placeholders, so HTTP failures, HTML parsing, queues, visited tracking, and crawl restrictions are not tested. Allocation-failure injection and fuzzing remain unavailable. In this MSYS2 toolchain AddressSanitizer could not link because libasan is missing, and Valgrind is unavailable; do not treat repeated successful runs as proof that there are no leaks.
