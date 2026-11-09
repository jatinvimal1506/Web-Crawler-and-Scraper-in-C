#include "download.h"
#include "download_fixture.h"
#include "http.h"
#include "../src/http_internal.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

static unsigned checks;
static unsigned failures;

#define CHECK(expression) do { \
    ++checks; \
    if (!(expression)) { \
        ++failures; \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expression); \
    } \
} while (0)

static DownloadStatus attempt(const DownloadConfig *config)
{
    DownloadResult result = {0};
    DownloadStatus status = download_document(config, &result);
    CHECK(result.status == status);
    download_result_free(&result);
    download_result_free(&result);
    CHECK(result.status == DOWNLOAD_OK);
    CHECK(result.final_url == NULL && result.file_path == NULL);
    return status;
}

static void test_redirect_scope_policy(void)
{
    ParsedUrl seed = {0}, target = {0};
    CHECK(url_parse("http://example.test:443/seed.pdf", &seed) == URL_OK);
    CHECK(url_parse("https://example.test:443/next.pdf", &target) == URL_OK);
    CHECK(http_scope_allows(&seed, &target, 1, 0));
    url_free(&target);
    url_free(&seed);

    CHECK(url_parse("https://example.test:443/seed.pdf", &seed) == URL_OK);
    CHECK(url_parse("http://example.test:443/downgrade.pdf", &target) == URL_OK);
    CHECK(!http_scope_allows(&seed, &target, 1, 1));
    url_free(&target);
    url_free(&seed);

    CHECK(url_parse("http://example.test:443/seed.pdf", &seed) == URL_OK);
    CHECK(url_parse("http://example.test:444/off-port.pdf", &target) == URL_OK);
    CHECK(!http_scope_allows(&seed, &target, 0, 0));
    url_free(&target);
    CHECK(url_parse("http://other.test:443/other.pdf", &target) == URL_OK);
    CHECK(!http_scope_allows(&seed, &target, 1, 0));
    CHECK(http_scope_allows(&seed, &target, 0, 0));
    url_free(&target);
    url_free(&seed);
}

int main(void)
{
    DownloadResult result = {0};
    DownloadConfig config = {0};
    char url[256], seed[256], off_scope[256];
    char long_name[DOWNLOAD_MAX_FILENAME_BYTES + 2];
    static const char *const invalid_names[] = {
        "", ".", "..", "../small.pdf", "..\\small.pdf", "C:small.pdf",
        "C:\\small.pdf", "subdir/small.pdf", "CON.pdf", "PRN.pdf",
        "AUX.pdf", "NUL.pdf", "COM1.pdf", "COM9.pdf", "LPT1.pdf",
        "LPT9.pdf", "trailing.pdf.", "trailing-space.pdf "
    };
    size_t bad_index;
    unsigned port = download_fixture_start();
    unsigned base_requests;

    test_redirect_scope_policy();
    CHECK(download_result_init(&result) == DOWNLOAD_OK);
    CHECK(download_result_init(&result) == DOWNLOAD_OK);
    CHECK(download_document(NULL, &result) == DOWNLOAD_ERROR_INVALID_ARGUMENT);
    CHECK(result.status == DOWNLOAD_ERROR_INVALID_ARGUMENT);
    download_result_free(&result);
    CHECK(download_result_init(NULL) == DOWNLOAD_ERROR_INVALID_ARGUMENT);
    download_result_free(NULL);

    (void)snprintf(url, sizeof(url), "http://127.0.0.1:%u/files/small.pdf",
                   port != 0 ? port : 1);
    (void)snprintf(seed, sizeof(seed), "http://127.0.0.1:%u/seed.pdf",
                   port != 0 ? port : 1);
    config.document_url = url;
    config.seed_url = seed;
    config.same_domain = 1;
    config.timeout_seconds = 3;
    config.output_directory = ".";
    config.filename = "small.pdf";

    config.timeout_seconds = 0;
    CHECK(attempt(&config) == DOWNLOAD_ERROR_INVALID_ARGUMENT);
    config.timeout_seconds = -1;
    CHECK(attempt(&config) == DOWNLOAD_ERROR_INVALID_ARGUMENT);
    config.timeout_seconds = LONG_MAX;
    CHECK(attempt(&config) == DOWNLOAD_ERROR_INVALID_ARGUMENT);
    config.timeout_seconds = DOWNLOAD_MAX_TIMEOUT_SECONDS + 1;
    CHECK(attempt(&config) == DOWNLOAD_ERROR_INVALID_ARGUMENT);
    config.timeout_seconds = 3;

    for (bad_index = 0;
         bad_index < sizeof(invalid_names) / sizeof(invalid_names[0]);
         ++bad_index) {
        config.filename = invalid_names[bad_index];
        CHECK(attempt(&config) == DOWNLOAD_ERROR_INVALID_ARGUMENT);
    }
    memset(long_name, 'a', DOWNLOAD_MAX_FILENAME_BYTES - 3);
    memcpy(long_name + DOWNLOAD_MAX_FILENAME_BYTES - 3, ".pdf", 5);
    config.filename = long_name;
    CHECK(attempt(&config) == DOWNLOAD_ERROR_INVALID_ARGUMENT);
    config.filename = "small.pdf";

    config.document_url = "https://example.invalid/file.txt";
    CHECK(attempt(&config) == DOWNLOAD_ERROR_UNSUPPORTED_TYPE);
    config.document_url = "ftp://example.invalid/file.pdf";
    CHECK(attempt(&config) == DOWNLOAD_ERROR_INVALID_ARGUMENT);
    config.document_url = url;

    config.same_domain = 2;
    CHECK(attempt(&config) == DOWNLOAD_ERROR_INVALID_ARGUMENT);
    config.same_domain = 1;
    config.seed_url = "not a url";
    CHECK(attempt(&config) == DOWNLOAD_ERROR_INVALID_ARGUMENT);
    config.seed_url = seed;
    config.output_directory = "no-such-output-directory";
    CHECK(attempt(&config) == DOWNLOAD_ERROR_INVALID_ARGUMENT);
    config.output_directory = ".";

    (void)snprintf(off_scope, sizeof(off_scope),
                   "http://localhost:%u/files/small.pdf", port != 0 ? port : 1);
    config.document_url = off_scope;
    CHECK(attempt(&config) == DOWNLOAD_ERROR_SCOPE);
    (void)snprintf(off_scope, sizeof(off_scope),
                   "http://127.0.0.1:%u/files/small.pdf",
                   port == 65535 ? 65534 : (port != 0 ? port + 1 : 2));
    config.document_url = off_scope;
    CHECK(attempt(&config) == DOWNLOAD_ERROR_SCOPE);
    config.document_url = url;

    result.status = DOWNLOAD_ERROR_NETWORK;
    CHECK(download_result_init(&result) == DOWNLOAD_ERROR_INVALID_ARGUMENT);
    CHECK(result.status == DOWNLOAD_ERROR_NETWORK);
    download_result_free(&result);
    download_result_free(&result);

    if (port != 0) {
        base_requests = (unsigned)download_fixture_request_count(DFX_PDF);
        CHECK(base_requests == 0);
    } else {
        puts("SKIP: POSIX loopback request-counter assertion (native Windows)");
    }
    download_fixture_stop();
    http_cleanup();
    printf("download unit: %u checks, %u failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
