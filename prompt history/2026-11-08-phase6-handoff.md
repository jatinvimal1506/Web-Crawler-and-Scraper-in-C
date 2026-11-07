# Phase 6 completion, acceptance evidence and student handoff

Execution clock observed **2026-11-07 UTC**. The 2026-11-08 filenames are
explicitly requested labels, not invented execution dates.

Implementation/QA: **COMPLETE on the verified Linux environment**, subject to
the explicitly stated coverage and scope limitations below. Git upload status
is recorded separately; a local commit is not proof of upload.
**NO PHASE 7 STARTED.**

## Baseline and folder boundaries

- Canonical: https://github.com/jatinvimal1506/Web-Crawler-and-Scraper-in-C.git
- Authorized branch: `main`.
- Verified clean initial C-only Git HEAD and live GitHub main:
  `e5a99d03353abf79f9d0b3fe4d551d777d2bbae5`.
- Previous Phase 5 implementation:
  `9667743d37a487cec9de8f9ccf2a1af186c36616`.
- Builds below ran from `DeepDownload-C/`, the C-only source folder. Reviewed,
  byte-identical intended changes are copied to the independent Git checkout
  `.local/github-sync/deepdownload-c` for delivery. The outer Replit web
  scaffold is neither the C product nor part of its GitHub push.
- Existing URL/HTTP/queue/visited implementations and headers and previous
  URL/HTTP/HTML/queue/visited tests were byte-compared against the baseline:
  unchanged. The required basic API valid-call assertion is the exception.
- All four preexisting prompt-history Markdown files were compared unchanged.

## Acceptance matrix — every work-order section

PASS refers only to the stated evidence, not unverified platforms/tools.

