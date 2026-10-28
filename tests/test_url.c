
#include "url.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Test a URL that should be accepted.
 *
 * expected_host and expected_path tell us
 * what the parser should extract.
 */
static void test_valid(
    const char *input,
    const char *expected_host,
    const char *expected_path)
{
    // Start with all pointers initialized to NULL.
    ParsedUrl url = {0};

    // Call our URL parsing function.
    int status = url_parse(input, &url);

    // Parsing must succeed.
    assert(status == 0);

    // The extracted hostname must match.
    assert(strcmp(url.host, expected_host) == 0);

    // The extracted path must match.
    assert(strcmp(url.path, expected_path) == 0);

    // Release dynamically allocated strings.
    url_free(&url);
}

/*
 * Test a URL that should be rejected.
 */
static void test_invalid(const char *input)
{
    ParsedUrl url = {0};

    // Invalid URLs must return a non-success status.
    int status = url_parse(input, &url);

    assert(status != URL_OK);

    // No memory should be left allocated.
    assert(url.scheme == NULL);
    assert(url.host == NULL);
    assert(url.path == NULL);
}

/*
 * Test a complete URL containing a port,
 * path, query and fragment.
 */
static void test_complete_url(void)
{
    // Initialize every structure field to NULL.
    ParsedUrl url = {0};

    const char *input =
        "https://example.com:8080/products"
        "?color=red#reviews";

    // Parse the complete URL.
    int status = url_parse(input, &url);

    // Verify that parsing succeeded.
    assert(status == 0);

    // Verify each extracted component.
    assert(strcmp(url.scheme, "https") == 0);
    assert(strcmp(url.host, "example.com") == 0);
    assert(strcmp(url.port, "8080") == 0);
    assert(strcmp(url.path, "/products") == 0);
    assert(strcmp(url.query, "color=red") == 0);
    assert(strcmp(url.fragment, "reviews") == 0);

    // Display the parsed information.
    printf("Scheme:   %s\n", url.scheme);
    printf("Host:     %s\n", url.host);
    printf("Port:     %s\n", url.port);
    printf("Path:     %s\n", url.path);
    printf("Query:    %s\n", url.query);
    printf("Fragment: %s\n", url.fragment);

    // Release allocated memory.
    url_free(&url);
}

/*
 * Test optional URL components.
 */
static void test_optional_components(void)
{
    ParsedUrl url = {0};

    // URL contains no port, query or fragment.
    assert(url_parse("https://example.com", &url) == 0);

    assert(strcmp(url.path, "/") == 0);
    assert(url.port == NULL);
    assert(url.query == NULL);
    assert(url.fragment == NULL);

    url_free(&url);

    // URL contains a query but no explicit path.
    assert(url_parse("https://example.com?x=1", &url) == 0);

    assert(strcmp(url.path, "/") == 0);
    assert(strcmp(url.query, "x=1") == 0);

    url_free(&url);

    // An explicitly empty query is not the same
    // as a missing query.
    assert(url_parse("https://example.com/?", &url) == 0);

    assert(url.query != NULL);
    assert(strcmp(url.query, "") == 0);

    url_free(&url);
}

/*
 * Test invalid port values.
 */
static void test_invalid_ports(void)
{
    // Reuse the test_invalid helper from Part 2.2.
    test_invalid("https://example.com:");
    test_invalid("https://example.com:abc");
    test_invalid("https://example.com:0");
    test_invalid("https://example.com:65536");
    test_invalid("https://example.com:80:90");
}

static void test_status_codes(void)
{
    ParsedUrl url = {0};
    assert(url_parse(NULL, &url) == URL_ERROR_INVALID_ARGUMENT);
    assert(url_parse("", &url) == URL_ERROR_INVALID_FORMAT);
    assert(url_parse("ftp://example.com/", &url) == URL_ERROR_UNSUPPORTED_SCHEME);
    assert(url_parse("https:///missing-host", &url) == URL_ERROR_INVALID_HOST);
    assert(url_parse("https://example.com:0/", &url) == URL_ERROR_INVALID_PORT);
    assert(url_parse("https://example.com:", &url) == URL_ERROR_INVALID_PORT);
    assert(url_parse("https://example.com:abc/", &url) == URL_ERROR_INVALID_PORT);
    assert(url_parse("https://bad..host/", &url) == URL_ERROR_INVALID_HOST);
    assert(strcmp(url_status_message(URL_ERROR_INVALID_PORT), "invalid port") == 0);
    assert(url_status_message((UrlStatus)999) != NULL);

    assert(url_parse("https://example.com/", &url) == URL_OK);
    assert(url_parse("https://other.example/", &url) == URL_ERROR_INVALID_ARGUMENT);
    assert(strcmp(url.host, "example.com") == 0); /* Existing ownership is preserved. */
    url_free(&url);
    url_free(&url); /* Clearing makes a second free safe. */
}

