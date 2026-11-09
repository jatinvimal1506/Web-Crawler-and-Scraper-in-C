#include "http.h"
#include "project.h"
#include "crawler_fixture.h"
#include <curl/curl.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

static CURLoption failed_option;
static CURLcode failed_code = CURLE_BAD_FUNCTION_ARGUMENT;
CURLcode http_test_option_result(CURLoption option);
CURLcode http_test_option_result(CURLoption option)
{
    return option == failed_option ? failed_code : CURLE_OK;
}

#ifdef _WIN32
int main(void)
{
    puts("SKIP Phase 7 socket safety fixture: POSIX only; native Windows NOT VERIFIED.");
    return 0;
}
#else
#include "phase5_check.h"
static unsigned port;
static char seed[128];

static void requests(const unsigned *expected, size_t count)
{
    unsigned got[128];
    size_t actual = crawler_fixture_requests(got, 128), i;
    CHECK(actual == count);
    for (i = 0; i < count; ++i) { CHECK(got[i] == expected[i]); }
}

static void scoped(unsigned route, int same, HttpStatus expected,
                   const unsigned *sequence, size_t count)
{
    char url[160];
    HttpResponse response = {0};
    (void)snprintf(url, sizeof(url), "%ssafety/%u", seed, route);
    CHECK(http_download_scoped(url, 1, seed, same, &response) == expected);
    if (expected == HTTP_OK) {
        CHECK(response.body != NULL && response.final_url != NULL);
    } else {
        CHECK(response.body == NULL && response.size == 0);
        CHECK(response.detail[0] != '\0');
    }
    http_response_free(&response);
    http_response_free(&response);
    CHECK(response.body == NULL && response.final_url == NULL && response.content_type == NULL);
    requests(sequence, count);
}

