# Phase 7 acceptance and handoff — 2026-11-09

**Implementation status: COMPLETE on tested Linux; tool/platform gaps below
are explicitly NOT VERIFIED, not passing tests.** No known reproducible
remaining defect within this work order's implemented timeout/scope contracts.
No assertion failed. Full LeakSanitizer sweep is blocked by the environment,
not represented as successful. Native Windows execution is NOT VERIFIED.

Canonical repo: https://github.com/jatinvimal1506/Web-Crawler-and-Scraper-in-C.git
Authorized branch: main. Started clean, HEAD/tracking/live origin all
`0f345a07bccfb9e9d8ae642488cc2295431c96bd`. Work/test directory:
`.local/github-sync/deepdownload-c`, the independent C-only Git checkout;
reviewed files are also synchronized to student-visible `DeepDownload-C/`.
Outer web scaffold and agent-mode configuration were not part of this commit.

## Requirement acceptance, one line per A–L

| ID | Result | Evidence / qualification |
|---|---|---|
| A | PASS; UI tier NOT VERIFIED | All executable sources/tests C11, no dependencies/mode/config/paid/deploy changes, no Phase 8. Existing expertMode=true untouched, not evidence of account UI selection. |
| B | PASS | Verified exact clean baseline/live SHA above before editing; existing config/stat layouts, URL/HTML/queue/visited implementations and ownership retained. |
| C | PASS; limitations stated | Timeout and post-fetch scope flaws fixed; additional HTTP URL-OOM mapping fixed; stale current docs corrected. Unknown-alias repeat, Windows and leak-tool limitations remain honestly documented. |
| D | PASS | 1..3600 cap both APIs; 0/-1/LONG_MAX/3601 produce no requests, 1/3/3600 succeed; 17 essential option failures stop before networking; real one-second timeout tested. |
| E | PASS; local TLS integration NOT VERIFIED | Real HTTP tests prove pre-request exact-host/seed-port policy, both host modes, five redirects, codes/Location/errors/relative final base. HTTPS policy tested in-memory; TLS verification explicitly enabled/checked, not a DNS/IP firewall. |
| F | PASS | Original 25 site scenarios retained; 331 original engine failure positions plus 727 new real-transport/scoped/BFS positions; all tracked allocations released, no false OOM success; candidate/alias ceilings, partial body and error stats tested. |
| G | PASS applicable Linux; Windows/full LSan NOT VERIFIED | Strict Werror production/CLI/tests and ASan/UBSan pass. Isolated leak-enabled real-HTTP suite passes; full leak sweep exits 2 due LSan ptrace restriction. Valgrind absent. Native Windows not available, MSYS2 branches/recipes preserved. |
| H | PASS | README and api/architecture/progress/testing updated for actual Phase 7; former valid NOT_IMPLEMENTED sentence removed; historical prompt receipts unchanged. |
| I | PASS | Full accessible A–L instruction, decisions/ownership and this evidence file retained under exact prompt history folder, along with prior Markdown. |
| J | PASS | Fresh clean build, full make test, every requested specialized target, new safety/alloc targets, strict sanitizers and actual controlled CLI demo: commands/statuses below. |
| K | PASS source upload; docs receipt separately verified | Only the 22 listed C-project C/header/Makefile/Markdown files; normal non-force delivery after tests, permission preflight and remote-state check. |
| L | PASS handoff; delivery receipt follows | All limits/gaps separated, next-development readiness conditional on fresh approval; NO PHASE 8 OR LATER PHASE STARTED. |

## Changes: exact 22-file C-project inventory

