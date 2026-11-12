#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#include "download.h"
#include "download_fixture.h"
#include "http.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static unsigned checks;
static unsigned failures;
static unsigned server_port;
static char output_directory[128];

#define CHECK(expression) do { \
    ++checks; \
    if (!(expression)) { \
        ++failures; \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expression); \
    } \
} while (0)

static DownloadStatus fetch(const char *path, const char *filename,
                            int same_domain, long timeout, DownloadResult *result)
{
    char document_url[512], seed_url[512];
    DownloadConfig config;
    (void)snprintf(document_url, sizeof(document_url),
                   "http://127.0.0.1:%u%s", server_port, path);
    (void)snprintf(seed_url, sizeof(seed_url),
                   "http://127.0.0.1:%u/seed.pdf", server_port);
    config.document_url = document_url;
    config.seed_url = seed_url;
    config.same_domain = same_domain;
    config.timeout_seconds = timeout;
    config.output_directory = output_directory;
    config.filename = filename;
    return download_document(&config, result);
}

static void expected_path(char *destination, size_t capacity, const char *filename)
{
    int written = snprintf(destination, capacity, "%s/%s", output_directory, filename);
    CHECK(written > 0 && (size_t)written < capacity);
}

static int verify_small_file(const char *path)
{
    size_t expected_size, read_count;
    const unsigned char *expected = download_fixture_small_data(&expected_size);
    unsigned char actual[32];
    FILE *file = fopen(path, "rb");
    int ok;
    if (file == NULL) { return 0; }
    read_count = fread(actual, 1, sizeof(actual), file);
    ok = read_count == expected_size &&
         memcmp(actual, expected, expected_size) == 0 &&
         fgetc(file) == EOF;
    if (fclose(file) != 0) { ok = 0; }
    return ok;
}

static int verify_pattern_file(const char *path, size_t expected_size)
{
    unsigned char buffer[8192];
    size_t offset = 0;
    FILE *file = fopen(path, "rb");
    if (file == NULL) { return 0; }
    while (offset < expected_size) {
        size_t amount = expected_size - offset, i, got;
        if (amount > sizeof(buffer)) { amount = sizeof(buffer); }
        got = fread(buffer, 1, amount, file);
        if (got != amount) { (void)fclose(file); return 0; }
        for (i = 0; i < amount; ++i) {
            if (buffer[i] != download_fixture_expected_byte(offset + i)) {
                (void)fclose(file);
                return 0;
            }
        }
        offset += amount;
    }
    {
        int ok = fgetc(file) == EOF;
        if (fclose(file) != 0) { ok = 0; }
        return ok;
    }
}

static void remove_result_file(DownloadResult *result)
{
    if (result->file_path != NULL) {
        CHECK(remove(result->file_path) == 0);
    }
    download_result_free(result);
}

static void expect_error(const char *path, const char *filename,
                         DownloadStatus expected, long timeout)
{
    DownloadResult result = {0};
    char full_path[512];
    DownloadStatus status = fetch(path, filename, 1, timeout, &result);
    expected_path(full_path, sizeof(full_path), filename);
    if (status != expected) {
        fprintf(stderr, "unexpected status for %s: got %s (%d), expected %s (%d)\n",
                path, download_status_message(status), (int)status,
                download_status_message(expected), (int)expected);
    }
    CHECK(status == expected);
    CHECK(result.status == expected);
    CHECK(result.file_path == NULL);
    if (expected != DOWNLOAD_ERROR_EXISTS) {
        CHECK(access(full_path, F_OK) != 0);
    }
    download_result_free(&result);
}

static void expect_success(const char *path, const char *filename)
{
    DownloadResult result = {0};
    char full_path[512];
    DownloadStatus status = fetch(path, filename, 1, 3, &result);
    expected_path(full_path, sizeof(full_path), filename);
    CHECK(status == DOWNLOAD_OK);
    CHECK(result.status == DOWNLOAD_OK);
    CHECK(result.file_path != NULL && strcmp(result.file_path, full_path) == 0);
    CHECK(result.final_url != NULL);
    CHECK(result.http_status == 200);
    CHECK(verify_small_file(full_path));
    remove_result_file(&result);
}

static void test_size_limit(const char *route, const char *filename,
                            size_t size, DownloadStatus expected)
{
    DownloadResult result = {0};
    char full_path[512];
    DownloadStatus status = fetch(route, filename, 1, 30, &result);
    expected_path(full_path, sizeof(full_path), filename);
    CHECK(status == expected);
    CHECK(result.status == expected);
    if (expected == DOWNLOAD_OK) {
        CHECK(result.bytes_written == size);
        CHECK(verify_pattern_file(full_path, size));
        remove_result_file(&result);
    } else {
        CHECK(result.file_path == NULL);
        CHECK(access(full_path, F_OK) != 0);
        CHECK(result.bytes_written <= DOWNLOAD_MAX_BYTES);
        download_result_free(&result);
    }
}

