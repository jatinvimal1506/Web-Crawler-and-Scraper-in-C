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
#include <unistd.h>

#define TRACKED_POINTERS 4096

static void *tracked[TRACKED_POINTERS];
static size_t live_pointers;
static size_t allocation_calls;
static size_t fail_allocation_at;
static int fail_next_write;
static int fail_next_close;
static int fail_next_remove;
static unsigned checks;
static unsigned failures;
static char output_directory[128];

#define CHECK(expression) do { \
    ++checks; \
    if (!(expression)) { \
        ++failures; \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expression); \
    } \
} while (0)

static size_t find_pointer(void *pointer)
{
    size_t i;
    for (i = 0; i < TRACKED_POINTERS; ++i) {
        if (tracked[i] == pointer) { return i; }
    }
    return TRACKED_POINTERS;
}

static void register_pointer(void *pointer)
{
    size_t i;
    if (pointer == NULL) { return; }
    for (i = 0; i < TRACKED_POINTERS; ++i) {
        if (tracked[i] == NULL) {
            tracked[i] = pointer;
            ++live_pointers;
            return;
        }
    }
    fprintf(stderr, "allocator tracker table exhausted\n");
    abort();
}

void *download_test_malloc(size_t size)
{
    void *pointer;
    ++allocation_calls;
    if (fail_allocation_at != 0 && allocation_calls == fail_allocation_at) {
        return NULL;
    }
    pointer = malloc(size);
    register_pointer(pointer);
    return pointer;
}

void *download_test_realloc(void *old_pointer, size_t size)
{
    size_t slot = old_pointer != NULL ? find_pointer(old_pointer) : TRACKED_POINTERS;
    void *pointer;
    ++allocation_calls;
    if (fail_allocation_at != 0 && allocation_calls == fail_allocation_at) {
        return NULL;
    }
    pointer = realloc(old_pointer, size);
    if (pointer == NULL) { return NULL; }
    if (old_pointer == NULL) {
        register_pointer(pointer);
    } else if (slot < TRACKED_POINTERS) {
        tracked[slot] = pointer;
    }
    return pointer;
}

void download_test_free(void *pointer)
{
    size_t slot;
    if (pointer == NULL) { return; }
    slot = find_pointer(pointer);
    if (slot < TRACKED_POINTERS) {
        tracked[slot] = NULL;
        --live_pointers;
    }
    free(pointer);
}

size_t download_test_fwrite(const void *buffer, size_t size, size_t count,
                            FILE *file)
{
    if (fail_next_write) {
        fail_next_write = 0;
        errno = ENOSPC;
        return 0;
    }
    return fwrite(buffer, size, count, file);
}

int download_test_fclose(FILE *file)
{
    int result = fclose(file);
    if (fail_next_close) {
        fail_next_close = 0;
        errno = EIO;
        return EOF;
    }
    return result;
}

int download_test_remove(const char *path)
{
    if (fail_next_remove) {
        fail_next_remove = 0;
        errno = EACCES;
        return -1;
    }
    return remove(path);
}

static DownloadStatus run_one(unsigned port, const char *filename,
                              DownloadResult *result)
{
    char document_url[256], seed_url[256];
    DownloadConfig config;
    (void)snprintf(document_url, sizeof(document_url),
                   "http://127.0.0.1:%u/files/small.pdf", port);
    (void)snprintf(seed_url, sizeof(seed_url),
                   "http://127.0.0.1:%u/seed.pdf", port);
    config.document_url = document_url;
    config.seed_url = seed_url;
    config.same_domain = 1;
    config.timeout_seconds = 3;
    config.output_directory = output_directory;
    config.filename = filename;
    return download_document(&config, result);
}

static void expected_path(char *path, size_t capacity, const char *filename)
{
    int written = snprintf(path, capacity, "%s/%s", output_directory, filename);
    CHECK(written > 0 && (size_t)written < capacity);
}

