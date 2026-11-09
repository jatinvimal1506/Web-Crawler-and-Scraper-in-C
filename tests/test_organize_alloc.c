#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#include "organize.h"
#include "sha256.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define TRACKED_POINTERS 256U
#define INPUT_PATH "build/phase10-allocation-input.bin"

static void *tracked[TRACKED_POINTERS];
static size_t live_allocations;
static size_t allocation_calls;
static size_t fail_allocation_at;
static size_t read_calls;
static size_t fail_read_at;
static size_t close_calls;
static size_t fail_close_at;
static int fail_next_write;
static int fail_next_remove;
static unsigned checks;
static unsigned failures;

#define CHECK(expression) do { \
    ++checks; \
    if (!(expression)) { \
        ++failures; \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expression); \
    } \
} while (0)

static size_t pointer_slot(void *pointer)
{
    size_t index;
    for (index = 0; index < TRACKED_POINTERS; ++index) {
        if (tracked[index] == pointer) { return index; }
    }
    return TRACKED_POINTERS;
}

static void track_pointer(void *pointer)
{
    size_t index;
    if (pointer == NULL) { return; }
    for (index = 0; index < TRACKED_POINTERS; ++index) {
        if (tracked[index] == NULL) {
            tracked[index] = pointer;
            ++live_allocations;
            return;
        }
    }
    fprintf(stderr, "allocation tracker table exhausted\n");
    abort();
}

void *organize_test_malloc(size_t size)
{
    void *pointer;
    ++allocation_calls;
    if (fail_allocation_at != 0 && allocation_calls == fail_allocation_at) {
        return NULL;
    }
    pointer = malloc(size);
    track_pointer(pointer);
    return pointer;
}

void organize_test_free(void *pointer)
{
    size_t slot;
    if (pointer == NULL) { return; }
    slot = pointer_slot(pointer);
    if (slot == TRACKED_POINTERS) {
        fprintf(stderr, "unexpected untracked free in organizer test\n");
        abort();
    }
    tracked[slot] = NULL;
    --live_allocations;
    free(pointer);
}

size_t organize_test_fread(void *buffer, size_t size, size_t count, FILE *file)
{
    ++read_calls;
    if (fail_read_at != 0 && read_calls == fail_read_at) {
        fail_read_at = 0;
        errno = EIO;
        return 0;
    }
    return fread(buffer, size, count, file);
}

size_t organize_test_fwrite(const void *buffer, size_t size, size_t count,
                            FILE *file)
{
    if (fail_next_write) {
        fail_next_write = 0;
        errno = ENOSPC;
        return 0;
    }
    return fwrite(buffer, size, count, file);
}

int organize_test_fclose(FILE *file)
{
    int result = fclose(file);
    ++close_calls;
    if (fail_close_at != 0 && close_calls == fail_close_at) {
        errno = EIO;
        return EOF;
    }
    return result;
}

int organize_test_remove(const char *path)
{
    if (fail_next_remove) {
        fail_next_remove = 0;
        errno = EACCES;
        return -1;
    }
    return remove(path);
}

static int write_input(void)
{
    static const unsigned char data[] = {0x00U, 0x01U, 0x80U, 0xffU, 'A', 0x00U};
    FILE *file = fopen(INPUT_PATH, "wb");
    int ok;
    if (file == NULL) { return 0; }
    ok = fwrite(data, 1, sizeof(data), file) == sizeof(data);
    if (fclose(file) != 0) { ok = 0; }
    return ok;
}

static int input_unchanged(void)
{
    static const unsigned char expected[] = {0x00U, 0x01U, 0x80U, 0xffU, 'A', 0x00U};
    unsigned char actual[sizeof(expected)];
    FILE *file = fopen(INPUT_PATH, "rb");
    int ok;
    if (file == NULL) { return 0; }
    ok = fread(actual, 1, sizeof(actual), file) == sizeof(actual) &&
         memcmp(actual, expected, sizeof(expected)) == 0 &&
         fgetc(file) == EOF;
    if (fclose(file) != 0) { ok = 0; }
    return ok;
}