| ID | Requirement and evidence | Result |
|---|---|---|
| 0 | No mode/config/paid-feature/deploy action; C11-only implementation/tests, no new dependency, no Phase 7. Existing expertMode=true was read, not changed. | PASS actions/scope; actual UI tier NOT VERIFIED |
| 1 | Clean initial branch/live remote equal e5a99d0; original four-field config/five-field stats/old enum values preserved. Existing module APIs retained. | PASS |
| 2 | Actual `crawler_run` calls queue, visited, URL, HTTP and HTML in a FIFO loop; root/siblings/grandchild real requests and all counters checked. Not standalone demos chained together. | PASS |
| 3a | Root depth 0, max_depth 0/1/2/SIZE_MAX; exact FIFO request order; synthetic INT_MAX stops without increment; negative item/dequeue invariant failures abort. | PASS |
| 3b | Contains distinguishes PRESENT/NOT_FOUND/errors; enqueue-before-insert; 331 individual allocation failure positions, including 6 queue OOMs, 90 visited-insert OOMs and 24 post-enqueue mark OOMs; no surviving counted allocations. | PASS |
| 3c | No automatic retry; root HTTP/parse failures abort; child 404/500/PDF/NUL/timeout continue to PARTIAL; critical OOM/invariant/budget errors abort. | PASS |
| 3d | Known redirect aliases skip queued direct keys, including failed 404/NUL targets; both alias orders tested. Unknown aliases can still repeat final HTTP fetch (explicitly tested/documented). | PASS stated policy, not universal exact-once fetch |
| 4a | Existing URL canonicalization, fragments/case/default-port equivalence in C in-memory engine/URL/visited tests. Literal relative links and final-URL relative base tested through real HTTP. | PASS |
| 4b | Exact canonical hostname vs localhost/IP/subdomain/lookalike; both modes pin effective port; direct off-port/default-service links not fetched. Mode 0 allows other hostname on same port. | PASS |
| 4c | Off-host and off-port redirects actually fetched then rejected before parsing; no leaked child. Both modes tested for off-port redirect. | PASS accepted post-download policy; network-level refusal NOT IMPLEMENTED |
| 4d | Existing TLS verification/default User-Agent/HTTP(S)-only redirects untouched; old local protocol rejection tests pass. Controlled new demos only; no bypass feature. No automatic robots-policy decision engine is claimed. | PASS scope; full robots enforcement NOT IMPLEMENTED |
| 5 | 100-attempt chain stops exactly at /p/99; 1000 duplicate candidates fit, 1001 fail; scheduled cap includes root/aliases and cleans queued nodes. Bounded parser prevents unlimited output arrays. Existing URL/5MiB/redirect/timeouts retained. | PASS |
| 5b | SIZE_MAX additions guarded in source, stats bounded in real tests; INT_MAX explicitly injected. Direct near-SIZE_MAX stats arithmetic is unreachable with reset stats/fixed budgets, not separately forced. | PASS guard inspection; direct synthetic size-counter overflow NOT TESTED |
| 6 | All five stats asserted across 25 site scenarios; duplicate/off-scope href occurrences count; HTTP_OK bytes include subsequent parse/scope rejection; error bodies not counted. Zero invalid-call stats preserved. PARTIAL/LIMIT never reported as OK. | PASS |
| 7a | C localhost fixture, real request-order records, siblings/grandchild, depths/cycles/duplicates, relative links, empty HTML, malformed/unsafe href, iframe/script exclusion, host/port filters, aliases, errors and budgets. | PASS: 375 checks, 25 site scenarios plus boundaries |
| 7b | Empty transport response, root refused connection, child timeout; binary PDF and embedded NUL rejection; invalid URLs/config; queue/set cleanup and injected failures. Original invalid/stat/status tests retained. | PASS |
| 7c | No test-only allocation hooks/stubs in release archive; test instrumented objects separate; original Phase 1–5 suites pass. | PASS |
| 8a | Fresh make clean/make/make test and HTML allocation target, strict C11 warnings; full command/exits below. | PASS Linux, zero warnings |
| 8b | 11 deterministic executables under ASan/UBSan with -Werror, leak detection disabled; no reported address/undefined behavior errors. | PASS for ASan/UBSan |
| 8c | LeakSanitizer enabled probe exits 1 with ptrace restriction; Valgrind absent. Test allocator accounting separately zero. | LeakSanitizer/Valgrind NOT VERIFIED, not mislabeled PASS |
| 8d | Windows/MSYS2 cmd/.exe/RUN handling retained; portable C stub/allocation tests. POSIX local fixtures explicitly SKIP on native Windows. | Recipe retained; native Windows build/runtime NOT VERIFIED |
| 9 | C BFS CLI prints real status/all stats, conservative URL/depth/domain args, actual controlled four-page demonstration; old demos preserved with stale text fixed. | PASS demo exit 0; missing URL exit 2 |
| 10 | Full accessible instruction + decisions + this requirement/function/QA handoff, existing history retained; current README/architecture/API/progress/testing updated. | PASS |
| 11 | Only 25 reviewed C-project source/tests/Makefile/Markdown files committed; safe non-force push exit 0; independent live remote SHA equals code commit. | PASS source upload; exact receipt below |
| 12 | Final response must report commit/live SHA, tool limits, UI tier uncertainty, Phase 7 readiness and NO PHASE 7 STARTED; no subsequent feature starts. | Handoff prepared; Git finalization separately verified |

## What changed (C-only Git payload)

| Files | Purpose |
|---|---|
| `src/project.c`, `include/project.h` | Real BFS, stats/status/scope/limits, complete ownership cleanup; additive enums/constants only, unchanged config/stat layouts. |
| `src/html.c`, `include/html.h` | Add bounded extraction using the same scanner; original unlimited API and all existing tests preserved. |
| `examples/bfs_demo.c` | New conservative real BFS CLI, all stats/status and nonzero incomplete/error exit. |
| `examples/demo.c`, `examples/demo_pipeline.c` | Retain old examples, replace obsolete not-implemented explanations with real results; demo cleans HTTP globals. Pipeline still makes additional standalone requests. |
| `tests/test_crawler.c` | Required old valid-run assertion now checks successful controlled empty-page crawl; original invalid/stat/status cases remain, new status strings checked. |
| `tests/crawler_stub.c`, `tests/crawler_stub.h` | Portable C transport for basic API and exhaustive allocator tests, never production library. |
| `tests/test_crawler_alloc.c` | Allocator accounting/failure sweep and queue/visited/invariant/depth/budget cleanup checks. |
| `tests/crawler_fixture.c`, `tests/crawler_fixture.h` | C loopback fixture: two listening ports, one reserved refused port, request records, routes/bodies/errors/redirects. POSIX test-only. |
| `tests/test_crawler_engine.c` | Real module integration/order/all-stat/status tests, plus bounded-parser boundary. |
| `tests/crawler_fixture_main.c` | Controlled fixture launcher/real CLI demonstration; no browser/Python. |
| `Makefile` | New build/test targets and separate fault objects; original Windows/Linux branching preserved. |
| `README.md`, `documentation/{api,architecture,progress,testing}.md` | Current implemented/deferred state, commands, contracts, warnings and limits. |
| `documentation/phase5.md` | Historical context note only; unchanged Phase 5 ownership/teaching explanation remains. |
| Three `prompt history/2026-11-08-phase6-*.md` files | Complete order, decisions and this verifiable handoff. |

