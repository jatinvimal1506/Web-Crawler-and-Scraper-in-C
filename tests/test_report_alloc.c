#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#include "report.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAX_TRACKED 128U

static void *tracked[MAX_TRACKED];
static size_t live_allocations;
static size_t allocation_calls;
static size_t fail_allocation_at;
static size_t write_calls;
static size_t fail_write_at;
static size_t close_calls;
static size_t fail_close_at;
static int fail_remove;
static unsigned checks;
static unsigned failures;

#define CHECK(expression) do { \
    ++checks; \
    if (!(expression)) { \
        ++failures; \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expression); \
    } \
} while (0)

void *report_test_malloc(size_t size)
{
    size_t index;
    void *pointer;
    ++allocation_calls;
    if (fail_allocation_at != 0U &&
        allocation_calls == fail_allocation_at) {
        return NULL;
    }
    pointer = malloc(size);
    if (pointer == NULL) { return NULL; }
    for (index = 0; index < MAX_TRACKED; ++index) {
        if (tracked[index] == NULL) {
            tracked[index] = pointer;
            ++live_allocations;
            return pointer;
        }
    }
    free(pointer);
    return NULL;
}

void report_test_free(void *pointer)
{
    size_t index;
    if (pointer == NULL) { return; }
    for (index = 0; index < MAX_TRACKED; ++index) {
        if (tracked[index] == pointer) {
            tracked[index] = NULL;
            --live_allocations;
            break;
        }
    }
    free(pointer);
}

size_t report_test_fwrite(const void *buffer, size_t size, size_t count,
                          FILE *file)
{
    ++write_calls;
    if (fail_write_at != 0U && write_calls == fail_write_at) {
        errno = EIO;
        return 0U;
    }
    return fwrite(buffer, size, count, file);
}

int report_test_fclose(FILE *file)
{
    int result;
    ++close_calls;
    result = fclose(file);
    if (fail_close_at != 0U && close_calls == fail_close_at) {
        return EOF;
    }
    return result;
}

int report_test_remove(const char *path)
{
    if (fail_remove) {
        fail_remove = 0;
        errno = EACCES;
        return -1;
    }
    return remove(path);
}

static int make_path(char *output, size_t capacity,
                     const char *root, const char *name)
{
    int written = snprintf(output, capacity, "%s/%s", root, name);
    return written >= 0 && (size_t)written < capacity;
}

static int file_exists(const char *path)
{
    struct stat info;
    return stat(path, &info) == 0;
}

static ReportRow sample_row(void)
{
    ReportRow row = {
        "https://example.test/resource.pdf",
        DOCUMENT_TYPE_PDF,
        "https://example.test/source",
        1U,
        REPORT_ROW_DOWNLOAD_FAILED,
        "",
        0U,
        "",
        "controlled failure"
    };
    return row;
}

static void clear_result(ReportResult *result)
{
    report_result_free(result);
    CHECK(result->output_path == NULL);
    CHECK(result->status == REPORT_OK);
}

int main(void)
{
    ReportRow row = sample_row();
    ReportFilter filter = {NULL, 0U};
    ReportResult result = {0};
    char root_template[] = "build/report-alloc-XXXXXX";
    char root[128], path[512];
    size_t baseline_allocations, baseline_writes, position;

    CHECK(mkdtemp(root_template) != NULL);
    (void)snprintf(root, sizeof(root), "%s", root_template);
    CHECK(make_path(path, sizeof(path), root, "baseline.csv"));
    allocation_calls = write_calls = close_calls = 0U;
    CHECK(report_write_csv(root, "baseline.csv", &row, 1U, &filter, &result) ==
          REPORT_OK);
    baseline_allocations = allocation_calls;
    baseline_writes = write_calls;
    CHECK(baseline_allocations > 0U && baseline_writes > 0U);
    clear_result(&result);
    CHECK(live_allocations == 0U);
    CHECK(remove(path) == 0);

    for (position = 1U; position <= baseline_allocations; ++position) {
        CHECK(make_path(path, sizeof(path), root, "alloc.csv"));
        allocation_calls = 0U;
        fail_allocation_at = position;
        CHECK(report_write_csv(root, "alloc.csv", &row, 1U, &filter, &result) ==
              REPORT_ERROR_OUT_OF_MEMORY);
        fail_allocation_at = 0U;
        CHECK(result.output_path == NULL);
        CHECK(!file_exists(path));
        clear_result(&result);
        CHECK(live_allocations == 0U);
    }

    for (position = 1U; position <= baseline_writes; position += baseline_writes - 1U) {
        CHECK(make_path(path, sizeof(path), root, "write.csv"));
        write_calls = close_calls = 0U;
        fail_write_at = position;
        CHECK(report_write_csv(root, "write.csv", &row, 1U, &filter, &result) ==
              REPORT_ERROR_WRITE);
        fail_write_at = 0U;
        CHECK(!file_exists(path));
        clear_result(&result);
        CHECK(live_allocations == 0U);
        if (position == baseline_writes) { break; }
    }

    CHECK(make_path(path, sizeof(path), root, "close.csv"));
    close_calls = 0U;
    fail_close_at = 1U;
    CHECK(report_write_csv(root, "close.csv", &row, 1U, &filter, &result) ==
          REPORT_ERROR_CLOSE);
    fail_close_at = 0U;
    CHECK(!file_exists(path));
    clear_result(&result);
    CHECK(live_allocations == 0U);

    CHECK(make_path(path, sizeof(path), root, "cleanup.csv"));
    write_calls = close_calls = 0U;
    fail_write_at = 1U;
    fail_remove = 1;
    CHECK(report_write_csv(root, "cleanup.csv", &row, 1U, &filter, &result) ==
          REPORT_ERROR_CLEANUP);
    fail_write_at = 0U;
    CHECK(result.output_path != NULL);
    CHECK(strcmp(result.output_path, path) == 0);
    CHECK(file_exists(path));
    clear_result(&result);
    CHECK(live_allocations == 0U);
    CHECK(remove(path) == 0);
    CHECK(report_write_csv(root, "cleanup.csv", &row, 1U, &filter, &result) ==
          REPORT_OK);
    clear_result(&result);
    CHECK(remove(path) == 0);
    CHECK(live_allocations == 0U);

    CHECK(rmdir(root) == 0);
    printf("report allocation tests: %u checks, %u failures; "
           "%zu allocation position(s), %zu write positions\n",
           checks, failures, baseline_allocations, baseline_writes);
    return failures == 0 ? 0 : 1;
}
#else
#include <stdio.h>
int main(void)
{
    puts("SKIP: Phase 11 allocation tests require POSIX mkdtemp");
    return 0;
}
#endif