| File | Purpose |
|---|---|
| Makefile | Add scoped production object and separate safety/real-allocation targets to make test; retain Windows handling. |
| README.md | Current phase, timeout/redirect behavior, new commands/history links and stop boundary. |
| documentation/api.md | Additive scoped API/status, output/timeout ownership contract. |
| documentation/architecture.md | Shared single-transfer transport and pre-request loop, unchanged containers. |
| documentation/progress.md | Current original Phase 7 vs deferred document work. |
| documentation/testing.md | Remove stale NOT_IMPLEMENTED claim; measured coverage, tool caveats, native MSYS2 commands. |
| examples/bfs_demo.c | Accurate Phase 7/pre-request policy messages, no changed arguments/defaults. |
| include/http.h | 3600 limit, additive HTTP_ERR_SCOPE and scoped function; existing signature retained. |
| include/project.h | Current timeout/pre-request comments; no changed struct fields. |
| src/http.c | Shared checked transport, finite timeout, option errors/TLS, bounded Location capture, OOM classification, arithmetic guards. |
| src/http_internal.h | Internal no-auto-follow single-transfer/owned Location contract. |
| src/http_scoped.c | URL-module-based scope check before each request, bounded manual redirect loop and cleanup. |
| src/project.c | Timeout cap and scoped transport integration, SCOPE root/child policy; unchanged BFS/container architecture. |
| tests/crawler_fixture.c | Small additional deterministic C routes for redirects/partial bodies/budgets. |
| tests/crawler_fixture.h | Reserved Phase 7 request IDs. |
| tests/crawler_stub.c | Additive API stub for existing portable API/engine allocation tests. |
| tests/test_crawler_engine.c | Tighten three redirect cases to require NO off-scope target fetch; preserve all 25 scenarios. |
| tests/test_phase7_safety.c | Bounds/options/zero-request proofs, redirect/error/partial/global-budget checks. |
| tests/test_phase7_alloc.c | All seven real modules fault-injected; TLS URL-policy-only transfer fake clearly separated. |
| prompt history/2026-11-09-phase7-authoritative-instruction.md | Complete accessible instruction body A–L and original quote. |
| prompt history/2026-11-09-phase7-decisions-and-fixes.md | Before/after findings, tradeoffs, functions/pointers/ownership, scope/timing/status limits. |
| prompt history/2026-11-09-phase7-acceptance-and-handoff.md | This requirement/test/delivery evidence. |

Outer `replit.md` received only a collaborator-facing scope/evidence pointer
update and is NOT included in GitHub delivery. No scaffold cleanup/deletion.
Source directories contain **27 .c / 10 .h**, no executable non-C source.
Git diff of URL, HTML, queue, visited implementations and their original
headers/tests, original HTTP tests/basic crawler tests and all earlier history
is empty. The intentional changes are listed above, not a rewrite of Phases 1–6.

## Fresh Linux build and regression results

Environment: Linux x86_64, GCC 16.2.0, GNU Make 4.4.1, libcurl 8.14.1.
Compiler warnings/errors: **zero** in normal and strict builds.
No Linux fixture SKIPs occurred. No tests installed anything or used browser,
Python/JS or a paid tool. Original test-http has public-network dependencies;
its successful result is separate from deterministic local evidence.

Literal normal commands (all from the C checkout):

```sh
make clean
make
timeout 180 make test
make test-crawler-engine
make test-crawler-alloc
make test-http-local
make test-http-alloc
make test-html-alloc
make test-phase7-safety
make test-phase7-alloc
make build/crawler-fixture
./build/crawler-fixture --demo
```

**Each command above exited 0**, including every separately requested target.
`make test` executes 13 C binaries, all pass (12 offline/local, one original
network-dependent HTTP suite); HTML fault injection is additionally run by
the explicit optional target.