Outer `replit.md` is updated only as collaborator context to identify the
source/Git checkout and current phase boundaries. It is NOT in the C GitHub
payload. No outer TS/React source/config or agent-mode setting is changed.

## Public behavior, pointers and owned memory

A pointer is an address. `const char *` here is borrowed text the function
must not change/free. `T *result` lets a function write the caller's object;
it does not mean the object itself was allocated by the callee. A NULL
pointer is no object and must be checked before access.

`crawler_run(const CrawlerConfig *, CrawlerStats *)` borrows both objects.
The URL remains caller-owned, all internal copies are freed before return.
The caller owns stats storage; no result pointer requiring free escapes.
`crawler_status_string(CrawlerStatus)` returns a borrowed static string.

Example: root `/site` depth 0 queues `/a` and `/b` at depth 1, then `/g` at
depth 2. Actual request order is `/site`, `/a`, `/b`, `/g`; duplicates/back
links do not add requests. With max_depth=1, `/g` is never fetched.
`HTTPS://EXAMPLE.TEST:443/root#seed` and `https://example.test/root` follow
existing normalization to the same scheduled key in the C stub test.

`html_extract_links_limited(const char *, const char *, size_t, HtmlLinks *)`
borrows HTML/base URL and returns owned array/independent strings on success.
Use the unchanged `html_links_free` to release them. If a valid extra link
would exceed maximum, return HTML_ERROR_LIMIT and leave empty output after
freeing the prefix. `html_extract_links` calls the shared implementation with
SIZE_MAX. `html_status_message` returns borrowed static text.

No queue/visited API is changed. `queue_enqueue` clones borrowed input;
`queue_dequeue` transfers its string to UrlQueueItem; queue_item_free releases
that item. Visited owns normalized key copies. No string in a queue is an
alias into an HtmlLinks/HTTP/caller buffer, so cleaning one module cannot leave
another with a dangling URL. Every partial-failure path releases what it owns.

### Every added/changed production helper

| Function | Arguments/result and memory purpose |
|---|---|
| `add_count(size_t *, size_t)` | Borrowed counter, checks available SIZE_MAX space before adding; no heap. |
| `visited_error(VisitedStatus)` | Pure OOM-vs-invariant error mapping; no ownership. |
| `effective_port(const ParsedUrl *)` | Reads validated borrowed parts, returns numeric explicit/default port. |
| `in_scope(const Crawl *, const char *, int *)` | Parses temporary owned URL parts, compares seed host/port, frees parts, writes allowed only on success. |
| `remember(VisitedSet *, const char *)` | Insert a canonical key into borrowed set, which owns its independent clone; ADDED/PRESENT accepted, true errors returned. |
| `schedule(Crawl *, const char *, int)` | Check set, bound key count, enqueue copy, then mark copy; error aborts caller's whole crawl. Borrowed key is not freed here. |
| `http_failure(HttpStatus)` | Pure documented status mapping; no heap. |
| `process_page(Crawl *, const UrlQueueItem *)` | Borrows dequeued item; owns local response/links/final-key allocations and frees them on success/failure; schedules owned child copies. |
| `crawler_run`, `crawler_status_string` | Public behavior above; all containers/seed/item cleanup at one exit, no caller-visible allocations. |
| HTML `append_href` | Existing decode/resolve helper gains maximum; frees temporary decoded/resolved strings on rejection, transfers resolved allocation into owned array only on success. |
| `html_extract_links`, `html_extract_links_limited`, `html_status_message` | Shared parser wrapper/implementation/new message as described above. |
| CLI `demo_depth(const char *, size_t *)` | Checks exact 0..10 text, writes borrowed scalar; no allocation. |
| `examples/bfs_demo.c:main` | Borrows argv URL; calls real engine, prints all stats/status, cleans global HTTP state; incomplete/error exits 1, malformed CLI 2. |
| Existing demo `main` functions | Print actual crawler outcome rather than claiming no integration; old pipeline separately invokes URL/HTTP demonstrations. |