static void test_redirect(unsigned route, const char *path, const char *filename)
{
    download_fixture_reset_counts();
    expect_success(path, filename);
    CHECK(download_fixture_request_count(route) == 1);
    CHECK(download_fixture_request_count(DFX_PDF) == 1);
}

static void test_cli_demo(void)
{
    char document_url[256], seed_url[256], path[512];
    pid_t child;
    int status;
    (void)snprintf(document_url, sizeof(document_url),
                   "http://127.0.0.1:%u/files/small.pdf", server_port);
    (void)snprintf(seed_url, sizeof(seed_url),
                   "http://127.0.0.1:%u/seed.pdf", server_port);
    child = fork();
    CHECK(child >= 0);
    if (child == 0) {
        execl("./build/download-document", "./build/download-document",
              document_url, seed_url, output_directory, "cli-demo.pdf",
              "1", "3", (char *)NULL);
        perror("exec download-document");
        _exit(127);
    }
    if (child < 0) { return; }
    if (waitpid(child, &status, 0) != child) {
        CHECK(0);
        return;
    }
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    expected_path(path, sizeof(path), "cli-demo.pdf");
    CHECK(verify_small_file(path));
    CHECK(download_fixture_request_count(DFX_PDF) == 1);
    CHECK(remove(path) == 0);
}