| Suite | Actual output / result |
|---|---|
| Original API / URL | Both binaries exit 0; assertion totals not printed, not invented. URL parsing/validation/normalization/resolution output reports passed. |
| HTML | 14 groups passed, includes malformed/raw-text/entity/ownership/size/truncation cases. |
| Queue | 3667 checks; 1200 elements, FIFO/depth/ownership/boundaries. |
| Visited | 3670 checks; 1200 keys/equivalence/distinctions/ownership. |
| Phase 5 allocations | 3281 checks; 39 failure positions; zero tracked allocations. |
| Crawler real HTTP | 372 checks; all 25 original scenarios plus invalid/connection/parser boundaries; exact BFS requests/stats. Former 375 becomes 372 because THREE forbidden target requests must now be absent, not because scenarios were removed. |
| Crawler allocations | 223355 checks; 331 injected positions; queue OOM=6, visited OOM=90, post-enqueue OOM=24, INT_MAX/invariant/budgets; zero tracked allocations. |
| Local HTTP | 81 checks: redirects/final-relative base, exact MIME, binary rejection, file/FTP restrictions, exact 5MiB/+1 behavior. |
| HTTP metadata allocations | 128 checks including shared 81; original three metadata-copy OOM cases, absent optional type, safe reuse retained. |
| Phase 7 safety | 303 checks: timeout bounds/17 forced option failures, 301/302/303/307/308, query/fragment/relative redirects, five-hop success, six-hop/loop rejection, malformed/missing/duplicate/unsupported Location, blocked host/port zero target GETs, open-host same-port success, standalone compatibility, redirected 404/500/PDF, partial/timeout failures, child scope PARTIAL, cross-page/alias budgets. |
| Phase 7 real allocations | 271334 checks; 727 failure positions = redirect 49 + blocked 39 + loop 153 + real-BFS 486. OOM never success, zero outstanding tracked allocations, successful retry; in-memory HTTP->HTTPS/downgrade policy checks. |
| HTML allocations (separate target) | All 143 failure positions leave empty output and no tracked allocation. |
| Original live HTTP | 7/7 categories passed; normal HTTPS status 200, 577 bytes, HTML; no SKIPs. Categories are not claimed as individual assertion totals. |

Large allocation CHECK counts include bookkeeping assertions on frees and
list operations; they are not hundreds of thousands of distinct scenarios.

## Strict compilation and memory tools

Literal command executed, inherited CFLAGS/LDFLAGS preserve existing Nix system
include/library paths:

```sh
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 timeout 240 make \
  BUILD=build/sanitize \
  CFLAGS="$CFLAGS -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined" \
  LDFLAGS="$LDFLAGS -fsanitize=address,undefined" \
  test-unit test-phase5-alloc test-crawler-engine test-crawler-alloc \
  test-http-local test-http-alloc test-html-alloc test-phase7-safety test-phase7-alloc
```

Exit **0**, all 13 deterministic binaries passed, no ASan/UBSan diagnostics,
no warnings. Public-network HTTP excluded; HTML allocation target included.
The same CFLAGS/LDFLAGS with `make BUILD=build/sanitize ... all` exited **0**
and strictly compiled the release library and all three CLI examples too.

```sh
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 \
  ./build/sanitize/test-phase7-alloc
```

Exit **0**; 271334 checks / 727 positions pass; no leak report. This isolated
result applies only to that suite.

Repeating the complete 13-binary make command above with
`ASAN_OPTIONS=detect_leaks=1` exited **2**, stopped at first test-queue:

```text
LeakSanitizer has encountered a fatal error.
HINT: LeakSanitizer does not work under ptrace (strace, gdb, etc)
make: *** [Makefile:113: test-phase5] Error 1
```

Therefore full-process/full-suite leak freedom is **NOT VERIFIED**.
Valgrind is not installed (`command -v valgrind` found none). Application fault
accounting and ASan/UBSan success are not substitutes for that missing evidence.
No unsupported tool was installed or mislabeled a passing test.

Release archive inspection `ar t build/libcrawler.a` shows exactly:
project.o, url.o, http.o, http_scoped.o, html.o, queue.o, visited.o.
`nm` test-hook/stub/allocator-symbol search found no matches. Instrumented
objects and helpers are only separate test targets under ignored build/.

## Actual controlled CLI demo, not a fabricated transcript

