# Testing

## Part 1 automated tests

Run `make test`. The current test program verifies:

- NULL configuration and NULL statistics pointers.
- NULL and empty starting URLs.
- Invalid domain flag and non-positive timeout.
- Valid configuration returns `CRAWLER_NOT_IMPLEMENTED`.
- Statistics are zeroed on both valid and invalid calls when the output pointer exists.
- Every defined status and an unknown status value produce readable messages.

## CLI checks

Run `make` first. In PowerShell:

```powershell
.\build\crawler-demo.exe https://example.org 2
.\build\crawler-demo.exe
.\build\crawler-demo.exe https://example.org nope
```

The first command should print a clear not-implemented message and exit successfully. The latter commands should report invalid usage and exit non-zero.

## Later test coverage

Once the corresponding modules exist, add tests for URLs, HTML, relative and duplicate links, domain/depth boundaries, network failures, large input, and cleanup on allocation failures. Use AddressSanitizer or Valgrind only when available for the selected Windows environment. These later behaviors are not tested or claimed in Part 1.