static int make_root(char path[256])
{
    (void)snprintf(path, 256, "build/phase10-alloc-XXXXXX");
    return mkdtemp(path) != NULL;
}

static void remove_root_files(const char *root, const char *digest)
{
    static const char *const folders[] = {"pdf", "docx", "zip"};
    static const char *const extensions[] = {".pdf", ".docx", ".zip"};
    char folder[512], filename[80], path[768];
    size_t i, j;
    for (j = 0; j < 3U; ++j) {
        (void)snprintf(filename, sizeof(filename), "%s%s", digest, extensions[j]);
        for (i = 0; i < 3U; ++i) {
            (void)snprintf(folder, sizeof(folder), "%s/%s", root, folders[i]);
            (void)snprintf(path, sizeof(path), "%s/%s", folder, filename);
            (void)remove(path);
        }
    }
    for (i = 0; i < 3U; ++i) {
        (void)snprintf(folder, sizeof(folder), "%s/%s", root, folders[i]);
        (void)rmdir(folder);
    }
    (void)rmdir(root);
}

static int output_exists(const char *root, const char *digest)
{
    char path[768];
    (void)snprintf(path, sizeof(path), "%s/pdf/%s.pdf", root, digest);
    return access(path, F_OK) == 0;
}

static void test_allocation_failures(const char *digest, size_t total_calls)
{
    size_t position;
    for (position = 1; position <= total_calls; ++position) {
        char root[256];
        OrganizeResult result = {0};
        OrganizerStatus status;
        CHECK(make_root(root));
        CHECK(organizer_prepare_directories(root) == ORGANIZER_OK);
        CHECK(live_allocations == 0);
        allocation_calls = 0;
        fail_allocation_at = position;
        status = organize_file(INPUT_PATH, DOCUMENT_TYPE_PDF, root, &result);
        fail_allocation_at = 0;
        CHECK(status == ORGANIZER_ERROR_OUT_OF_MEMORY);
        CHECK(result.status == ORGANIZER_ERROR_OUT_OF_MEMORY);
        CHECK(result.stored_path == NULL);
        CHECK(!output_exists(root, digest));
        CHECK(input_unchanged());
        organize_result_free(&result);
        CHECK(live_allocations == 0);

        CHECK(organize_file(INPUT_PATH, DOCUMENT_TYPE_PDF, root, &result) ==
              ORGANIZER_NEW_FILE);
        CHECK(output_exists(root, digest));
        organize_result_free(&result);
        CHECK(live_allocations == 0);
        remove_root_files(root, digest);
    }
}

static void test_io_failures(const char *digest)
{
    char root[256];
    OrganizeResult result = {0};

    CHECK(make_root(root));
    CHECK(organizer_prepare_directories(root) == ORGANIZER_OK);
    read_calls = 0;
    fail_read_at = 1;
    CHECK(organize_file(INPUT_PATH, DOCUMENT_TYPE_PDF, root, &result) ==
          ORGANIZER_ERROR_READ);
    CHECK(!output_exists(root, digest));
    CHECK(input_unchanged());
    organize_result_free(&result);
    CHECK(live_allocations == 0);

    read_calls = 0;
    fail_read_at = 3U; /* First read in the copy after the input has been hashed. */
    CHECK(organize_file(INPUT_PATH, DOCUMENT_TYPE_PDF, root, &result) ==
          ORGANIZER_ERROR_READ);
    fail_read_at = 0;
    CHECK(!output_exists(root, digest));
    CHECK(input_unchanged());
    organize_result_free(&result);
    CHECK(live_allocations == 0);

    CHECK(organize_file(INPUT_PATH, DOCUMENT_TYPE_PDF, root, &result) ==
          ORGANIZER_NEW_FILE);
    organize_result_free(&result);
    remove_root_files(root, digest);
}

