#include "project.h"
#include "html.h"
#include "crawler_fixture.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#ifdef _WIN32
int main(void)
{
    puts("SKIP: crawler HTTP integration fixture requires POSIX fork/sockets; native Windows NOT VERIFIED.");
    return 0;
}
#else
#include "phase5_check.h"
static unsigned port;
static size_t scenarios;

static void order(const unsigned *expected, size_t count)
{
    unsigned actual[512];
    size_t got = crawler_fixture_requests(actual, 512), i;
    CHECK(got == count);
    for (i = 0; i < count; ++i) {
        CHECK(actual[i] == expected[i]);
    }
}

static void run(const char *path, size_t depth, int same_domain,
                CrawlerStatus expected, CrawlerStats wanted)
{
    char url[256], before[256];
    CrawlerConfig config;
    CrawlerStats stats = {99, 99, 99, 99, 99};
    (void)snprintf(url, sizeof(url), "http://127.0.0.1:%u%s", port, path);
    memcpy(before, url, strlen(url) + 1);
    config = (CrawlerConfig){url, depth, same_domain, 1};
    CHECK(crawler_run(&config, &stats) == expected);
    CHECK(strcmp(url, before) == 0);
    CHECK(stats.pages_visited == wanted.pages_visited);
    CHECK(stats.pages_failed == wanted.pages_failed);
    CHECK(stats.links_discovered == wanted.links_discovered);
    CHECK(stats.max_depth_reached == wanted.max_depth_reached);
    CHECK(stats.bytes_downloaded == wanted.bytes_downloaded);
    ++scenarios;
}

