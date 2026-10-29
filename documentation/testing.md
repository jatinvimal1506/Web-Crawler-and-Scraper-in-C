# Testing

Run from the project folder in the configured Windows PowerShell/MSYS2 environment:

```powershell
make
make test
```

`make` also builds the pipeline demo. Run `.\build\demo_pipeline.exe` for the default URL or pass a URL argument. The Part 1 test checks invalid arguments, statistics initialization, status messages, and the valid not-implemented response. The URL suite checks normal HTTP/HTTPS parsing, each parsed component, status values, host and port validation, URL-size and DNS label/hostname boundaries, query and fragment handling, normalization, reference resolution, output state on errors, repeated cleanup, and the previously failing resolution/trailing-slash cases.

The HTTP suite includes live-network probes, so those checks may be skipped or fail when the environment blocks network access. It also checks URL rejection, HTTP failures when reachable, error categorization, and response cleanup. HTML extraction, queues, visited tracking, and crawl restrictions are not implemented. Allocation-failure injection and fuzzing remain unavailable. In this MSYS2 toolchain AddressSanitizer could not link because libasan is missing, and Valgrind is unavailable; do not treat repeated successful runs as proof that there are no leaks.
