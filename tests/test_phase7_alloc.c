#include "http.h"
#include "../src/http_internal.h"
#include "project.h"
#include "crawler_fixture.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static size_t attempts, fail_at, live;
static int tls_mode;
static unsigned tls_calls;
void *phase7_malloc(size_t size);
void *phase7_calloc(size_t count, size_t size);
void *phase7_realloc(void *old, size_t size);
void phase7_free(void *pointer);
HttpStatus phase7_real_once(const char *, long, HttpResponse *, char **);

#include "phase5_check.h"
void *phase7_malloc(size_t size)
{
    void *p;
    if (++attempts == fail_at) { return NULL; }
    p = malloc(size);
    CHECK(p != NULL);
    ++live;
    return p;
}
void *phase7_calloc(size_t count, size_t size)
{
    void *p;
    if (++attempts == fail_at) { return NULL; }
    p = calloc(count, size);
    CHECK(p != NULL);
    ++live;
    return p;
}
void *phase7_realloc(void *old, size_t size)
{
    int empty = old == NULL;
    void *p;
    CHECK(size != 0);
    if (++attempts == fail_at) { return NULL; }
    p = realloc(old, size);
    CHECK(p != NULL);
    if (empty) { ++live; }
    return p;
}
void phase7_free(void *p)
{
    if (p != NULL) { CHECK(live != 0); --live; }
    free(p);
}
static char *clone(const char *s)
{
    char *p = phase7_malloc(strlen(s) + 1);
    if (p != NULL) { memcpy(p, s, strlen(s) + 1); }
    return p;
}

// Only this TEST object dispatches a fake transfer for HTTP->HTTPS URL-policy
// validation. All ordinary profiles call the real checked curl transport.
// This proves scope logic, NOT a local TLS handshake/certificate validation.
HttpStatus http_download_once(const char *url, long timeout,
                             HttpResponse *response, char **location)
{
    if (!tls_mode) { return phase7_real_once(url, timeout, response, location); }
    (void)timeout;
    ++tls_calls;
    *response = (HttpResponse){0};
    *location = NULL;
    response->final_url = clone(url);
    CHECK(response->final_url != NULL);
    if (tls_calls == 1) {
        response->status_code = 302;
        *location = clone(tls_mode == 1 ? "https://EXAMPLE.TEST:8080/final#x"
                                      : "http://example.test:8080/final");
        CHECK(*location != NULL);
        return HTTP_ERR_HTTP_STATUS;
    }
    CHECK(tls_mode == 1 && strcmp(url, "https://example.test:8080/final") == 0);
    response->body = clone("<html></html>");
    response->content_type = clone("text/html");
    response->size = strlen("<html></html>");
    response->status_code = 200;
    CHECK(response->body != NULL && response->content_type != NULL);
    return HTTP_OK;
}

#ifdef _WIN32
int main(void)
{
    puts("SKIP Phase 7 real HTTP allocation fixture: POSIX only; native Windows NOT VERIFIED.");
    return 0;
}
#else
static int execute(unsigned profile, const char *url, const char *seed)
{
    if (profile == 3) {
        CrawlerConfig config = {url, 2, 1, 2};
        CrawlerStats stats;
        CrawlerStatus status = crawler_run(&config, &stats);
        CHECK(stats.pages_visited + stats.pages_failed <= CRAWLER_MAX_PAGES);
        CHECK(stats.links_discovered <= CRAWLER_MAX_CANDIDATES);
        return (int)status;
    } else {
        HttpResponse response = {0};
        HttpStatus status = http_download_scoped(url, 2, seed, 1, &response);
        if (status == HTTP_ERR_OUT_OF_MEMORY) {
            CHECK(response.body == NULL && response.size == 0 && response.status_code == 0);
            CHECK(response.final_url == NULL && response.content_type == NULL);
        }
        http_response_free(&response);
        http_response_free(&response);
        return (int)status;
    }
}

int main(void)
{
    static const char *paths[] = {"redirect", "off-redirect", "safety/7", "alias-site"};
    static const int expected[] = {HTTP_OK, HTTP_ERR_SCOPE, HTTP_ERR_TOO_MANY_REDIRECTS, CRAWLER_OK};
    size_t points[4], i, p, total = 0;
    unsigned records[16], port = crawler_fixture_start();
    char seed[128], url[160];
    HttpResponse response = {0};
    (void)snprintf(seed, sizeof(seed), "http://127.0.0.1:%u/", port);
    for (p = 0; p < 4; ++p) {
        (void)snprintf(url, sizeof(url), "%s%s", seed, paths[p]);
        attempts = 0;
        CHECK(execute((unsigned)p, url, seed) == expected[p]);
        points[p] = attempts;
        total += attempts;
        CHECK(live == 0);
        (void)crawler_fixture_requests(records, 16);
        for (i = 1; i <= points[p]; ++i) {
            attempts = 0;
            fail_at = i;
            CHECK(execute((unsigned)p, url, seed) ==
                  (p == 3 ? (int)CRAWLER_OUT_OF_MEMORY : (int)HTTP_ERR_OUT_OF_MEMORY));
            CHECK(live == 0);
            (void)crawler_fixture_requests(records, 16);
        }
        fail_at = 0;
        CHECK(execute((unsigned)p, url, seed) == expected[p] && live == 0);
        (void)crawler_fixture_requests(records, 16);
    }
    tls_mode = 1;
    tls_calls = 0;
    CHECK(http_download_scoped("http://example.test:8080/start", 1,
          "http://example.test:8080/start", 1, &response) == HTTP_OK);
    CHECK(tls_calls == 2);
    http_response_free(&response);
    CHECK(live == 0);
    tls_mode = 2;
    tls_calls = 0;
    CHECK(http_download_scoped("https://example.test:8080/start", 1,
          "https://example.test:8080/start", 1, &response) == HTTP_ERR_SCOPE);
    CHECK(tls_calls == 1);
    http_response_free(&response);
    CHECK(live == 0);
    crawler_fixture_stop();
    printf("PASS Phase 7 allocations: %zu checks; %zu failure positions "
           "(redirect=%zu blocked=%zu loop=%zu real-BFS=%zu); "
           "zero tracked allocations; HTTP->HTTPS policy/downgrade checked in-memory\n",
           checks, total, points[0], points[1], points[2], points[3]);
    return 0;
}
#endif