### Every added/changed test/fixture helper

- `crawler_stub.c:copy_text` mallocs a clone; stub `http_download` allocates
  owned body/final/type (or cleans all on OOM), `http_response_free` releases
  them/reset output. Stub main modes provide the three-page graph or 1000/
  1001-anchor bodies; globals are test control only.
- `crawler_test_malloc`, `_calloc`, `_realloc`, `_free` count allocations and
  one selected failing position; failed realloc preserves its original block.
  Free(NULL) is safe. Live allocation count must return to zero per crawl.
- `crawler_test_enqueue`, `_insert` observe real container results and
  post-enqueue OOM; `_dequeue` calls real dequeue or injects an invariant/depth
  boundary. Instrumented objects only. `test_crawler_alloc:main` sweeps 331
  points, retries a new crawl after failures and checks large-frontier cleanup.
- `crawler_fixture.c:require` stops fixture on setup error; `route_id` maps a
  borrowed path to an integer; `build_body` writes into a bounded caller
  buffer; `crawler_fixture_body_size` calculates expected payload lengths.
- `send_all` borrows byte span, sends remaining bytes safely; `serve` builds
  bounded stack HTTP header/body, handles controlled response/redirect/delay/
  empty-transport behavior; neither returns stack pointers.
- `server_loop` selects the two listening loopback sockets, accepts/records/
  serves/closes each connection in one test-server process. It is not a
  multithreaded crawler or general server framework.
- `crawler_fixture_start` creates/binds sockets/pipe, forks server and registers
  cleanup; `_stop` kills/reaps it, closes owned parent descriptors and cleans
  HTTP global state; `_refused_port` returns reserved non-listening port;
  `_requests` drains bounded request records into borrowed caller storage.
  Windows versions explicitly stub/skip POSIX fixture execution.
- `test_crawler_engine.c:run` builds stack URL/config, verifies borrowed text
  unchanged plus status/all five stats; `order` checks exact request IDs/host
  flags; `main` runs 25 site scenarios, invalid/refused/bounded-parser cases.
- `crawler_fixture_main:main` starts the controlled site and either waits for
  Enter or forks/executes the actual BFS CLI, checks its real process exit,
  then cleans fixture resources.
- Existing `test_crawler:main` retains invalid/stat/status assertions and now
  expects one successful empty HTML page from the portable stub.

The fixture's additional process is a test server; production crawling
remains one thread and one synchronous HTTP call at a time.

## Literal QA commands and observed exits

Environment: Linux x86_64, GCC 16.2.0, GNU Make 4.4.1, libcurl 8.14.1.
Original Makefile appends C11/-Wall/-Wextra/-Wpedantic/-Iinclude to Nix's
dependency flags. No dependency installation or toolchain replacement.

Executed from `DeepDownload-C`:

```sh
make clean
make
timeout 180 make test
make test-html-alloc
make build/crawler-fixture
./build/crawler-fixture --demo
./build/crawler-bfs
```

First six commands: exit **0** each. Last command intentionally omits URL:
exit **2**, expected usage validation, not a failed build. No compiler
warnings/errors in clean/build/test/HTML-allocation/fixture-build logs.
Earlier targeted `make -s test-crawler-engine test-crawler-alloc` also exited 0.

