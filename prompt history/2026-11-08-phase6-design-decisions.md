# Phase 6 design decisions and central contract

This records the choices for the separately approved Phase 6 order. The
requested filename/date label is preserved; no earlier historical prompt is
rewritten. Baseline verified before edits: local and live GitHub `main` both
`e5a99d03353abf79f9d0b3fe4d551d777d2bbae5`, clean C-only checkout.
Execution clock observed 2026-11-07 UTC; the 2026-11-08 filenames are the
user-requested labels, not a claim of a later execution date.

## Scope, dependencies and compatibility

Only C11 single-threaded BFS integration, C acceptance fixtures, CLI and
Markdown/build changes. No Phase 7, document downloader, binary save, AI,
browser/UI, concurrency or new dependency. The existing C/libcurl toolchain
is sufficient. No mode/settings change is needed. Existing expertMode=true
is not proof of the active account UI tier; that tier cannot be verified here.

CrawlerConfig keeps exactly its original four fields and CrawlerStats its
five size_t fields. No uninitialized config fields are added. Old enum numeric
values/signatures remain; PARTIAL, LIMIT_REACHED, INTERNAL_ERROR are appended.
NOT_IMPLEMENTED is retained as a legacy status/string, not valid-run behavior.
Queue, visited, URL and HTTP public interfaces/implementations are unchanged.

## 1. Real FIFO BFS and ownership

`crawler_run` validates and normalizes the seed, parses its canonical host/
effective port, and creates zeroed queue/scheduled/attempted containers.
It schedules the root at depth 0. On a new canonical key:

1. `visited_contains` must return NOT_FOUND (PRESENT skips; errors abort).
2. `queue_enqueue` copies URL/depth. If it fails, do not mark visited.
3. `visited_insert` must return ADDED. Any failure after enqueue is fatal.
4. The caller immediately stops and frees the whole queue and both sets;
   there is no pretend rollback/resume or continued untracked queued work.

The oldest item is dequeued. Its string transfers to the engine and is freed
with queue_item_free after processing (and again safely in final cleanup).
Children go to the tail with parent+1. No URL deeper than max_depth is
scheduled/fetched; max_depth=0 still fetches and extracts the root but follows
no links. Before addition, depth is checked against INT_MAX. A synthetic test
forces INT_MAX with max_depth=SIZE_MAX; if links would require deeper work,
the result is LIMIT_REACHED, never signed overflow.

Every page has local HttpResponse, HtmlLinks and canonical-final ownership.
One page cleanup frees all three on every path. Final engine cleanup frees
the current item, pending queue, scheduled/attempted sets, parsed seed and
normalized seed string. Caller start_url/config/stats are borrowed, unchanged
except for the documented stats writes. No owned pointers escape the call.
The caller may call http_cleanup once at program exit; crawler_run does not
tear down libcurl globally between pages/crawls.

Rejected: DFS/recursion, container API redesign, a third-party/hash-table
framework and a resumable cross-container transaction. Existing teaching
containers are sufficient within the fixed budgets.

## 2. Scheduled, attempted, aliases and failures

Scheduled means accepted as future work, NOT successfully downloaded.
Requested keys are marked attempted before download. Known in-scope canonical
final URLs are added to scheduled/attempted even when HTTP status, MIME or
later parsing fails. Therefore queued direct URLs already learned from an
earlier redirect are skipped without another HTTP call or retry.

An unknown alias can still redirect to a previously fetched target. With the
unchanged automatic-redirect downloader, that target may already be fetched
again before the engine learns the alias. Tests explicitly cover both orders:
alias before queued direct target avoids the direct fetch; direct target
before unknown alias necessarily repeats the final redirect fetch.
This is not hidden as an exact-once guarantee. That second successful HTML
response is parsed/counted again, and duplicate children still are not queued.

There are NO automatic retries. A failed canonical page remains attempted.
Duplicate hrefs, fragments, hostname/scheme case and default ports follow
existing url_normalize rules; no new slash/percent/query-order equivalences
are invented. URL lengths retain the existing 2048-byte validation.

