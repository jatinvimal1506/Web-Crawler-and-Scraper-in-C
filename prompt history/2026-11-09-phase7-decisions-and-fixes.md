# Phase 7 decisions, fixes and ownership

Authorized scope: final hardening/validation of the original seven-phase C11
crawler, NOT a DeepDownload file/document feature phase. Baseline was clean
main, local/tracking/live GitHub all
`0f345a07bccfb9e9d8ae642488cc2295431c96bd`. Earlier instruction/history files
are retained unchanged. No mode, expertMode flag, paid service, dependency,
compute tier or deployment was changed. Active account UI mode is NOT VERIFIED.

## Findings and before/after evidence

| Finding | Before | Phase 7 |
|---|---|---|
| Unsupported positive timeout | libcurl 8.14.1 rejected LONG_MAX setters with code 43, yet real HTTP/crawler returned OK on local /ok. | Both APIs reject outside 1..3600 before networking; request counters prove zero calls for 0/-1/LONG_MAX/3601. 1/3/3600 pass. |
| Ignored setup errors | Most curl option results ignored, including deadlines. | Every used option checked; any error aborts before perform. 17 forced option failures produce zero server requests; forced option OOM propagates OOM. |
| Redirect escapes seed host/port before rejection | Phase 6 fixture observed blocked target GETs before final scope check. | Scoped downloader disables automatic following and validates each next URL BEFORE transfer; blocked targets have zero GETs. Standalone automatic-follow API retained. |
| HTTP URL parse OOM misclassified | Source inspection showed any url_parse error mapped to BAD_URL, including allocation failure. | Explicit OUT_OF_MEMORY mapping; real combined transport/URL/BFS fault sweeps cover it. This is an additional inherited defect, not a fabricated prior runtime observation. |
| Stale testing.md | Old valid NOT_IMPLEMENTED sentence contradicted current C test; current-result pointer/LSan wording overbroad. | Corrected current guide; historical receipts left unchanged; individual tool results recorded separately. |

Global curl initialization preserves its CURLcode; known initialization OOM
and NULL easy-handle allocation now map to OUT_OF_MEMORY rather than losing
that distinction. libcurl-internal allocators are not instrumented by the
application allocation tests, so no claim of exhaustive libcurl OOM injection.
Body callback arithmetic now checks multiplication/addition before use.

## Chosen narrow transport design

Original `http_download(url, timeout, response)` stays public and source
compatible. It still permits HTTP(S) automatic redirects for standalone users,
up to five transitions. No config/stat fields or queue/visited APIs changed.
Timeouts are deliberately tightened to **1..3600 seconds** (constant
HTTP_MAX_TIMEOUT_SECONDS); normal previous tests include the supported 3600.

Both entry points reuse `download_request` in http.c. It checks every option,
explicitly sets TLS peer verification=1 and host verification=2, restricts
protocols and redirect protocols to HTTP(S), installs the 5MiB cap and timeouts.
Options rejected by curl return HTTP_ERR_INIT (OOM stays OUT_OF_MEMORY).
There is no fallback to unsafe/unbounded settings.

New `http_download_scoped(url, timeout, seed_url, same_domain, response)` calls
the internal single-transfer `http_download_once`, with FOLLOWLOCATION=0:

1. Validate borrowed config/URLs; parse seed and normalize requested URL.
2. Parse target, compare exact normalized host if strict, numeric effective
   seed port in BOTH modes, and reject HTTPS->HTTP downgrade.
3. Only THEN perform one GET.
4. For 301/302/303/307/308, resolve a usable Location against the actual response
   final_url through existing url_resolve. Repeat validation BEFORE next GET.
5. Maximum five transitions/six transfers. Missing/invalid/unsupported Location
   is HTTP_ERR_HTTP_STATUS; loop/extra hop HTTP_ERR_TOO_MANY_REDIRECTS.

`LocationHeader` is a fixed 2049-byte stack buffer (2048 URL bytes + NUL).
Header names are case-insensitive, values trimmed; duplicate/empty/oversized/
embedded-NUL values fail closed. Interim/proxy status lines reset header state.
It is not a new URL parser: all targets still use the existing URL module.
Same-port HTTP->HTTPS is allowed and keeps TLS checks; downgrade is a deliberate
additional safety restriction only in the additive scoped API.

Rejected alternatives: naive hostname prefix comparisons (lookalikes, ports,
userinfo), parsing a Location after curl already followed it, attempting to
control redirect permission from an incomplete callback, or rewriting the
queue/visited/crawler architecture. The fixed header callback only captures
Location; the understandable C loop makes the request authorization decision.

## Exact remaining network limitations

This closes URL-level redirect host/port leakage, NOT all network policy:
DNS may resolve an allowed hostname to private/loopback addresses or rebind it.
System proxies, DNS/IP policy, robots restrictions and authorization remain
the caller/operator's responsibility. No login/CAPTCHA/block bypass exists.
Only permissioned/controlled sites should be used.

Unknown redirect aliases may still lead to a repeated final fetch. Learned
final aliases skip queued direct targets; both orders remain tested. There is
no universal exactly-once HTTP promise, retry/resume or concurrent crawling.

## Timing, budgets, stats and status contract

Timeout is per individual transfer in the new manual scoped loop: at most six
transfers can occur per logical page. Standalone automatic following retains
curl's total timeout for that call. There is no whole-crawl wall-clock timer.
CLI uses 3 seconds. A request/body that stalls is actually tested with timeout 1.

Fixed limits stay 100 attempted downloader calls, 1000 complete emitted href
entries and 1000 scheduled/known-alias keys. The root counts as one key.
INT_MAX child-depth guard stays; supported max_depth may be SIZE_MAX.
One logical attempt can now issue up to six actual GETs. 5MiB is per transfer;
wire traffic/redirect bodies are not measured by bytes_downloaded.

