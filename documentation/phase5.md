# Phase 5: a FIFO queue and a visited URL set

Historical Phase 5 scope/guide. Its statements that integration is future work
describe that delivery. Phase 6 now integrates these unchanged container APIs;
see `../prompt history/2026-11-08-phase6-design-decisions.md`.

These are independent C11 library containers, not a running crawler.
`project.c`, `url.c`, `http.c`, `html.c` and all pre-existing tests are unchanged.
No network, recursion, document downloader, UI, AI or concurrency is added.

## Why these representations?

A queue node holds one owned URL, one `int` depth and a pointer to the next
node. `head` points to the oldest item; `tail` points to the newest. Enqueue
links at tail; dequeue removes head. There is no shifting/growing array,
fixed capacity or third-party package. Linking/removal is O(1); enqueue also
copies L characters, so the full enqueue is O(L). Size/empty queries are O(1).
Queue destruction walks n nodes: O(n), freeing all owned strings too.

The visited set is a singly linked list of canonical URL strings. It is
intentionally simple, not a hash table. Normalization costs O(L); finding an
equal string costs O(n*L) worst case. A unique insert then links one node at
head. Storage is O(n + total stored URL bytes). Large-scale crawl performance
is not the Phase 5 objective. The 1200-key test is a correctness check, not a
claim of production-scale performance.

## Start with zeroed containers

```c
UrlQueue queue = {0};
UrlQueueItem item = {0};
VisitedSet seen = {0};
/* Check queue_init(&queue) == QUEUE_OK and
 * visited_init(&seen) == VISITED_OK before further work. */
```

The caller owns these small structs (often on the stack). `{0}` is required
before first use, including before init/free: an uninitialized pointer is not
safe to inspect or free. Init is allocation-free and rejects populated objects
to avoid losing their pointers. Do not assign/copy populated containers by
value; that would give two structs ownership of the same allocations.
Fields are exposed to keep this university API small, but are private by
convention: use the functions, not hand-edited counts or node pointers.

## Public queue functions (`include/queue.h`)

| Function | Arguments, result and purpose |
|---|---|
| `queue_init(UrlQueue *queue)` | Borrow a zeroed container. Returns `QUEUE_OK`; NULL/nonempty is `QUEUE_ERROR_INVALID_ARGUMENT`. |
| `queue_size(const UrlQueue *queue, size_t *size)` | Write the number of pending items through `size`; NULL arguments are invalid. |
| `queue_is_empty(const UrlQueue *queue, int *empty)` | Write 1 if empty, otherwise 0. Does not remove anything. |
| `queue_enqueue(UrlQueue *queue, const char *url, int depth)` | Borrow a nonempty NUL-terminated string, copy it, associate nonnegative depth, append to tail. Returns OK, invalid argument, OOM or overflow. |
| `queue_dequeue(UrlQueue *queue, UrlQueueItem *item)` | Remove oldest node and TRANSFER its string/depth into an item whose `url` is NULL. Returns OK or EMPTY; NULL/populated output is invalid. No allocation. |
| `queue_item_free(UrlQueueItem *item)` | Free its owned URL and reset URL/depth to zero. NULL/repeated cleanup safe. |
| `queue_free(UrlQueue *queue)` | Free every remaining node/string and reset head/tail/count. NULL/repeated cleanup safe; reuse is allowed. |

Failures leave the queue and dequeue output unchanged. All strings supplied to
the queue are borrowed: they are never modified or freed. Enqueue's node and
string allocations must both succeed before the node becomes visible.
If the second allocation fails, the temporary node is freed. `SIZE_MAX` guards
count addition and `length + 1`; there is no optional queue bound.
Depth 0, 1, 2, 12345 and INT_MAX are valid; negative values are rejected.
The queue intentionally does not parse URLs: Phase 6 will schedule validated
canonical URLs, but the container is a nonempty string/depth FIFO.

Example:

```text
enqueue("https://example.com/A", 0)
enqueue("https://example.com/B", 1)
enqueue("https://example.com/C", 2)
dequeue -> A,0; free returned item
dequeue -> B,1; free returned item
dequeue -> C,2; free returned item
dequeue -> QUEUE_EMPTY (not an error that destroys the queue)
```

Freeing the queue does not free a URL already transferred to an output item.
That string belongs to the caller until `queue_item_free`.

## Public visited functions (`include/visited.h`)

| Function | Arguments, result and purpose |
|---|---|
| `visited_init(VisitedSet *set)` | Accept a zeroed set; returns `VISITED_OK`. Reject NULL/nonempty objects. |
| `visited_size(const VisitedSet *set, size_t *size)` | Write stored unique key count; NULL arguments are invalid. |
| `visited_contains(const VisitedSet *set, const char *url)` | Borrow input, normalize a temporary key, search, free temporary; return `VISITED_PRESENT` or `VISITED_NOT_FOUND`, or a real error. Does NOT insert. |
| `visited_insert(VisitedSet *set, const char *url)` | Normalize input. If equal key exists, free temporary and return PRESENT without growth. Otherwise allocate a node and retain the canonical string; return ADDED. |
| `visited_free(VisitedSet *set)` | Free keys and nodes, reset set. NULL/repeated cleanup safe; no keys are returned to callers. |

