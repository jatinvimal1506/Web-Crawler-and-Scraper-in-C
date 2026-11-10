#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
#include "download_fixture.h"
#include "http.h"
#include "pipeline.h"
#include "report.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static size_t allocation_calls;
static size_t fail_at;
static size_t live_count;
static size_t tracked_count;
static int fail_remove_once;
static ReportStatus injected_report_status = REPORT_OK;
static struct {
    void *pointer;
    int live;
} tracked[2048];
static unsigned checks;
static unsigned failures;

#define CHECK(expression) do { \
    ++checks; \
    if (!(expression)) { \
        ++failures; \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expression); \
    } \
} while (0)

void *pipeline_test_malloc(size_t size)
{
    void *pointer;
    ++allocation_calls;
    if (fail_at != 0U && allocation_calls == fail_at) { return NULL; }
    pointer = malloc(size);
    if (pointer != NULL) {
        if (tracked_count >= sizeof(tracked) / sizeof(tracked[0])) {
            free(pointer);
            return NULL;
        }
        tracked[tracked_count].pointer = pointer;
        tracked[tracked_count].live = 1;
        ++tracked_count;
        ++live_count;
    }
    return pointer;
}

void pipeline_test_free(void *pointer)
{
    size_t index;
    if (pointer == NULL) { return; }
    for (index = tracked_count; index != 0U; --index) {
        if (tracked[index - 1U].pointer == pointer &&
            tracked[index - 1U].live) {
            tracked[index - 1U].live = 0;
            --live_count;
            free(pointer);
            return;
        }
    }
    /* PipelineResult can own strings created by the other library modules. */
    free(pointer);
}

int pipeline_test_remove(const char *path)
{
    if (fail_remove_once) {
        fail_remove_once = 0;
        errno = EACCES;
        return -1;
    }
    return remove(path);
}

ReportStatus pipeline_test_report_write_csv(const char *directory,
                                            const char *filename,
                                            const ReportRow *rows,
                                            size_t row_count,
                                            const ReportFilter *filter,
                                            ReportResult *result)
{
    if (injected_report_status != REPORT_OK) {
        ReportStatus status = injected_report_status;
        injected_report_status = REPORT_OK;
        return status;
    }
    return report_write_csv(directory, filename, rows, row_count, filter,
                            result);
}

#ifdef _WIN32
int main(void)
{
    puts("pipeline allocation tests: SKIP (local POSIX fixture is unavailable on Windows)");
    return 0;
}
#else
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

static char test_root[256];
static unsigned output_number;

static int make_output(char *path, size_t capacity)
{
    int written;
    ++output_number;
    written = snprintf(path, capacity, "%s/out-%03u", test_root, output_number);
    return written > 0 && (size_t)written < capacity &&
           mkdir(path, 0700) == 0;
}

static void clear_tracking(void)
{
    size_t index;
    CHECK(live_count == 0U);
    for (index = 0U; index < tracked_count; ++index) {
        tracked[index].pointer = NULL;
        tracked[index].live = 0;
    }
    tracked_count = 0U;
    allocation_calls = 0U;
}

static void remove_tree(const char *path)
{
    struct stat info;
    if (lstat(path, &info) != 0) { return; }
    if (!S_ISDIR(info.st_mode)) {
        (void)unlink(path);
        return;
    }
    {
        DIR *directory = opendir(path);
        struct dirent *entry;
        if (directory == NULL) { return; }
        while ((entry = readdir(directory)) != NULL) {
            char child[1024];
            int written;
            if (strcmp(entry->d_name, ".") == 0 ||
                strcmp(entry->d_name, "..") == 0) {
                continue;
            }
            written = snprintf(child, sizeof(child), "%s/%s",
                               path, entry->d_name);
            if (written > 0 && (size_t)written < sizeof(child)) {
                remove_tree(child);
            }
        }
        (void)closedir(directory);
    }
    (void)rmdir(path);
}

static PipelineStatus run_one_named(unsigned port, const char *output,
                                    const char *report_name,
                                    PipelineResult *result)
{
    char url[256];
    PipelineConfig config = {0};
    int written = snprintf(url, sizeof(url), "http://127.0.0.1:%u/staging-site",
                           port);
    if (written <= 0 || (size_t)written >= sizeof(url)) {
        return PIPELINE_ERROR_INTERNAL;
    }
    config.start_url = url;
    config.output_root = output;
    config.report_filename = report_name;
    config.max_depth = 0U;
    config.same_domain = 1;
    config.timeout_seconds = 3L;
    (void)pipeline_result_init(result);
    return pipeline_run(&config, result);
}

static PipelineStatus run_one(unsigned port, const char *output,
                              PipelineResult *result)
{
    return run_one_named(port, output, "report.csv", result);
}

static void test_report_failure_retry(unsigned port,
                                      ReportStatus injected_status)
{
    char output[512];
    char report_path[1024];
    PipelineResult result = {0};
    PipelineStatus status;
    int written;

    CHECK(make_output(output, sizeof(output)));
    clear_tracking();
    fail_at = 0U;
    injected_report_status = injected_status;
    download_fixture_reset_counts();
    status = run_one(port, output, &result);
    CHECK(status == PIPELINE_ERROR_REPORT);
    CHECK(result.status == PIPELINE_ERROR_REPORT);
    CHECK(injected_report_status == REPORT_OK);
    CHECK(download_fixture_request_total() > 0U);
    CHECK(result.stats.newly_stored_documents == 1U);
    CHECK(result.orphaned_path_count == 0U);
    CHECK(result.report_path == NULL);
    written = snprintf(report_path, sizeof(report_path), "%s/report.csv",
                       output);
    CHECK(written > 0 && (size_t)written < sizeof(report_path));
    if (written > 0 && (size_t)written < sizeof(report_path)) {
        CHECK(access(report_path, F_OK) != 0);
    }
    pipeline_result_free(&result);
    CHECK(live_count == 0U);

    clear_tracking();
    fail_at = 0U;
    download_fixture_reset_counts();
    status = run_one_named(port, output, "report-retry.csv", &result);
    CHECK(status == PIPELINE_OK);
    CHECK(result.stats.duplicate_content_documents == 1U);
    CHECK(result.report_path != NULL);
    if (result.report_path != NULL) {
        CHECK(strstr(result.report_path, "/report-retry.csv") != NULL);
        CHECK(access(result.report_path, F_OK) == 0);
    }
    pipeline_result_free(&result);
    CHECK(live_count == 0U);
}

