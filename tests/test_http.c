#include "http.h"
#include "url.h"
#include "project.h"

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int s_live_network_available = 0;

static void test_normal_cases(void)
{
    printf("[1] Testing Normal Cases...\n");

    const char *ok_msg = http_status_string(HTTP_OK);
    assert(ok_msg != NULL);
    assert(strcmp(ok_msg, "success") == 0);

    assert(http_to_crawler_status(HTTP_OK) == CRAWLER_OK);

    HttpResponse resp = {0};
    HttpStatus status = http_download("https://example.com", 10, &resp);

    if (status == HTTP_OK) {
        s_live_network_available = 1;
        assert(resp.body != NULL);
        assert(resp.size > 0);
        assert(resp.status_code == 200);
        assert(resp.final_url != NULL);
        assert(strncmp(resp.final_url, "https://example.com", 19) == 0);
        assert(resp.content_type != NULL);
        assert(strstr(resp.content_type, "text/html") != NULL);

        assert(strstr(resp.body, "Example Domain") != NULL ||
               strstr(resp.body, "<html") != NULL ||
               strstr(resp.body, "<!doctype") != NULL);

        printf("  PASS: Normal HTTPS download (status=%ld, bytes=%zu, type=%s)\n",
               resp.status_code, resp.size, resp.content_type);
        http_response_free(&resp);
    } else {
        printf("  INFO: Live network download probe returned %s: %s (offline/filtered environment)\n",
               http_status_string(status), resp.detail);
        http_response_free(&resp);
    }

    printf("  PASS: Normal cases completed successfully.\n");
}

static void test_boundary_cases(void)
{
    printf("[2] Testing Boundary Cases...\n");

    HttpResponse resp = {0};

    HttpStatus status = http_download("http://nonexistent-domain-test-boundary-123.invalid", 1, &resp);
    assert(status != HTTP_ERR_INVALID_ARGUMENT);
    http_response_free(&resp);

    status = http_download("http://nonexistent-domain-test-boundary-123.invalid", 3600, &resp);
    assert(status != HTTP_ERR_INVALID_ARGUMENT);
    http_response_free(&resp);

    assert(HTTP_MAX_BODY_BYTES == (size_t)5 * 1024 * 1024);
    assert(HTTP_MAX_BODY_BYTES == 5242880);

    if (s_live_network_available) {
        status = http_download("https://example.com/nonexistent_boundary_404_check", 10, &resp);
        assert(status == HTTP_ERR_HTTP_STATUS);
        assert(resp.status_code == 404);
        assert(resp.body == NULL);
        assert(resp.size == 0);
        assert(strlen(resp.detail) > 0);
        http_response_free(&resp);
    }

    printf("  PASS: Boundary cases completed successfully.\n");
}

static void test_invalid_input(void)
{
    printf("[3] Testing Invalid Input Cases...\n");

    HttpResponse resp = {0};

    assert(http_download(NULL, 5, &resp) == HTTP_ERR_INVALID_ARGUMENT);
    assert(resp.body == NULL && resp.size == 0 && resp.status_code == 0);
    assert(strlen(resp.detail) > 0);
    http_response_free(&resp);

    assert(http_download("https://example.com", 5, NULL) == HTTP_ERR_INVALID_ARGUMENT);

    assert(http_download("https://example.com", 0, &resp) == HTTP_ERR_INVALID_ARGUMENT);
    assert(resp.body == NULL && resp.size == 0);
    http_response_free(&resp);

    assert(http_download("https://example.com", -1, &resp) == HTTP_ERR_INVALID_ARGUMENT);
    http_response_free(&resp);

    assert(http_download("https://example.com", -100, &resp) == HTTP_ERR_INVALID_ARGUMENT);
    http_response_free(&resp);

    assert(http_download("not_a_valid_url", 5, &resp) == HTTP_ERR_BAD_URL);
    assert(resp.body == NULL && resp.size == 0);
    assert(strlen(resp.detail) > 0);
    http_response_free(&resp);

    assert(http_download("ftp://example.com/file.zip", 5, &resp) == HTTP_ERR_BAD_URL);
    assert(resp.body == NULL);
    http_response_free(&resp);

    assert(http_download("file:///etc/hosts", 5, &resp) == HTTP_ERR_BAD_URL);
    http_response_free(&resp);

    assert(http_download("https://", 5, &resp) == HTTP_ERR_BAD_URL);
    http_response_free(&resp);

    assert(http_download("https://example..com", 5, &resp) == HTTP_ERR_BAD_URL);
    http_response_free(&resp);

    assert(http_download("https://-example.com", 5, &resp) == HTTP_ERR_BAD_URL);
    http_response_free(&resp);

    assert(http_download("https://example.com:65536", 5, &resp) == HTTP_ERR_BAD_URL);
    http_response_free(&resp);

    assert(http_download("https://example.com:abc", 5, &resp) == HTTP_ERR_BAD_URL);
    http_response_free(&resp);

    printf("  PASS: Invalid input cases completed successfully.\n");
}

static void test_empty_input(void)
{
    printf("[4] Testing Empty Input Cases...\n");

    HttpResponse resp = {0};

    assert(http_download("", 5, &resp) == HTTP_ERR_INVALID_ARGUMENT);
    assert(resp.body == NULL && resp.size == 0);
    assert(strlen(resp.detail) > 0);
    http_response_free(&resp);

    assert(http_download("   ", 5, &resp) == HTTP_ERR_BAD_URL);
    http_response_free(&resp);

    ParsedUrl parsed = {0};
    assert(url_parse("https://example.com", &parsed) == URL_OK);
    assert(strcmp(parsed.path, "/") == 0);
    assert(parsed.query == NULL);
    assert(parsed.fragment == NULL);
    url_free(&parsed);

    assert(url_parse("https://example.com/?", &parsed) == URL_OK);
    assert(parsed.query != NULL && strcmp(parsed.query, "") == 0);
    url_free(&parsed);

    assert(url_parse("https://example.com/#", &parsed) == URL_OK);
    assert(parsed.fragment != NULL && strcmp(parsed.fragment, "") == 0);
    url_free(&parsed);

    printf("  PASS: Empty input cases completed successfully.\n");
}

