# Memory and error-handling contract

## Part 1 API

The caller owns `CrawlerConfig`, its `start_url`, and `CrawlerStats`. These values remain valid during `crawler_run`; stats are zeroed before validation when the stats pointer is non-NULL. The Part 1 stub allocates no memory and returns a `CrawlerStatus` rather than exiting.

## Part 2 URL API

Initialize `ParsedUrl` as `{0}` before its first `url_parse` call. The function builds into a temporary structure and publishes component pointers only on success. It rejects a non-empty output rather than replacing existing allocations. Each successful component belongs to the caller until `url_free`; `url_free` releases the fields and resets them, so calling it a second time is safe for a structure it has already cleared.

`url_normalize` and `url_resolve` set `*output` to NULL before work. Successful outputs are caller-owned heap strings and must be released with `free`. Internal parsed components, path buffers, candidates, and failed output allocations are freed before returning. Allocation failure returns `URL_ERROR_OUT_OF_MEMORY`; deterministic allocation-failure injection is not part of the current test setup.

All URL functions return a `UrlStatus`. `url_status_message` returns static storage that callers must not alter or release. Invalid input does not cause the library to exit. See `documentation/api.md` for the status list and supported URL scope.