int main(void)
{
    char template_path[] = "/tmp/deepdownload-pipeline-alloc-XXXXXX";
    char output[512];
    char staging_collision[768];
    char orphan_path[1024], retry_report[1024];
    PipelineResult result = {0};
    PipelineStatus status;
    unsigned port;
    size_t successful_allocations, fail_position;
    FILE *collision_file;
    int written;

    if (mkdtemp(template_path) == NULL) {
        perror("mkdtemp");
        return 2;
    }
    (void)snprintf(test_root, sizeof(test_root), "%s", template_path);
    port = download_fixture_start();
    CHECK(port != 0U);

    CHECK(make_output(output, sizeof(output)));
    written = snprintf(staging_collision, sizeof(staging_collision),
                       "%s/staging", output);
    CHECK(written > 0 && (size_t)written < sizeof(staging_collision));
    collision_file = fopen(staging_collision, "wb");
    CHECK(collision_file != NULL);
    if (collision_file != NULL) {
        CHECK(fclose(collision_file) == 0);
        clear_tracking();
        fail_at = 0U;
        download_fixture_reset_counts();
        status = run_one(port, output, &result);
        CHECK(status == PIPELINE_ERROR_FILESYSTEM);
        CHECK(download_fixture_request_total() == 0U);
        pipeline_result_free(&result);
        CHECK(live_count == 0U);
        CHECK(unlink(staging_collision) == 0);
    }

    CHECK(make_output(output, sizeof(output)));
    clear_tracking();
    fail_at = 0U;
    fail_remove_once = 1;
    download_fixture_reset_counts();
    status = run_one(port, output, &result);
    CHECK(status == PIPELINE_ERROR_FILESYSTEM);
    CHECK(fail_remove_once == 0);
    CHECK(result.status == PIPELINE_ERROR_FILESYSTEM);
    CHECK(result.stats.newly_stored_documents == 1U);
    CHECK(result.orphaned_path_count == 1U);
    if (result.orphaned_path_count == 1U &&
        result.orphaned_paths[0] != NULL) {
        written = snprintf(orphan_path, sizeof(orphan_path), "%s",
                           result.orphaned_paths[0]);
        CHECK(written > 0 && (size_t)written < sizeof(orphan_path));
        if (written > 0 && (size_t)written < sizeof(orphan_path)) {
            CHECK(access(orphan_path, F_OK) == 0);
            pipeline_result_free(&result);
            CHECK(live_count == 0U);
            CHECK(unlink(orphan_path) == 0);
        } else {
            pipeline_result_free(&result);
        }
    } else {
        CHECK(0);
        pipeline_result_free(&result);
    }
    CHECK(live_count == 0U);
    written = snprintf(retry_report, sizeof(retry_report), "%s/%s",
                       output, "report-retry.csv");
    CHECK(written > 0 && (size_t)written < sizeof(retry_report));
    clear_tracking();
    fail_at = 0U;
    download_fixture_reset_counts();
    status = run_one_named(port, output, "report-retry.csv", &result);
    CHECK(status == PIPELINE_OK);
    CHECK(result.stats.duplicate_content_documents == 1U);
    CHECK(result.report_path != NULL);
    if (result.report_path != NULL) {
        CHECK(strcmp(result.report_path, retry_report) == 0);
        CHECK(access(result.report_path, F_OK) == 0);
    }
    pipeline_result_free(&result);
    CHECK(live_count == 0U);

    test_report_failure_retry(port, REPORT_ERROR_WRITE);
    test_report_failure_retry(port, REPORT_ERROR_CLOSE);

    CHECK(make_output(output, sizeof(output)));
    clear_tracking();
    fail_at = 0U;
    download_fixture_reset_counts();
    status = run_one(port, output, &result);
    CHECK(status == PIPELINE_OK);
    pipeline_result_free(&result);
    CHECK(live_count == 0U);
    successful_allocations = allocation_calls;
    CHECK(successful_allocations > 0U);

    for (fail_position = 1U; fail_position <= successful_allocations;
         ++fail_position) {
        CHECK(make_output(output, sizeof(output)));
        clear_tracking();
        fail_at = fail_position;
        download_fixture_reset_counts();
        status = run_one(port, output, &result);
        CHECK(status == PIPELINE_ERROR_OUT_OF_MEMORY);
        CHECK(allocation_calls == fail_position);
        pipeline_result_free(&result);
        CHECK(live_count == 0U);
        pipeline_result_free(&result);
        CHECK(live_count == 0U);
    }
    fail_at = 0U;
    clear_tracking();
    download_fixture_stop();
    http_cleanup();
    remove_tree(test_root);
    printf("pipeline allocation tests: %zu failure positions, %u checks, "
           "%u failures\n", successful_allocations, checks, failures);
    return failures == 0U ? 0 : 1;
}
#endif