int main(void)
{
    size_t root, a, b, g, new_page, new_child, off, i, bytes;
    unsigned sequence[100];
    port = crawler_fixture_start();
    root = crawler_fixture_body_size(FIX_SITE);
    a = crawler_fixture_body_size(FIX_A);
    b = crawler_fixture_body_size(FIX_B);
    g = crawler_fixture_body_size(FIX_G);
    new_page = crawler_fixture_body_size(FIX_NEW_PAGE);
    new_child = crawler_fixture_body_size(FIX_NEW_CHILD);
    off = crawler_fixture_body_size(FIX_OFF_LANDING);
    run("/site", 0, 1, CRAWLER_OK, (CrawlerStats){1, 0, 4, 0, root});
    order((unsigned[]){FIX_SITE}, 1);
    run("/site", 1, 1, CRAWLER_OK, (CrawlerStats){3, 0, 7, 1, root + a + b});
    order((unsigned[]){FIX_SITE, FIX_A, FIX_B}, 3);
    run("/site", 2, 1, CRAWLER_OK, (CrawlerStats){4, 0, 8, 2, root + a + b + g});
    order((unsigned[]){FIX_SITE, FIX_A, FIX_B, FIX_G}, 4);
    run("/site", SIZE_MAX, 1, CRAWLER_OK, (CrawlerStats){4, 0, 8, 2, root + a + b + g});
    order((unsigned[]){FIX_SITE, FIX_A, FIX_B, FIX_G}, 4);
    run("/empty", 2, 1, CRAWLER_OK, (CrawlerStats){1, 0, 0, 0, 0});
    order((unsigned[]){FIX_EMPTY}, 1);
    run("/errors", 1, 1, CRAWLER_PARTIAL,
        (CrawlerStats){2, 5, 7, 1, crawler_fixture_body_size(FIX_ERRORS) + 4 + 2});
    order((unsigned[]){FIX_ERRORS, FIX_MISSING, FIX_BOOM, FIX_PDF, FIX_NUL, FIX_HANG, FIX_OK}, 7);
    run("/missing", 0, 1, CRAWLER_NETWORK_ERROR, (CrawlerStats){0, 1, 0, 0, 0});
    order((unsigned[]){FIX_MISSING}, 1);
    run("/pdf", 0, 1, CRAWLER_PARSE_ERROR, (CrawlerStats){0, 1, 0, 0, 0});
    order((unsigned[]){FIX_PDF}, 1);
    run("/nul", 0, 1, CRAWLER_PARSE_ERROR, (CrawlerStats){0, 1, 0, 0, 4});
    order((unsigned[]){FIX_NUL}, 1);
    run("/drop", 0, 1, CRAWLER_NETWORK_ERROR, (CrawlerStats){0, 1, 0, 0, 0});
    order((unsigned[]){FIX_DROP}, 1);
    run("/redirect", 1, 1, CRAWLER_OK, (CrawlerStats){2, 0, 1, 1, new_page + new_child});
    order((unsigned[]){FIX_REDIRECT, FIX_NEW_PAGE, FIX_NEW_CHILD}, 3);
    run("/alias-site", 2, 1, CRAWLER_OK,
        (CrawlerStats){3, 0, 4, 2, crawler_fixture_body_size(FIX_ALIAS) + new_page + new_child});
    order((unsigned[]){FIX_ALIAS, FIX_REDIRECT, FIX_NEW_PAGE, FIX_NEW_CHILD}, 4);
    /* Unknown alias AFTER direct fetch necessarily repeats final HTTP fetch.
     * State this limitation honestly rather than pretending it was prevented. */
    run("/alias-reverse", 2, 1, CRAWLER_OK,
        (CrawlerStats){4, 0, 4, 2, crawler_fixture_body_size(FIX_ALIAS_REVERSE) + 2 * new_page + new_child});
    order((unsigned[]){FIX_ALIAS_REVERSE, FIX_NEW_PAGE, FIX_REDIRECT, FIX_NEW_PAGE, FIX_NEW_CHILD}, 5);
    run("/failed-alias", 2, 1, CRAWLER_PARTIAL,
        (CrawlerStats){1, 1, 2, 1, crawler_fixture_body_size(FIX_FAILED_ALIAS)});
    order((unsigned[]){FIX_FAILED_ALIAS, FIX_FAILED_REDIRECT, FIX_MISSING}, 3);
    run("/nul-alias", 2, 1, CRAWLER_PARTIAL,
        (CrawlerStats){1, 1, 2, 1, crawler_fixture_body_size(FIX_NUL_ALIAS) + 4});
    order((unsigned[]){FIX_NUL_ALIAS, FIX_NUL_REDIRECT, FIX_NUL}, 3);
    run("/off-redirect", 1, 1, CRAWLER_PARSE_ERROR, (CrawlerStats){0, 1, 0, 0, 0});
    order((unsigned[]){FIX_OFF_REDIRECT}, 1); /* Blocked destination NEVER requested. */
    run("/off-redirect", 1, 0, CRAWLER_OK, (CrawlerStats){2, 0, 1, 1, off + 5});
    order((unsigned[]){FIX_OFF_REDIRECT, FIX_OFF_LANDING | FIX_LOCALHOST_BIT,
                      FIX_OFF_CHILD | FIX_LOCALHOST_BIT}, 3);
    run("/port-redirect", 1, 1, CRAWLER_PARSE_ERROR,
        (CrawlerStats){0, 1, 0, 0, 0});
    order((unsigned[]){FIX_PORT_REDIRECT}, 1);
    run("/port-redirect", 1, 0, CRAWLER_PARSE_ERROR,
        (CrawlerStats){0, 1, 0, 0, 0});
    order((unsigned[]){FIX_PORT_REDIRECT}, 1);
    run("/scope-strict", 1, 1, CRAWLER_OK,
        (CrawlerStats){2, 0, 6, 1, crawler_fixture_body_size(FIX_SCOPE_STRICT) + 2});
    order((unsigned[]){FIX_SCOPE_STRICT, FIX_OK}, 2);
    run("/scope-open", 1, 0, CRAWLER_OK,
        (CrawlerStats){3, 0, 4, 1, crawler_fixture_body_size(FIX_SCOPE_OPEN) + 2 + off});
    order((unsigned[]){FIX_SCOPE_OPEN, FIX_OK, FIX_OFF_LANDING | FIX_LOCALHOST_BIT}, 3);
    run("/candidate-limit", 1, 1, CRAWLER_LIMIT_REACHED,
        (CrawlerStats){0, 1, 0, 0, crawler_fixture_body_size(FIX_CANDIDATE_LIMIT)});
    order((unsigned[]){FIX_CANDIDATE_LIMIT}, 1);
    run("/candidate-exact", 1, 1, CRAWLER_OK,
        (CrawlerStats){2, 0, 1000, 1, crawler_fixture_body_size(FIX_CANDIDATE_EXACT) + 2});
    order((unsigned[]){FIX_CANDIDATE_EXACT, FIX_OK}, 2);
    run("/queue-limit", 1, 1, CRAWLER_LIMIT_REACHED,
        (CrawlerStats){1, 0, 1000, 0, crawler_fixture_body_size(FIX_QUEUE_LIMIT)});
    order((unsigned[]){FIX_QUEUE_LIMIT}, 1);
    bytes = 0;
    for (i = 0; i < 100; ++i) {
        sequence[i] = FIX_CHAIN_BASE + (unsigned)i;
        bytes += crawler_fixture_body_size(sequence[i]);
    }
    run("/p/0", SIZE_MAX, 1, CRAWLER_LIMIT_REACHED, (CrawlerStats){100, 0, 100, 99, bytes});
    order(sequence, 100);
    {
        CrawlerStats stats;
        char url[128];
        CrawlerConfig config = {url, 0, 1, 1};
        (void)snprintf(url, sizeof(url), "http://127.0.0.1:%u/no-listener",
                       crawler_fixture_refused_port());
        CHECK(crawler_run(&config, &stats) == CRAWLER_NETWORK_ERROR);
        CHECK(stats.pages_failed == 1 && stats.pages_visited == 0 && stats.bytes_downloaded == 0);
        order(NULL, 0);
    }
    {
        const char *invalid[] = {"relative", "ftp://localhost/file", "https://", "javascript:x"};
        CrawlerStats stats;
        CrawlerConfig config = {NULL, 0, 1, 1};
        for (i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
            config.start_url = invalid[i];
            stats = (CrawlerStats){99, 99, 99, 99, 99};
            CHECK(crawler_run(&config, &stats) == CRAWLER_INVALID_ARGUMENT);
            CHECK(stats.pages_visited == 0 && stats.pages_failed == 0 &&
                  stats.links_discovered == 0 && stats.max_depth_reached == 0 &&
                  stats.bytes_downloaded == 0);
        }
        order(NULL, 0);
    }
    {
        HtmlLinks links = {0};
        CHECK(html_extract_links_limited("<a href='/a'><a href='/b'>",
              "http://localhost/", 1, &links) == HTML_ERROR_LIMIT);
        CHECK(links.urls == NULL && links.count == 0);
        CHECK(html_extract_links_limited("", "http://localhost/", 0, &links) == HTML_OK);
        html_links_free(&links);
    }
    crawler_fixture_stop();
    printf("PASS crawler HTTP engine: %zu checks; %zu site scenarios plus invalid/connection/parser boundaries; exact BFS/redirect request order and stats\n",
           checks, scenarios);
    return 0;
}
#endif