static void test_path_normalization(void)
{
    char *result = NULL;
    assert(url_normalize("https://a.test/a/", &result) == URL_OK);
    assert(strcmp(result, "https://a.test/a/") == 0);
    free(result);
    assert(url_normalize("https://a.test/a/.", &result) == URL_OK);
    assert(strcmp(result, "https://a.test/a/") == 0);
    free(result);
    assert(url_normalize("https://a.test/a/b/..", &result) == URL_OK);
    assert(strcmp(result, "https://a.test/a/") == 0);
    free(result);
    assert(url_normalize("https://a.test/a/../", &result) == URL_OK);
    assert(strcmp(result, "https://a.test/") == 0);
    free(result);
    assert(url_normalize("https://a.test/a//b/", &result) == URL_OK);
    assert(strcmp(result, "https://a.test/a//b/") == 0);
    free(result);
    assert(url_normalize("http://a.test:80/a/../b#x", &result) == URL_OK);
    assert(strcmp(result, "http://a.test/b") == 0);
    free(result);
    assert(url_normalize("https://a.test:443/a", &result) == URL_OK);
    assert(strcmp(result, "https://a.test/a") == 0);
    free(result);
    assert(url_normalize("https://a.test/%2F/%7e", &result) == URL_OK);
    assert(strcmp(result, "https://a.test/%2F/%7e") == 0);
    free(result);
    assert(url_normalize("http://EXAMPLE.com:80/a/../b#x", &result) == URL_OK);
    assert(strcmp(result, "http://example.com/b") == 0);
    free(result);
}

static void test_reference_resolution(void)
{
    char *result = NULL;
    size_t i;
    assert(url_resolve("https://example.com/a/page.html?old=1#oldfrag",
                       "#part", &result) == URL_OK);
    assert(strcmp(result, "https://example.com/a/page.html?old=1") == 0);
    free(result);
    assert(url_resolve("https://example.com/a/page.html?old=1#frag",
                       "", &result) == URL_OK);
    assert(strcmp(result, "https://example.com/a/page.html?old=1") == 0);
    free(result);
    assert(url_resolve("https://example.com/a/b/page.html", "../../../../x",
                       &result) == URL_OK);
    assert(strcmp(result, "https://example.com/x") == 0);
    free(result);
    assert(url_resolve("https://example.com/a/page.html", "/root?q=1",
                       &result) == URL_OK);
    assert(strcmp(result, "https://example.com/root?q=1") == 0);
    free(result);
    assert(url_resolve("https://example.com/a/page.html", "next.html",
                       &result) == URL_OK);
    assert(strcmp(result, "https://example.com/a/next.html") == 0);
    free(result);
    assert(url_resolve("https://example.com/a/page.html", "//other.test/x",
                       &result) == URL_OK);
    assert(strcmp(result, "https://other.test/x") == 0);
    free(result);
    assert(url_resolve("https://example.com/a/page.html", "http://other.test/x",
                       &result) == URL_OK);
    assert(strcmp(result, "http://other.test/x") == 0);
    free(result);
    assert(url_resolve("https://example.com/a/page.html", "mailto:user@example.com",
                       &result) == URL_ERROR_UNSUPPORTED_SCHEME);
    assert(result == NULL);

    /* Absolute references repeatedly exercise the formerly leaking branch. */
    for (i = 0; i < 10000; ++i) {
        assert(url_resolve("https://example.com/base/", "https://other.test/x",
                           &result) == URL_OK);
        assert(strcmp(result, "https://other.test/x") == 0);
        free(result);
        result = NULL;
    }
    assert(url_resolve("https://example.com/a/page.html", "http://[bad",
                       &result) != URL_OK);
    assert(result == NULL);
}

