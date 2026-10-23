# Memory and error-handling contract

## Public API

The caller owns `CrawlerConfig`, its `start_url` string, and `CrawlerStats`. These objects and the URL string must remain valid during `crawler_run`. The function initializes all statistics to zero before validating the configuration whenever the stats pointer is non-NULL.

The Part 1 API allocates no caller-visible or dynamic memory. `crawler_status_string` returns a pointer to a static string; do not free or modify it. The library reports errors with a `CrawlerStatus` value and never exits the process.

## Future internal modules

Before adding allocating functions, document whether returned strings and buffers belong to the caller or module, how they are freed, and what output state is guaranteed after failure. Free partially built results on every error path. Report invalid input, allocation, network, and parse failures through `CrawlerStatus`.
