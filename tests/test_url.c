
#include "url.h"

#include <assert.h>
#include <stdio.h>
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

    // Invalid URLs must return -1.
    int status = url_parse(input, &url);

    assert(status == -1);

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

int main(void)
{
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
    assert(url_parse("https://example.com", NULL) == -1);

    printf("All URL validation tests passed.\n");

    return 0;
}