Stats retain their definitions:
- pages_visited: HTTP_OK, in-scope, completely extracted attempts.
- pages_failed: downloader/setup/scope/parsing failure attempts.
- links_discovered: entries from completed extraction, duplicates/off-scope
  entries included; rolled-back extraction prefixes excluded.
- max_depth_reached: maximum attempted depth, not merely queued depth.
- bytes_downloaded: body sizes delivered as HTTP_OK, including later parse
  failures, NOT discarded HTTP/redirect/error bodies or total wire traffic.

A blocked target contributes no bytes because it is not fetched. Scope error
HTTP_ERR_SCOPE (appended enum, earlier values preserved) maps to root
CRAWLER_PARSE_ERROR; child failure is counted and finishes CRAWLER_PARTIAL.
Other HTTP status/transfer failures map as before. Invalid config/timeouts
zero stats and return INVALID_ARGUMENT. OOM/invariant/budget errors abort,
never silently become duplicates or successful completion.
LIMIT_REACHED is incomplete. OK means the permitted depth-bounded queue drained
without page failures, not exhaustive Internet/website/document discovery.

## Pointers and cleanup — student function guide

A borrowed pointer is an address used temporarily, not freed by the receiver.
An owned pointer refers to heap memory that its owner must free. `char **out`
allows the function to store a newly allocated string address in the caller.
Stack arrays live only for that call and are never returned.

| Revised/added function/helper | Purpose, arguments and ownership |
|---|---|
| `http_write_callback` | Curl supplies borrowed chunk/buffer pointers. Check lengths; realloc a temporary, preserve old buffer on failure; body belongs to request until success. |
| `location_header` / `LocationHeader` | Borrow curl header bytes, copy into bounded stack storage, reject ambiguity; no callback heap ownership. |
| `SET_OPTION` | Typed macro checks each actual curl option before perform, cleans handle on failure. Test-only hook is removed by preprocessing in release. |
| `ensure_curl_initialized` | Return real CURLcode, keep existing global lifetime; caller can http_cleanup at exit. |
| `download_request` | Shared checked transfer; owns curl handle and body while receiving. Copy metadata, transfer success body to response; free partial buffers on errors. |
| `http_download` | Original borrowed inputs/output object, unchanged signature; calls shared transport with automatic following. |
| `http_download_once` | Internal helper, no automatic following. Returns owned Location copy through char**; scoped caller always frees it. Not a new public include API. |
| `scoped_port` | Read validated borrowed ParsedUrl, return explicit/default numeric port, no allocation. |
| `redirect_status` | Pure check of the five supported status codes. |
| `http_download_scoped` | Own seed/target parts, current/next keys, Location between hops. One cleanup block frees them; on OOM frees response too. On success caller owns HttpResponse. |
| HTTP status helpers | Add SCOPE string/mapping without renumbering old statuses. Static strings are borrowed, never freed by caller. |
| `crawler_run` | Validate timeout cap before any work; caller retains config/start_url/stats storage, no allocated pointer escapes. |
| `process_page` | Calls scoped downloader with borrowed seed/domain config; existing response/links/final-key cleanup unchanged. |
| `in_scope` | Retained final defense-in-depth check; no longer the only redirect guard. |
| CLI `main` | Same arguments/defaults, now accurately describes Phase 7/pre-request checking. |

Queue enqueues borrowed URL by cloning, dequeue transfers item string, engine
frees item; visited owns its canonical copies. Enqueue-before-mark remains.
Mark failure after enqueue aborts/free both containers; no continued untracked
work. New scope OOM clears all owned response fields; other errors may retain
metadata from the last ACTUALLY requested hop. Never label the blocked target
as a fetched final_url. Always free responses before reuse/repeated cleanup.

## Tests and practical limits

`test_phase7_safety`: C localhost fixture and forced setup results. The latter
does not variadically forward curl options (avoids undefined test code); only
test objects compile HTTP_OPTION_TEST. Zero-request counters, all redirect
codes, exact/excessive hops, malformed/unsupported Location, partial/error
responses, scope modes and global candidate/alias-key budgets are checked.

`test_phase7_alloc`: instrument all seven application modules, not libcurl
internals. Fail every allocation in real redirect success, blocked target,
loop and real redirecting BFS profiles; verify OOM, response ownership,
queue/set cleanup and successful retry. Its HTTPS transfer fake proves URL
upgrade/downgrade policy ONLY, not a TLS handshake/certificate fixture.

Original suites retained; only three old Phase 6 redirect expectations are
intentionally tightened from "target fetched then rejected" to "no target GET".
The check total drops by three because those unwanted request records vanish,
not because a scenario was deleted. All 25 scenarios remain.
Test headers/stubs/hooks stay out of the seven-object release archive.

Native Windows/MSYS2 and deterministic local TLS are NOT VERIFIED here.
POSIX socket tests explicitly SKIP on Windows; no untested Windows fixture
rewrite is introduced. Existing portable tests remain and Windows setup is
documented. New CHECK tests stay active under NDEBUG; legacy assert-based tests
must be built without -DNDEBUG for meaningful coverage.
Sanitizer/leak outcomes and exact commands are in the acceptance handoff;
allocation accounting is not a whole-process leak certificate.

## Stop boundary

This finishes the original crawler's Phase 7 scope on the tested platform.
Windows/reliable external leak verification remain release-validation items
if required. Document discovery, PDF/DOCX/ZIP saving, provenance, SHA-256/file
dedup, CSV/reporting, UI/AI/concurrency/deployment need a fresh work order.
NO PHASE 8 OR LATER PHASE STARTED.