`./build/crawler-fixture --demo` starts the existing C localhost server then
executes `./build/crawler-bfs URL 2 1`; actual ephemeral URL was
`http://127.0.0.1:58067/site`. Exit **0**, printed:

```text
Status: success
Pages visited: 4
Pages failed: 0
Links discovered: 8
Maximum depth attempted: 2
Bytes downloaded: 328
Limits: 100 page attempts, 1000 href candidates, 1000 scheduled/alias keys.
```

This is root->siblings->grandchild BFS, not document downloading. CLI message
correctly says redirects checked before each request. No website published.

## Remaining original-crawler validation vs future work

- Native Windows/MSYS2 runtime NOT VERIFIED: no native target/cross-compiler
  available. Existing Windows Makefile/launcher handling retained and exact
  installation/build/test commands documented in testing.md.
- POSIX local fixtures explicitly SKIP on native Windows; their successful
  Linux run is not native Windows proof.
- Full leak-enabled sweep blocked as above; local TLS certificate-negative/
  handshake fixture NOT VERIFIED (HTTPS URL-policy fake is clearly labeled).
- Unknown redirect aliases can repeat a final fetch; no universal exactly-once
  promise. Timing is per transfer (max six per logical attempt); no whole-crawl
  deadline, DNS/IP pinning/firewall or automatic robots/authorization checking.
- No remaining known in-scope functional blocker on the tested Linux build.
  **Ready for separately approved Linux document-discovery development: YES**
  with these constraints, NOT certification for native Windows release.
- Future features require explicit fresh approval: PDF/DOCX/ZIP discovery and
  saving, provenance, file hashing/dedup/reporting, UI/AI/concurrency/deployment.
  **NO PHASE 8 OR LATER PHASE STARTED.**

## Git delivery receipt

Source commit:
[`29988d7f3676b16b415e59a57ba1aaf2f7b15c3b`](https://github.com/jatinvimal1506/Web-Crawler-and-Scraper-in-C/commit/29988d7f3676b16b415e59a57ba1aaf2f7b15c3b)
on **main**, message:
`Complete Phase 7 timeout and pre-request redirect safety validation`.

Actual command sequence, all exit **0**:

```sh
git diff --cached --check
git commit -m "Complete Phase 7 timeout and pre-request redirect safety validation"
GIT_TERMINAL_PROMPT=0 git push --dry-run origin main
git ls-remote origin refs/heads/main
GIT_TERMINAL_PROMPT=0 git push origin main
git rev-parse HEAD
git rev-parse origin/main
git ls-remote origin refs/heads/main
GIT_TERMINAL_PROMPT=0 git ls-remote https://github.com/jatinvimal1506/Web-Crawler-and-Scraper-in-C.git refs/heads/main
git status --short
```

Immediately before the actual push, live main still equaled
`0f345a07bccfb9e9d8ae642488cc2295431c96bd`. Permission preflight passed;
**one normal non-force source push succeeded**, no 403/conflict, no retry.
Afterward HEAD, origin/main, live proxy main and independent canonical GitHub
main ALL equaled **29988d7f3676b16b415e59a57ba1aaf2f7b15c3b**; C tree clean.
Remote receipt:

```text
0f345a0..29988d7  main -> main
```

Configured origin remains Replit's secure GitHub proxy for the intended
repository; it was not changed. Canonical GitHub was separately queried,
not inferred from a local tracking ref.

This final source-upload receipt is a necessary **docs-only follow-up commit**
to this file, authorized by the work order. Its own hash cannot be embedded in
its contents without changing the hash. The Agent's final response independently
reports its exact SHA and verifies live GitHub equals it; it advances main
beyond the source commit without altering any tested C file. Identify that
documentation commit using:

```sh
git log -1 --format='%H %s' -- 'prompt history/2026-11-09-phase7-acceptance-and-handoff.md'
git ls-remote origin refs/heads/main
```