int main(void)
{
    char template_path[] = "/tmp/deepdownload-phase9-XXXXXX";
    char path[512];
    unsigned i;
    DownloadResult result = {0};
    DownloadStatus status;
    const size_t exact_size = DOWNLOAD_MAX_BYTES;

    server_port = download_fixture_start();
    if (server_port == 0) {
        puts("SKIP: POSIX local HTTP/download integration (native Windows)");
        return 0;
    }
    if (mkdtemp(template_path) == NULL) {
        perror("mkdtemp");
        download_fixture_stop();
        return 1;
    }
    (void)snprintf(output_directory, sizeof(output_directory), "%s", template_path);

    download_fixture_reset_counts();
    test_cli_demo();

    download_fixture_reset_counts();
    expect_success("/files/small.pdf", "small.pdf");
    CHECK(download_fixture_request_count(DFX_PDF) == 1);
    expect_success("/files/small.pdf?download=1#top", "query.pdf");

    status = fetch("/files/small.pdf", "timeout-one.pdf", 1, 1, &result);
    expected_path(path, sizeof(path), "timeout-one.pdf");
    CHECK(status == DOWNLOAD_OK);
    CHECK(verify_small_file(path));
    remove_result_file(&result);
    status = fetch("/files/small.pdf", "timeout-max.pdf", 1,
                   DOWNLOAD_MAX_TIMEOUT_SECONDS, &result);
    expected_path(path, sizeof(path), "timeout-max.pdf");
    CHECK(status == DOWNLOAD_OK);
    CHECK(verify_small_file(path));
    remove_result_file(&result);

    expect_success("/files/word.docx", "word.docx");
    expect_success("/files/archive.zip", "archive.zip");
    expect_success("/files/archive-alt.zip", "archive-alt.zip");
    expect_success("/files/octet.pdf", "octet.pdf");

    expect_error("/files/html.pdf", "html.pdf", DOWNLOAD_ERROR_CONTENT_TYPE, 3);
    expect_error("/files/wrong.pdf", "wrong.pdf", DOWNLOAD_ERROR_CONTENT_TYPE, 3);
    expect_error("/files/duplicate-type.pdf", "duplicate.pdf",
                 DOWNLOAD_ERROR_CONTENT_TYPE, 3);
    expect_error("/files/no-type.pdf", "no-type.pdf", DOWNLOAD_ERROR_CONTENT_TYPE, 3);
    expect_error("/files/empty.pdf", "empty.pdf", DOWNLOAD_ERROR_EMPTY, 3);
    expect_error("/files/not-found.pdf", "not-found.pdf", DOWNLOAD_ERROR_HTTP_STATUS, 3);
    expect_error("/files/server-error.pdf", "server-error.pdf",
                 DOWNLOAD_ERROR_HTTP_STATUS, 3);
    expect_error("/files/partial-content.pdf", "partial-content.pdf",
                 DOWNLOAD_ERROR_HTTP_STATUS, 3);
    expect_error("/files/partial.pdf", "partial.pdf", DOWNLOAD_ERROR_INCOMPLETE, 3);
    expect_error("/files/drop.pdf", "drop.pdf", DOWNLOAD_ERROR_NETWORK, 3);
    expect_error("/files/slow.pdf", "slow.pdf", DOWNLOAD_ERROR_TIMEOUT, 1);

    test_redirect(DFX_RELATIVE_REDIRECT, "/redir/start.pdf", "relative.pdf");
    test_redirect(DFX_REDIRECT_301, "/r301.pdf", "r301.pdf");
    test_redirect(DFX_REDIRECT_302, "/r302.pdf", "r302.pdf");
    test_redirect(DFX_REDIRECT_303, "/r303.pdf", "r303.pdf");
    test_redirect(DFX_REDIRECT_307, "/r307.pdf", "r307.pdf");
    test_redirect(DFX_REDIRECT_308, "/r308.pdf", "r308.pdf");
    expect_error("/missing-location.pdf", "missing-location.pdf",
                 DOWNLOAD_ERROR_REDIRECT, 3);
    expect_error("/malformed-location.pdf", "malformed-location.pdf",
                 DOWNLOAD_ERROR_REDIRECT, 3);
    expect_error("/ftp.pdf", "ftp.pdf", DOWNLOAD_ERROR_REDIRECT, 3);
    expect_error("/file.pdf", "file.pdf", DOWNLOAD_ERROR_REDIRECT, 3);
    expect_error("/redirect-404.pdf", "redirect-404.pdf",
                 DOWNLOAD_ERROR_HTTP_STATUS, 3);
    expect_error("/redirect-wrong.pdf", "redirect-wrong.pdf",
                 DOWNLOAD_ERROR_CONTENT_TYPE, 3);

    download_fixture_reset_counts();
    expect_error("/loop-a.pdf", "loop.pdf", DOWNLOAD_ERROR_REDIRECT, 3);
    CHECK(download_fixture_request_count(DFX_LOOP_A) == 1);
    CHECK(download_fixture_request_count(DFX_LOOP_B) == 1);

    download_fixture_reset_counts();
    expect_error("/hop/0.pdf", "too-many-hops.pdf", DOWNLOAD_ERROR_REDIRECT, 3);
    for (i = 0; i <= 5; ++i) {
        CHECK(download_fixture_request_count(DFX_HOP_BASE + i) == 1);
    }
    CHECK(download_fixture_request_count(DFX_HOP_BASE + 6) == 0);

    download_fixture_reset_counts();
    status = fetch("/off-host.pdf", "off-host.pdf", 1, 3, &result);
    CHECK(status == DOWNLOAD_ERROR_SCOPE);
    CHECK(download_fixture_request_count(DFX_OFF_HOST_REDIRECT) == 1);
    CHECK(download_fixture_request_count(DFX_HOST_TARGET) == 0);
    download_result_free(&result);

    download_fixture_reset_counts();
    status = fetch("/off-port.pdf", "off-port.pdf", 0, 3, &result);
    CHECK(status == DOWNLOAD_ERROR_SCOPE);
    CHECK(download_fixture_request_count(DFX_OFF_PORT_REDIRECT) == 1);
    CHECK(download_fixture_request_count(DFX_PORT_TARGET) == 0);
    download_result_free(&result);

    download_fixture_reset_counts();
    status = fetch("/localhost-redirect.pdf", "same-port-host.pdf", 0, 3, &result);
    expected_path(path, sizeof(path), "same-port-host.pdf");
    CHECK(status == DOWNLOAD_OK);
    CHECK(verify_small_file(path));
    CHECK(download_fixture_request_count(DFX_HOST_TARGET) == 1);
    remove_result_file(&result);

    test_size_limit("/files/exact.pdf", "exact.pdf", exact_size, DOWNLOAD_OK);
    test_size_limit("/files/exact-chunked.pdf", "exact-chunked.pdf",
                    exact_size, DOWNLOAD_OK);
    test_size_limit("/files/oversize.pdf", "oversize.pdf",
                    exact_size + 1, DOWNLOAD_ERROR_TOO_LARGE);
    test_size_limit("/files/oversize-chunked.pdf", "oversize-chunked.pdf",
                    exact_size + 1, DOWNLOAD_ERROR_TOO_LARGE);

    expected_path(path, sizeof(path), "existing.pdf");
    {
        FILE *existing = fopen(path, "wb");
        static const char marker[] = "keep";
        CHECK(existing != NULL);
        if (existing != NULL) {
            CHECK(fwrite(marker, 1, sizeof(marker) - 1, existing) == sizeof(marker) - 1);
            CHECK(fclose(existing) == 0);
        }
    }
    download_fixture_reset_counts();
    expect_error("/files/small.pdf", "existing.pdf", DOWNLOAD_ERROR_EXISTS, 3);
    CHECK(download_fixture_request_count(DFX_PDF) == 0);
    {
        FILE *existing = fopen(path, "rb");
        char marker[5] = {0};
        CHECK(existing != NULL);
        if (existing != NULL) {
            CHECK(fread(marker, 1, 4, existing) == 4);
            CHECK(fclose(existing) == 0);
            CHECK(strcmp(marker, "keep") == 0);
        }
    }
    CHECK(remove(path) == 0);

    CHECK(rmdir(output_directory) == 0);
    download_fixture_stop();
    http_cleanup();
    printf("download integration: %u checks, %u failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
#else
#include <stdio.h>
int main(void)
{
    puts("SKIP: POSIX local HTTP/download integration (native Windows)");
    return 0;
}
#endif