| Suite/target | Actual result |
|---|---|
| Basic crawler API | Exit 0; original assert-based test emits no numeric count. Controlled empty HTML success, invalid/stats/status cases intact. |
| URL parsing + validation | Exit 0, both original success summaries; executable emits no numeric aggregate. |
| Original HTML | 14 groups passed; unchanged. |
| Queue | 3667 checks, 1200 URLs passed; unchanged. |
| Visited | 3670 checks, 1200 canonical keys passed; unchanged. |
| Phase 5 allocator | 3281 checks; failure points 2+10+9+9+9=39; zero tracked allocations. |
| New crawler integration | **375 checks, 25 site scenarios**, plus invalid/connection/bounded-parser checks; exact requests and all counters. |
| New engine allocator/invariants | **223355 checks; 331 individually injected allocation positions**, queue OOM=6, visited insert OOM=90, post-enqueue OOM=24; zero tracked allocations. |
| Local HTTP | 81 checks passed. |
| HTTP metadata-allocation fixture | 128 total checks passed, including the shared 81 plus metadata OOM/cleanup coverage; do not double-count these as all unique assertions. |
| Original network-dependent HTTP | 7/7 groups passed, no SKIP observed; normal HTTPS response 200, 577 bytes in this run. This suite depends on public network availability, unlike new engine tests. |
| Optional HTML allocator | All 143 failure positions passed, empty output/no tracked allocations. |

Normal full `make test`: 11 executables, exit 0, **zero observed failures,
zero observed SKIPs**. The separate HTML allocation executable also passed.
No single aggregate assertion count is invented across groups/suites.

### Actual controlled CLI observation

The launcher generated `http://127.0.0.1:35873/site` in this run and executed
`crawler-bfs URL 2 1`. Actual output: status success; pages visited **4**,
failed **0**, links **8**, maximum attempted depth **2**, bytes **328**.
Port is ephemeral; future runs must use their printed port.

### Sanitizers and release isolation

Literal sanitizer command (CFLAGS/LDFLAGS retain supplied Nix dependency paths):

```sh
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 timeout 180 \
make BUILD=build/sanitize \
CFLAGS="$CFLAGS -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined" \
LDFLAGS="$LDFLAGS -fsanitize=address,undefined" \
test-unit test-phase5-alloc test-crawler-engine test-crawler-alloc \
test-http-local test-http-alloc test-html-alloc
```

Exit **0**, all **11 deterministic executables** pass, same numeric results
above, no compiler warning, ASan error or UBSan runtime finding observed.
The original public-network HTTP executable is not in this sanitizer command.

Actual LeakSanitizer-enabled probe:

```sh
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 \
./build/sanitize/test-crawler-engine
```

Exit **1**, fatal tool message:
`LeakSanitizer does not work under ptrace (strace, gdb, etc)`.
Therefore real LeakSanitizer scanning is **NOT VERIFIED**, not passed.
Valgrind is absent. Explicit tracked application-object accounting passes
independently; it does not certify libcurl/internal process allocations.

`ar t build/libcrawler.a` contains exactly project.o, url.o, http.o, html.o,
queue.o, visited.o. `nm` hook scan found NO crawler_test_/phase5_test_/
__wrap_/test_fail symbols. No test stub/object is archived.

## Documentation and exact prompt-history location

Inside the C product: `DeepDownload-C/prompt history/`.
The same relative `prompt history/` directory belongs to its independent Git
checkout and GitHub repository. Markdown only:

1. `2026-11-07-historical-requests.md` — previous accessible requests, retained.
2. `2026-11-07-project-history.md` — prior factual/retrospective milestones, retained.
3. `2026-11-07-phase5-authoritative-instruction.md` — complete prior order, retained.
4. `2026-11-07-phase5-handoff.md` — prior verification/receipt, retained.
5. `2026-11-08-phase6-authoritative-instruction.md` — complete accessible current
   instruction, sections 0–12 and quoted user text, not a summary; transport
   wrappers excluded and no invented message time.