NULL input/container/output is `VISITED_ERROR_INVALID_ARGUMENT`. Empty,
relative, non-HTTP(S), malformed and too-long URLs are
`VISITED_ERROR_INVALID_URL`. Normalizer malloc failures are
`VISITED_ERROR_OUT_OF_MEMORY`, never PRESENT/NOT_FOUND/INVALID_URL.
Count overflow is `VISITED_ERROR_OVERFLOW`. A failed unique insertion frees
the temporary canonical key and changes neither nodes nor count.
Even duplicate checks can fail to allocate their temporary normalized key;
callers must inspect the full status, not treat every non-ADDED result as a
duplicate. `visited_size` and init use OK, whereas insert success is ADDED or
PRESENT; membership uses PRESENT or NOT_FOUND.

`url_normalize` is reused without edits. Thus:

```text
insert HTTPS://Example.COM:443/docs#top -> ADDED
insert https://example.com/docs#other  -> PRESENT (same canonical key)
insert https://example.com/docs?x=1    -> ADDED
insert https://example.com/docs?x=2    -> ADDED
```

The existing normalizer keeps `/docs` distinct from `/docs/`, preserves query
ordering and does not equate `%7E` with `~`. These are existing library rules,
not new parser behavior. User inputs stay unchanged. Mutating the original
caller buffer after insertion does not change the stored key.

## Pointers and heap memory

- `char *` points at mutable character storage; every owned URL includes a
  final NUL byte. `const char *` borrows bytes that this function must not edit.
- `UrlQueue *` / `VisitedSet *` let a function update the caller's container.
  The functions do not allocate/free the container struct itself.
- `size_t *` / `int *` let query functions write one result into caller storage.
- `char **` in private `normalize_key` passes the address of a pointer so
  `url_normalize` can return a newly allocated string.
- A node's `next` pointer links another node; NULL means the end. Save `next`
  before freeing the current node so the loop never reads freed memory.
- `malloc` allocates storage; check NULL before use. `memcpy` copies the URL
  including its NUL. `free` releases exactly one owned allocation.
- No realloc/calloc or resizable arrays are used by the new containers.
  The existing URL normalizer owns/frees its intermediate allocations and
  returns one independent canonical allocation on success.

Private `normalize_key` maps the existing URL statuses (preserving OOM).
Private `has_key` only borrows strings/list nodes and compares them.
Test functions generate data, check expected states, and count allocations;
allocator replacements live only in separate test objects.

## Testing and boundaries

```sh
make clean
make
make test-phase5
make test-phase5-alloc
make test
make test-html-alloc
```

`test_queue.c`: empty/invalid states, exact FIFO and depths, 1200 simultaneous
nodes, enqueue during dequeue, owned copies, transferred output lifetime,
undrained/repeated cleanup, reuse, INT_MIN rejection and INT_MAX acceptance.
`test_visited.c`: initial/duplicate/missing outcomes, invalid absolute URL
requirements, canonical equivalence, path/query distinctions, 1200 keys,
owned copies, long input rejection, repeated cleanup/reuse.

Both test synthetic SIZE_MAX counts, restoring them immediately afterward:
no machine can realistically allocate SIZE_MAX nodes. A SIZE_MAX-byte string
cannot be safely materialized here, so the theoretical strlen+1 boundary is
guarded but NOT runtime-tested at that impossible length.

`test_phase5_alloc.c` fails every malloc position in representative enqueue,
unique insert, duplicate insert, contains-missing and contains-present actions.
It checks rollback, preexisting FIFO/keys, retries and zero tracked live
allocations. The instrumented queue/visited/url objects do NOT enter the
production archive. The shared CHECK macro remains active under NDEBUG.
Full measured commands/results are in the prompt-history handoff.

## Phase 6 prerequisites — explanation ONLY, not implemented

Before writing an engine, agree on its orchestration/status/statistics and
retry behavior. Existing configuration limits must be applied deliberately.
Do not increment an INT_MAX depth; bound/check before computing a child depth.
Use a downloaded page's final URL for relative extraction (redirect coverage
already exists). No authentication/site/crawling-policy assumptions are made
by these Phase 5 containers.

A future single-threaded BFS can check canonical membership, enqueue a URL,
then mark it scheduled. Marking on scheduling prevents cycles/duplicate queued
work, not proof of a successful download. FIFO puts new children behind older
items; the caller assigns depth. If enqueue fails, do not mark visited.
If marking fails AFTER enqueue, there is no atomic cross-container rollback:
STOP scheduling, return a real failure and safely clean up rather than
continue with unmarked queued work or report success. Restart/recovery rules
must be explicit in Phase 6; neither silently dropping work nor blindly
re-enqueueing an already queued URL is safe. Never interpret a contains error
as missing/present. Download failures and retry policy also need explicit
handling; being scheduled is not being successfully fetched.

Queue/visited alone do not supply integrated BFS, atomic scheduling,
redirect-alias tracking or retry state. If Phase 6 requires resumable
cross-container transactions, design and test that separately with approval.
The current `crawler_run` remains NOT_IMPLEMENTED; no Phase 6 code is included.
