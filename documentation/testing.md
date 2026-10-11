# Test plan

- Valid and invalid start URLs; null and empty arguments.
- Normal HTML with title, headings, paragraphs, metadata, and links.
- Missing title, empty HTML, malformed HTML, large HTML.
- Absolute and relative links, normalized duplicates, repeated discoveries.
- Same-domain filtering and depth boundary behavior (root is depth 0).
- Unreachable host, timeout, redirect, and non-success HTTP status.
- Allocation and cleanup failure paths; use AddressSanitizer/Valgrind where available.