int main(void)
{
    char template_path[] = "/tmp/deepdownload-alloc-XXXXXX";
    char path[512];
    unsigned port = download_fixture_start();
    DownloadResult result = {0};
    DownloadStatus status;
    size_t expected_calls, i;

    if (port == 0) {
        puts("SKIP: POSIX allocation-fault download tests (native Windows)");
        return 0;
    }
    if (mkdtemp(template_path) == NULL) {
        perror("mkdtemp");
        download_fixture_stop();
        return 1;
    }
    (void)snprintf(output_directory, sizeof(output_directory), "%s", template_path);
    expected_path(path, sizeof(path), "baseline.pdf");

    status = run_one(port, "baseline.pdf", &result);
    CHECK(status == DOWNLOAD_OK);
    CHECK(live_pointers > 0);
    CHECK(remove(path) == 0);
    download_result_free(&result);
    CHECK(live_pointers == 0);
    expected_calls = allocation_calls;
    CHECK(expected_calls > 0);

    for (i = 1; i <= expected_calls; ++i) {
        allocation_calls = 0;
        fail_allocation_at = i;
        status = run_one(port, "injected.pdf", &result);
        fail_allocation_at = 0;
        expected_path(path, sizeof(path), "injected.pdf");
        CHECK(status == DOWNLOAD_ERROR_OUT_OF_MEMORY);
        CHECK(result.status == DOWNLOAD_ERROR_OUT_OF_MEMORY);
        CHECK(result.file_path == NULL);
        CHECK(access(path, F_OK) != 0);
        download_result_free(&result);
        CHECK(live_pointers == 0);
    }

    allocation_calls = 0;
    fail_next_write = 1;
    status = run_one(port, "write-failure.pdf", &result);
    expected_path(path, sizeof(path), "write-failure.pdf");
    CHECK(status == DOWNLOAD_ERROR_WRITE);
    CHECK(result.file_path == NULL);
    CHECK(access(path, F_OK) != 0);
    download_result_free(&result);
    CHECK(live_pointers == 0);

    status = run_one(port, "write-failure.pdf", &result);
    CHECK(status == DOWNLOAD_OK);
    CHECK(result.file_path != NULL);
    CHECK(remove(path) == 0);
    download_result_free(&result);
    CHECK(live_pointers == 0);

    fail_next_close = 1;
    status = run_one(port, "close-failure.pdf", &result);
    expected_path(path, sizeof(path), "close-failure.pdf");
    CHECK(status == DOWNLOAD_ERROR_FILE);
    CHECK(result.file_path == NULL);
    CHECK(access(path, F_OK) != 0);
    download_result_free(&result);
    CHECK(live_pointers == 0);

    fail_next_write = 1;
    fail_next_remove = 1;
    status = run_one(port, "remove-failure.pdf", &result);
    expected_path(path, sizeof(path), "remove-failure.pdf");
    CHECK(status == DOWNLOAD_ERROR_CLEANUP);
    CHECK(result.file_path != NULL);
    CHECK(access(path, F_OK) == 0);
    download_result_free(&result);
    CHECK(remove(path) == 0);
    CHECK(live_pointers == 0);

    CHECK(rmdir(output_directory) == 0);
    download_fixture_stop();
    http_cleanup();
    printf("download allocation tests: %u checks, %u failures; "
           "%zu allocation failure positions\n", checks, failures, expected_calls);
    return failures == 0 ? 0 : 1;
}
#else
#include <stdio.h>
#include <stdlib.h>
void *download_test_malloc(size_t size) { return malloc(size); }
void *download_test_realloc(void *pointer, size_t size)
{
    return realloc(pointer, size);
}
void download_test_free(void *pointer) { free(pointer); }
size_t download_test_fwrite(const void *data, size_t size, size_t count,
                            FILE *file)
{
    return fwrite(data, size, count, file);
}
int download_test_fclose(FILE *file) { return fclose(file); }
int download_test_remove(const char *path) { return remove(path); }
int main(void)
{
    puts("SKIP: POSIX allocation-fault download tests (native Windows)");
    return 0;
}
#endif