Rejected: guessing redirect destinations, silently treating visited errors
as duplicates/missing, or calling already scheduled pages successfully fetched.

## 3. Scope, ports and redirects — deliberately limited guarantee

`same_domain=1` requires exact canonical hostname equality with the seed,
not subdomains, lookalikes, DNS aliases or merely the same IP.

**Both modes pin the effective numeric seed port.** A missing port means 80
for HTTP or 443 for HTTPS; a relative URL keeps its resolved port. Mode 0
permits different HTTP(S) hostnames on that same port, not arbitrary services.
This is stricter than unrestricted cross-origin crawling. For example, an
HTTP:80 seed does not automatically permit an HTTPS:443 upgrade; explicit
HTTPS:80 has the same port but still uses normal TLS verification. No default
port is substituted for a nondefault loopback service.

Scope is checked before scheduling children and against the effective final
URL BEFORE parsing its body. Off-scope pages/links are not parsed/scheduled.
But HTTP follows redirects internally: **checking final_url after download
does NOT prevent an off-host/off-port network fetch**. Local tests prove that
such a redirect target is fetched, then rejected, with no child-link leakage.
No network-level origin isolation, DNS-rebinding protection or SSRF firewall
is claimed. Existing HTTP(S)-only protocols, five-redirect limit, TLS defaults,
User-Agent and request timeout remain unchanged.

Rejected here: rewriting the transport into a manual redirect engine or a
header callback whose Location interpretation could diverge from libcurl.
Robust pre-connect origin enforcement would need separate transport design/
approval and tests; the work order explicitly permits honest post-checking.

No robots parser/authentication/CAPTCHA/block bypass is implemented or claimed.
Only controlled/permissioned local sites are used for the new demonstration.
Callers must have permission and obey site restrictions; the engine is not an
automatic authorization or robots-policy decision system.

## 4. Bounded memory/work

Constants in project.h, not new config fields:

| Budget | Meaning |
|---|---|
| CRAWLER_MAX_PAGES = 100 | HTTP download API invocations; redirect hops are not separate attempts. |
| CRAWLER_MAX_CANDIDATES = 1000 | Total href entries from completed page extractions, including duplicates/off-scope entries. |
| CRAWLER_MAX_SCHEDULED = 1000 | Scheduled canonical/known-alias keys, INCLUDING root, so at most 999 other keys if no aliases consume slots. |

The original HTML API is preserved. The additive
`html_extract_links_limited` shares the SAME scanner/decoder/resolver, with
a maximum retained array length. On the next valid href beyond that bound,
it frees partial output and returns HTML_ERROR_LIMIT. Its array growth is
clamped to the maximum. An extra temporary candidate may be resolved before
being rejected; no unbounded candidate array is retained.

Rejected: first extracting an unlimited array and only then truncating it.
That would violate the intended memory bound on a 5 MiB page containing many
short anchors. The old unlimited API still calls the shared parser with
SIZE_MAX and retains its tested behavior/allocation sequences.

A truncated extraction is NOT a completed parse: that attempted page counts
failed, its prefix links are not emitted/countable, and the crawl returns
LIMIT_REACHED. Already completed earlier-page stats stay intact. Exactly
1000 duplicate hrefs plus a leaf fits; 1001 hrefs does not. A 1000-unique-child
root can hit the scheduled-key limit before any children are fetched.

At the page limit the next unknown queued item is freed without fetching;
known aliases can be skipped without consuming attempts. Remaining containers
are always cleaned up. Existing HTTP cap is 5 MiB per call; configured timeout
and five redirects bound each call. One attempt can involve up to six HTTP
requests (initial + five redirects). No parallel requests or busy crawl loop.
All stats additions check SIZE_MAX before increment/add; requested max_depth
may be SIZE_MAX but integer child-depth addition is still guarded.

## 5. Central stats and return contract