static void test_url_boundaries(void)
{
    char *url = NULL, *result = NULL;
    size_t prefix_length, target_length;
    const char *prefix = "https://a.test/";
    ParsedUrl parsed = {0};

    prefix_length = strlen(prefix);
    for (target_length = 2047; target_length <= 2049; ++target_length) {
        url = malloc(target_length + 1);
        assert(url != NULL);
        memcpy(url, prefix, prefix_length);
        memset(url + prefix_length, 'x', target_length - prefix_length);
        url[target_length] = '\0';
        if (target_length <= 2048) {
            assert(url_parse(url, &parsed) == URL_OK);
            url_free(&parsed);
        } else {
            assert(url_parse(url, &parsed) == URL_ERROR_TOO_LONG);
        }
        free(url);
    }
    assert(url_parse("http://a.test:1/", &parsed) == URL_OK);
    url_free(&parsed);
    assert(url_parse("http://a.test:65535/", &parsed) == URL_OK);
    url_free(&parsed);
    assert(url_parse("http://a.test:65536/", &parsed) == URL_ERROR_INVALID_PORT);
    assert(url_normalize("https://a.test/", NULL) == URL_ERROR_INVALID_ARGUMENT);
    assert(url_resolve("bad base", "x", &result) == URL_ERROR_INVALID_FORMAT);
    assert(result == NULL);

    /* Exercise the maximum-length query as well as maximum-length paths. */
    {
        char query_url[2050];
        const char *query_prefix = "https://a.test/?";
        size_t query_prefix_length = strlen(query_prefix);
        memcpy(query_url, query_prefix, query_prefix_length);
        memset(query_url + query_prefix_length, 'q', 2048 - query_prefix_length);
        query_url[2048] = '\0';
        assert(url_parse(query_url, &parsed) == URL_OK);
        url_free(&parsed);
        query_url[2048] = 'q';
        query_url[2049] = '\0';
        assert(url_parse(query_url, &parsed) == URL_ERROR_TOO_LONG);
    }

    /* Maximum DNS hostname and label sizes, then one over each limit. */
    {
        char host[256];
        char full_url[280];
        memset(host, 'a', 63); host[63] = '.';
        memset(host + 64, 'b', 63); host[127] = '.';
        memset(host + 128, 'c', 63); host[191] = '.';
        memset(host + 192, 'd', 61); host[253] = '\0';
        snprintf(full_url, sizeof(full_url), "http://%s/", host);
        assert(url_parse(full_url, &parsed) == URL_OK);
        url_free(&parsed);
        host[253] = 'e'; host[254] = '\0';
        snprintf(full_url, sizeof(full_url), "http://%s/", host);
        assert(url_parse(full_url, &parsed) == URL_ERROR_INVALID_HOST);

        memset(host, 'z', 64); host[64] = '\0';
        snprintf(full_url, sizeof(full_url), "http://%s.test/", host);
        assert(url_parse(full_url, &parsed) == URL_ERROR_INVALID_HOST);
    }
}

int main(void)
{
    char *normalized = NULL, *resolved = NULL;
    /*
     * TEST GROUP 1: Valid URLs
     */
    test_valid(
        "https://example.com/about",
        "example.com",
        "/about"
    );

    test_valid(
        "http://example.org",
        "example.org",
        "/"
    );

    test_valid(
        "https://my-site.com/products",
        "my-site.com",
        "/products"
    );

    /*
     * TEST GROUP 2: Invalid URLs
     */
    test_invalid(NULL);
    test_invalid("");
    test_invalid("example.com");
    test_invalid("ftp://example.com");
    test_invalid("https://");
    test_invalid("https://example..com");
    test_invalid("https://-example.com");
    test_invalid("https://example.com/about us");
    test_invalid("https://example.com/<bad>");
    test_invalid("https://example.com/%XZ");
    test_invalid("https://example.com:abc");

    /*
     * TEST GROUP 3: Boundary cases
     */
    char large_url[2100];
    test_complete_url();
    test_optional_components();
    test_invalid_ports();

    printf("\nAll URL tests passed.\n");


    // Build a valid URL whose length exceeds the parser limit.
    const char *long_url_prefix = "https://example.com/";
    size_t prefix_length = strlen(long_url_prefix);
    memcpy(large_url, long_url_prefix, prefix_length);
    memset(large_url + prefix_length, 'a',
           sizeof(large_url) - prefix_length - 1);
    large_url[sizeof(large_url) - 1] = '\0';
    large_url[sizeof(large_url) - 1] = '\0';

    test_invalid(large_url);

    /*
     * TEST GROUP 4: Invalid output pointer
     */
    assert(url_parse("https://example.com", NULL) == URL_ERROR_INVALID_ARGUMENT);

    assert(url_normalize("HTTP://Example.COM:80/a/./b/../c?q=1#part", &normalized) == URL_OK);
    assert(strcmp(normalized, "http://example.com/a/c?q=1") == 0);
    free(normalized);
    assert(url_resolve("https://example.com/products/page.html", "../about.html", &resolved) == URL_OK);
    assert(strcmp(resolved, "https://example.com/about.html") == 0);
    free(resolved);
    test_status_codes();
    test_path_normalization();
    test_reference_resolution();
    test_url_boundaries();

    printf("All URL validation tests passed.\n");

    return 0;
}
