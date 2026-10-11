# Memory and error-handling contract

## Public API

`CrawlerConfig` and its `start_url` are borrowed by `crawler_run` and must remain valid until it returns. The caller owns `CrawlerStats`; the function initializes it before processing. The current API returns status codes and does not allocate caller-visible memory.

## Internal modules (to define during implementation)

For every allocating function, document whether returned strings/buffers are owned by the caller or module, how they are freed, and what output state is guaranteed after failure. Free partially built results on all error paths. Report invalid input, allocation, network, and parse failures via `CrawlerStatus`; do not terminate the process from library code.