6. `2026-11-08-phase6-design-decisions.md` — central stats/status/scope/budget/
   scheduling/ownership contracts and alternatives/reasons.
7. `2026-11-08-phase6-handoff.md` — this acceptance/function/QA/delivery record.

Current README/API/architecture/progress/testing guides distinguish implemented
Phases 1–6 from future Phase 7. Older Phase 4/5 guides/history remain evidence
of those stages, not rewritten to pretend they included later integration.

## Git delivery receipt

Pre-delivery live `main` still equals the clean e5a99d0 baseline above.
Origin is the configured Replit GitHub proxy URL for the canonical repository:
`https://github.replit.internal/jatinvimal1506/Web-Crawler-and-Scraper-in-C.git`.
No remote replacement, force push, secret read or unrelated scaffold staging.

### Actual successful source upload

- Source commit: **`c11704e732ce45007abfc6d53ccadb4e65980a16`**.
- Subject: Implement Phase 6 bounded C11 BFS crawler with deterministic
  integration and fault tests.
- Parent remains the verified `e5a99d03353abf79f9d0b3fe4d551d777d2bbae5`;
  no divergence, merge, reset or history overwrite.
- `git push --dry-run origin main`: exit **0**, write preflight only.
- **ONE** actual `git push origin main`: exit **0**, normal NON-FORCE push.
- Subsequent independent `git ls-remote origin refs/heads/main`: exit **0**,
  returned **`c11704e732ce45007abfc6d53ccadb4e65980a16`**, exactly matching
  `git rev-parse HEAD`.
- C-only checkout status after source push: clean `main...origin/main`,
  neither ahead nor behind.
- Payload: 25 reviewed C-project files, 1956 insertions/64 deletions.
  No outer web scaffold, executable, build/cache output, credential or secret.
- Commit URL:
  https://github.com/jatinvimal1506/Web-Crawler-and-Scraper-in-C/commit/c11704e732ce45007abfc6d53ccadb4e65980a16

This successful push proves the current GitHub authentication permitted THIS
repository upload; it is not a claim about unrelated account permissions.

This receipt update changes only this Markdown file and is explicitly
authorized by Phase 6 section 11. Its separate normal non-force push must
check live main still equals the source commit first, and verify remote HEAD
afterward. The receipt commit cannot literally contain its own hash; its
latest SHA/upload result is therefore verified in the final Replit response.

## Remaining limitations and Phase 7 gate

- Native Windows/MSYS2 build/runtime NOT VERIFIED; recipe support retained,
  POSIX test fixtures explicitly skip there.
- LeakSanitizer/Valgrind NOT VERIFIED as explained; ASan/UBSan and explicit
  allocator accounting are separate positive evidence.
- Scope enforcement is post-download for redirects, NOT network isolation;
  both modes intentionally restrict port. Unknown redirect aliases may repeat
  final fetches. No automatic retry, resumability or per-page error report.
- Existing HTML parser/entity subset and URL limitations (e.g. IPv6/IDN/full
  HTML5/RFC coverage) remain. Malformed markup is tolerated, not fully validated.
- No automatic robots-policy/auth decision engine. Use controlled/permitted
  sites only; no bypass functionality or public crawler production guarantee.
- Configured positive timeout applies per HTTP call, not one overall wall-clock
  crawl timer; redirect hops are not individual page attempts.
- Near-SIZE_MAX counter injection is not runtime-tested; reset counters and
  hard budgets make it unreachable through supported inputs.

**Phase 7 readiness: YES as a tested Linux baseline for a separately approved
next phase**, not permission to start or proof of production-wide safety.
Prerequisites: explicit next scope/acceptance order and mode consent if any
change were needed; decide redirect isolation/robots/site-permission policy
before broader crawling; obtain native Windows and leak-tool evidence if
those become release requirements. Future document discovery/downloads,
provenance, files, AI/UI/concurrency and deployments require their own approval.

Active Replit UI Power Mode/account tier: **NOT VERIFIED**. Existing
expertMode=true is not treated as equivalent proof. No mode setting/flag,
paid feature, tier or deployment has been changed by this work.
**NO PHASE 7 STARTED.**