int main(void)
{
    static const long rejected[] = {0, -1, LONG_MAX, HTTP_MAX_TIMEOUT_SECONDS + 1};
    static const long supported[] = {1, 3, HTTP_MAX_TIMEOUT_SECONDS};
    static const CURLoption options[] = {
        CURLOPT_URL, CURLOPT_FOLLOWLOCATION, CURLOPT_MAXREDIRS,
        CURLOPT_TIMEOUT, CURLOPT_CONNECTTIMEOUT, CURLOPT_SSL_VERIFYPEER,
        CURLOPT_SSL_VERIFYHOST, CURLOPT_WRITEFUNCTION, CURLOPT_WRITEDATA,
        CURLOPT_ERRORBUFFER, CURLOPT_USERAGENT, CURLOPT_MAXFILESIZE_LARGE,
        CURLOPT_NOSIGNAL, CURLOPT_HEADERFUNCTION, CURLOPT_HEADERDATA,
#if LIBCURL_VERSION_NUM >= 0x075500
        CURLOPT_PROTOCOLS_STR, CURLOPT_REDIR_PROTOCOLS_STR
#else
        CURLOPT_PROTOCOLS, CURLOPT_REDIR_PROTOCOLS
#endif
    };
    HttpResponse response = {0};
    CrawlerStats stats;
    CrawlerConfig config = {NULL, 1, 1, 1};
    char url[160];
    unsigned sequence[8];
    size_t i;
    port = crawler_fixture_start();
    (void)snprintf(seed, sizeof(seed), "http://127.0.0.1:%u/", port);
    (void)snprintf(url, sizeof(url), "%sok", seed);
    config.start_url = url;
    for (i = 0; i < sizeof(rejected) / sizeof(rejected[0]); ++i) {
        CHECK(http_download(url, rejected[i], &response) == HTTP_ERR_INVALID_ARGUMENT);
        CHECK(response.body == NULL && response.final_url == NULL && response.detail[0] != '\0');
        CHECK(http_download_scoped(url, rejected[i], seed, 1, &response) == HTTP_ERR_INVALID_ARGUMENT);
        config.timeout_seconds = rejected[i];
        CHECK(crawler_run(&config, &stats) == CRAWLER_INVALID_ARGUMENT);
        CHECK(stats.pages_visited == 0 && stats.pages_failed == 0 &&
              stats.links_discovered == 0 && stats.bytes_downloaded == 0 && stats.max_depth_reached == 0);
        requests(NULL, 0); /* Proof: no socket request for rejected configuration. */
    }
    for (i = 0; i < sizeof(supported) / sizeof(supported[0]); ++i) {
        CHECK(http_download(url, supported[i], &response) == HTTP_OK);
        CHECK(response.size == 2);
        http_response_free(&response);
        CHECK(http_download_scoped(url, supported[i], seed, 1, &response) == HTTP_OK);
        http_response_free(&response);
        config.timeout_seconds = supported[i];
        CHECK(crawler_run(&config, &stats) == CRAWLER_OK && stats.pages_visited == 1);
        requests((unsigned[]){FIX_OK, FIX_OK, FIX_OK}, 3);
    }
    for (i = 0; i < sizeof(options) / sizeof(options[0]); ++i) {
        failed_option = options[i];
        CHECK(http_download_scoped(url, 1, seed, 1, &response) == HTTP_ERR_INIT);
        CHECK(response.body == NULL && response.final_url == NULL && response.content_type == NULL);
        CHECK(response.detail[0] != '\0');
        requests(NULL, 0);
    }
    failed_option = CURLOPT_TIMEOUT;
    failed_code = CURLE_OUT_OF_MEMORY;
    CHECK(http_download_scoped(url, 1, seed, 1, &response) == HTTP_ERR_OUT_OF_MEMORY);
    CHECK(response.body == NULL && response.final_url == NULL);
    config.timeout_seconds = 1;
    CHECK(crawler_run(&config, &stats) == CRAWLER_OUT_OF_MEMORY);
    requests(NULL, 0);
    failed_option = (CURLoption)0;
    failed_code = CURLE_BAD_FUNCTION_ARGUMENT;
    for (i = 0; i < 5; ++i) {
        scoped((unsigned)i, 1, HTTP_OK, (unsigned[]){FIX_SAFETY_BASE + (unsigned)i, FIX_NEW_PAGE}, 2);
    }
    scoped(5, 1, HTTP_OK, (unsigned[]){FIX_SAFETY_BASE + 5, FIX_SAFETY_BASE + 40}, 2);
    scoped(6, 1, HTTP_OK, (unsigned[]){FIX_SAFETY_BASE + 6, FIX_OK}, 2);
    for (i = 0; i < 6; ++i) { sequence[i] = FIX_SAFETY_BASE + 7; }
    scoped(7, 1, HTTP_ERR_TOO_MANY_REDIRECTS, sequence, 6);
    for (i = 0; i < 6; ++i) { sequence[i] = FIX_SAFETY_BASE + 8 + (unsigned)i; }
    scoped(8, 1, HTTP_ERR_TOO_MANY_REDIRECTS, sequence, 6);
    for (i = 0; i < 6; ++i) { sequence[i] = FIX_SAFETY_BASE + 33 + (unsigned)i; }
    scoped(33, 1, HTTP_OK, sequence, 6);
    for (i = 16; i <= 19; ++i) {
        scoped((unsigned)i, 1, HTTP_ERR_HTTP_STATUS, (unsigned[]){FIX_SAFETY_BASE + (unsigned)i}, 1);
    }
    scoped(20, 1, HTTP_ERR_SCOPE, (unsigned[]){FIX_SAFETY_BASE + 20}, 1);
    scoped(20, 0, HTTP_OK, (unsigned[]){FIX_SAFETY_BASE + 20, FIX_OK | FIX_LOCALHOST_BIT}, 2);
    scoped(21, 1, HTTP_ERR_HTTP_STATUS, (unsigned[]){FIX_SAFETY_BASE + 21}, 1);
    scoped(22, 1, HTTP_ERR_HTTP_STATUS, (unsigned[]){FIX_SAFETY_BASE + 22}, 1);
    scoped(23, 1, HTTP_ERR_HTTP_STATUS, (unsigned[]){FIX_SAFETY_BASE + 23, FIX_MISSING}, 2);
    scoped(24, 1, HTTP_ERR_HTTP_STATUS, (unsigned[]){FIX_SAFETY_BASE + 24, FIX_BOOM}, 2);
    scoped(25, 1, HTTP_ERR_NOT_HTML, (unsigned[]){FIX_SAFETY_BASE + 25, FIX_PDF}, 2);
    scoped(26, 1, HTTP_ERR_NETWORK, (unsigned[]){FIX_SAFETY_BASE + 26}, 1);
    (void)snprintf(url, sizeof(url), "%ssafety/26", seed);
    config.start_url = url;
    CHECK(crawler_run(&config, &stats) == CRAWLER_NETWORK_ERROR);
    CHECK(stats.pages_visited == 0 && stats.pages_failed == 1 && stats.bytes_downloaded == 0);
    requests((unsigned[]){FIX_SAFETY_BASE + 26}, 1);
    (void)snprintf(url, sizeof(url), "%shang", seed);
    CHECK(http_download_scoped(url, 1, seed, 1, &response) == HTTP_ERR_TIMEOUT);
    CHECK(response.body == NULL && response.size == 0);
    http_response_free(&response);
    requests((unsigned[]){FIX_HANG}, 1);
    /* Standalone API intentionally STILL follows cross-host redirects. */
    (void)snprintf(url, sizeof(url), "%soff-redirect", seed);
    CHECK(http_download(url, 1, &response) == HTTP_OK);
    http_response_free(&response);
    requests((unsigned[]){FIX_OFF_REDIRECT, FIX_OFF_LANDING | FIX_LOCALHOST_BIT}, 2);
    (void)snprintf(url, sizeof(url), "%sport-redirect", seed);
    CHECK(http_download_scoped(url, 1, seed, 0, &response) == HTTP_ERR_SCOPE);
    CHECK(response.status_code == 302 && response.size == 0);
    http_response_free(&response);
    requests((unsigned[]){FIX_PORT_REDIRECT}, 1);
    (void)snprintf(url, sizeof(url), "%ssafety/29", seed);
    config.start_url = url;
    CHECK(crawler_run(&config, &stats) == CRAWLER_PARTIAL);
    CHECK(stats.pages_visited == 2 && stats.pages_failed == 1 && stats.links_discovered == 2);
    requests((unsigned[]){FIX_SAFETY_BASE + 29, FIX_OFF_REDIRECT, FIX_OK}, 3);
    (void)snprintf(url, sizeof(url), "%ssafety/30", seed);
    CHECK(crawler_run(&config, &stats) == CRAWLER_LIMIT_REACHED);
    CHECK(stats.pages_visited == 1 && stats.pages_failed == 1 && stats.links_discovered == 999);
    requests((unsigned[]){FIX_SAFETY_BASE + 30, FIX_SAFETY_BASE + 31}, 2);
    (void)snprintf(url, sizeof(url), "%ssafety/32", seed);
    CHECK(crawler_run(&config, &stats) == CRAWLER_LIMIT_REACHED);
    CHECK(stats.pages_visited == 1 && stats.pages_failed == 1 && stats.links_discovered == 999);
    requests((unsigned[]){FIX_SAFETY_BASE + 32, FIX_REDIRECT, FIX_NEW_PAGE}, 3);
    CHECK(http_to_crawler_status(HTTP_ERR_SCOPE) == CRAWLER_PARSE_ERROR);
    crawler_fixture_stop();
    printf("PASS Phase 7 safety: %zu checks; timeout bounds/17 option failures/redirects/partial bodies/cross-page and alias budgets\n", checks);
    return 0;
}
#endif
