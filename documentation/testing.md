# Testing

Run the Part 1 and Part 2 checks from the project folder:

```powershell
make clean
make
make test
.\build\test-url.exe
```

`test-crawler.c` checks invalid API arguments, statistics initialization, and the not-implemented response for a valid request. `test_url.c` checks parsing, optional URL components, invalid schemes/hosts/ports/characters, long inputs, normalization, and relative resolution.

HTTP errors, HTML parsing, queue behavior, visited-set behavior, crawl depth/domain rules, and network failures are not tested in this branch because Parts 3–6 are not implemented. Integrated testing remains Part 7 work. Use AddressSanitizer or Valgrind only if available in the selected Windows environment; they have not been run as part of the current verification.