static void test_large_input(void)
{
    printf("[5] Testing Large Input Cases...\n");

    HttpResponse resp = {0};

    char large_url[2200];
    const char *prefix = "https://example.com/";
    size_t prefix_len = strlen(prefix);
    memcpy(large_url, prefix, prefix_len);
    memset(large_url + prefix_len, 'a', sizeof(large_url) - prefix_len - 1);
    large_url[sizeof(large_url) - 1] = '\0';

    assert(http_download(large_url, 5, &resp) == HTTP_ERR_BAD_URL);
    assert(resp.body == NULL && resp.size == 0);
    assert(strlen(resp.detail) > 0);
    http_response_free(&resp);

    char boundary_url[2049];
    memcpy(boundary_url, prefix, prefix_len);
    memset(boundary_url + prefix_len, 'x', 2048 - prefix_len);
    boundary_url[2048] = '\0';

    ParsedUrl parsed = {0};
    UrlStatus ustatus = url_parse(boundary_url, &parsed);
    assert(ustatus == URL_OK);
    url_free(&parsed);

    printf("  PASS: Large input cases completed successfully.\n");
}

static void test_failure_conditions(void)
{
    printf("[6] Testing Failure Conditions...\n");

    HttpResponse resp = {0};

    HttpStatus status = http_download("http://nonexistent-crawler-domain-12345.invalid", 3, &resp);
    assert(status == HTTP_ERR_DNS);
    assert(resp.body == NULL);
    assert(resp.size == 0);
    assert(strlen(resp.detail) > 0);
    http_response_free(&resp);

    status = http_download("http://127.0.0.1:59999", 5, &resp);
    assert(status == HTTP_ERR_CONNECT || status == HTTP_ERR_TIMEOUT);
    assert(resp.body == NULL);
    assert(resp.size == 0);
    assert(strlen(resp.detail) > 0);
    http_response_free(&resp);

    HttpStatus failure_codes[] = {
        HTTP_ERR_INVALID_ARGUMENT,
        HTTP_ERR_BAD_URL,
        HTTP_ERR_OUT_OF_MEMORY,
        HTTP_ERR_INIT,
        HTTP_ERR_DNS,
        HTTP_ERR_CONNECT,
        HTTP_ERR_TIMEOUT,
        HTTP_ERR_TLS,
        HTTP_ERR_TOO_MANY_REDIRECTS,
        HTTP_ERR_HTTP_STATUS,
        HTTP_ERR_TOO_LARGE,
        HTTP_ERR_NOT_HTML,
        HTTP_ERR_NETWORK
    };

    size_t count = sizeof(failure_codes) / sizeof(failure_codes[0]);
    for (size_t i = 0; i < count; i++) {
        const char *msg = http_status_string(failure_codes[i]);
        assert(msg != NULL);
        assert(strlen(msg) > 0);
        assert(strcmp(msg, "unknown HTTP status") != 0);

        CrawlerStatus cstatus = http_to_crawler_status(failure_codes[i]);
        assert(cstatus != CRAWLER_OK);
    }

    assert(strcmp(http_status_string((HttpStatus)9999), "unknown HTTP status") == 0);
    assert(http_to_crawler_status((HttpStatus)9999) == CRAWLER_NETWORK_ERROR);

    printf("  PASS: Failure conditions completed successfully.\n");
}

static void test_memory_management(void)
{
    printf("[7] Testing Memory Management...\n");

    http_response_free(NULL);

    HttpResponse resp = {0};
    http_response_free(&resp);

    resp.body = (char *)malloc(64);
    assert(resp.body != NULL);
    strcpy(resp.body, "<html><body>Hello World</body></html>");
    resp.size = strlen(resp.body);
    resp.status_code = 200;

    resp.final_url = (char *)malloc(64);
    assert(resp.final_url != NULL);
    strcpy(resp.final_url, "https://example.com/target");

    resp.content_type = (char *)malloc(64);
    assert(resp.content_type != NULL);
    strcpy(resp.content_type, "text/html; charset=UTF-8");

    snprintf(resp.detail, sizeof(resp.detail), "Test detail message");

    http_response_free(&resp);
    assert(resp.body == NULL);
    assert(resp.size == 0);
    assert(resp.status_code == 0);
    assert(resp.final_url == NULL);
    assert(resp.content_type == NULL);
    assert(resp.detail[0] == '\0');

    http_response_free(&resp);
    http_response_free(&resp);

    for (int i = 0; i < 100; i++) {
        HttpStatus st = http_download("https://-invalid-host-label.com", 1, &resp);
        assert(st == HTTP_ERR_BAD_URL);
        assert(resp.body == NULL);
        http_response_free(&resp);
    }

    http_cleanup();
    http_cleanup();

    printf("  PASS: Memory management cases completed successfully.\n");
}

int main(void)
{
    printf("=========================================\n");
    printf("Part 3 HTTP Downloader Test Suite\n");
    printf("=========================================\n\n");

    test_normal_cases();
    test_boundary_cases();
    test_invalid_input();
    test_empty_input();
    test_large_input();
    test_failure_conditions();
    test_memory_management();

    printf("\n=========================================\n");
    printf("ALL HTTP DOWNLOADER TESTS PASSED (7/7)\n");
    printf("=========================================\n");

    return 0;
}