int main(void)
{
    char digest[65];
    size_t bytes = 0, total_calls, close_position;
    char baseline_root[256];
    OrganizeResult baseline = {0}, result = {0};

    CHECK(write_input());
    CHECK(sha256_file(INPUT_PATH, digest, &bytes) == SHA256_OK);
    CHECK(bytes == 6U);
    CHECK(make_root(baseline_root));
    CHECK(organizer_prepare_directories(baseline_root) == ORGANIZER_OK);
    allocation_calls = 0;
    CHECK(organize_file(INPUT_PATH, DOCUMENT_TYPE_PDF, baseline_root,
                        &baseline) == ORGANIZER_NEW_FILE);
    total_calls = allocation_calls;
    organize_result_free(&baseline);
    CHECK(live_allocations == 0);
    remove_root_files(baseline_root, digest);
    CHECK(total_calls > 0U);

    test_allocation_failures(digest, total_calls);
    test_io_failures(digest);

    CHECK(make_root(baseline_root));
    CHECK(organizer_prepare_directories(baseline_root) == ORGANIZER_OK);
    fail_next_write = 1;
    CHECK(organize_file(INPUT_PATH, DOCUMENT_TYPE_PDF, baseline_root, &result) ==
          ORGANIZER_ERROR_WRITE);
    CHECK(!output_exists(baseline_root, digest));
    CHECK(input_unchanged());
    organize_result_free(&result);
    CHECK(live_allocations == 0);
    CHECK(organize_file(INPUT_PATH, DOCUMENT_TYPE_PDF, baseline_root, &result) ==
          ORGANIZER_NEW_FILE);
    organize_result_free(&result);
    remove_root_files(baseline_root, digest);

    for (close_position = 1U; close_position <= 4U; ++close_position) {
        CHECK(make_root(baseline_root));
        CHECK(organizer_prepare_directories(baseline_root) == ORGANIZER_OK);
        close_calls = 0;
        fail_close_at = close_position;
        CHECK(organize_file(INPUT_PATH, DOCUMENT_TYPE_PDF, baseline_root,
                            &result) == ORGANIZER_ERROR_CLOSE);
        fail_close_at = 0;
        CHECK(!output_exists(baseline_root, digest));
        CHECK(input_unchanged());
        organize_result_free(&result);
        CHECK(live_allocations == 0);
        CHECK(organize_file(INPUT_PATH, DOCUMENT_TYPE_PDF, baseline_root,
                            &result) == ORGANIZER_NEW_FILE);
        organize_result_free(&result);
        remove_root_files(baseline_root, digest);
    }

    CHECK(make_root(baseline_root));
    CHECK(organizer_prepare_directories(baseline_root) == ORGANIZER_OK);
    fail_next_write = 1;
    fail_next_remove = 1;
    CHECK(organize_file(INPUT_PATH, DOCUMENT_TYPE_PDF, baseline_root, &result) ==
          ORGANIZER_ERROR_CLEANUP);
    CHECK(result.stored_path != NULL);
    CHECK(output_exists(baseline_root, digest));
    CHECK(input_unchanged());
    organize_result_free(&result);
    CHECK(live_allocations == 0);
    /* The injected remove failure is consumed; now the test owns cleanup. */
    remove_root_files(baseline_root, digest);
    CHECK(mkdir(baseline_root, 0700) == 0);
    CHECK(organizer_prepare_directories(baseline_root) == ORGANIZER_OK);
    CHECK(organize_file(INPUT_PATH, DOCUMENT_TYPE_PDF, baseline_root, &result) ==
          ORGANIZER_NEW_FILE);
    organize_result_free(&result);
    remove_root_files(baseline_root, digest);

    CHECK(remove(INPUT_PATH) == 0);
    CHECK(live_allocations == 0);
    printf("organize allocation tests: %u checks, %u failures; %zu allocation positions\n",
           checks, failures, total_calls);
    return failures == 0 ? 0 : 1;
}
#else
#include <stdio.h>
int main(void)
{
    puts("SKIP: Phase 10 allocation fault tests require POSIX mkdtemp");
    return 0;
}
#endif