| Field | Exact meaning |
|---|---|
| pages_visited | HTTP_OK, in-scope, fully extracted HTML attempts. Not necessarily unique final URLs if an unknown alias redirects to an old target. |
| pages_failed | Attempted pages rejected by download, final-URL/scope/C-string validation, or incomplete/failed extraction. |
| links_discovered | Entries emitted by completed extraction, including duplicate/off-scope occurrences, even at max depth. Not raw invalid/unsafe href spellings or a rolled-back extraction prefix. |
| max_depth_reached | Maximum depth whose HTTP call was attempted; enqueued-but-unfetched depths do not count. Root=0. |
| bytes_downloaded | Sum of HttpResponse.size for HTTP_OK responses, even if later scope/parsing fails. Error bodies discarded by HTTP are not counted. Not raw wire bytes or redirect headers/body accounting. |

Stats are zeroed whenever stats is non-NULL, even on invalid config/start URL.
A scheduling OOM after successful parent extraction does not turn that parent
into a failed page. OOM before any HTTP call does not fabricate a failed
attempt. Attempts equal pages_visited + pages_failed for normal completed
crawls; skips of known redirect aliases are not attempts.

| Situation | Return |
|---|---|
| Invalid config/start URL | INVALID_ARGUMENT, zero stats. |
| Any allocation failure | OUT_OF_MEMORY, abort/cleanup, coherent partial stats. |
| Unexpected queue/set contract failure | INTERNAL_ERROR, abort/cleanup. |
| Page/candidate/key/int-depth budget | LIMIT_REACHED, incomplete, never exhaustive OK. |
| Root HTTP non-HTML | PARSE_ERROR. |
| Root HTTP DNS/connect/timeout/TLS/redirect-limit/HTTP status/too-large/other transfer failure | NETWORK_ERROR unless effective-URL validation/scope rejects first. |
| HTTP invalid argument/bad URL | INVALID_ARGUMENT (normally excluded by engine validation). |
| Invalid/off-scope effective URL, embedded-NUL body, invalid base/extraction error | Root PARSE_ERROR; non-root failure counted and continued. |
| Non-root recoverable errors | Continue with no retry, ultimately PARTIAL rather than success. |
| Queue drained, no failed pages | OK for the requested in-scope/depth-bounded crawl, not whole-site completeness. |

HTTP OOM takes priority immediately. Otherwise, known final metadata is
validated/scoped/remembered even on failed transfers before ordinary HTTP
error mapping; an invalid/off-scope final URL can therefore yield PARSE_ERROR.
Malformed hrefs/tags follow the existing tolerant scanner: skip/consume safely,
not full HTML5 validation. Embedded NUL is rejected because the parser is a
C-string API. Child recoverable failures continue; critical OOM/invariant/
budget statuses never become OK/PARTIAL.

## 6. Test and demo choices

The basic API's valid-run expectation now uses an empty-HTML C transport stub,
so invalid/stat/status unit tests remain offline and portable. A real C POSIX
fixture separately verifies actual libcurl requests, FIFO order, all counters,
scope/depth/redirect aliases, failures, candidate/frontier/page budgets and
controlled cross-host/port behavior. It logs compact request records through
a pipe, including whether the Host was localhost; no browser or public site.

Separate instrumented objects replace malloc/realloc/calloc/free and observe
enqueue/insert/dequeue ONLY in the allocation executable. They fail each
allocation position in a complete representative crawl, plus force INT_MAX,
negative depth and dequeue contract failures. Large budget cleanup is checked.
These hooks/stubs are not archived into libcrawler.a.

The conservative CLI requires a URL, defaults depth 1/same-domain 1/timeout 3,
accepts CLI depths 0..10 and optional same-domain 0|1, prints real status/stats
and returns nonzero for partial/error/limit. Original demos remain, with stale
NOT_IMPLEMENTED explanations corrected. A C fixture launcher starts the site
and actually executes the CLI. Windows recipe support remains; native Windows
and LeakSanitizer cannot be claimed verified without a real applicable run.

## Phase 7 boundary

NO PHASE 7 STARTED. Further validation/hardening or DeepDownload features need
a new explicit work order, permitted UI-mode confirmation, scope/policy
decisions and acceptance tests. No inferred permission for downloads, new
dependencies, upgrades, deployment, AI or higher-cost agent modes.
