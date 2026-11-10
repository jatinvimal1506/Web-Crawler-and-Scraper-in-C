#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
#include "download_fixture.h"
#include "http.h"
#include "pipeline.h"
#include "sha256.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
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

#ifdef _WIN32
int main(void)
{
    puts("pipeline integration: SKIP (local POSIX fixture is unavailable on Windows)");
    return 0;
}
#else
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static char test_root[256];
static unsigned output_index;

static int make_output_directory(char *path, size_t capacity)
{
    int written;
    ++output_index;
    written = snprintf(path, capacity, "%s/out-%03u", test_root, output_index);
    return written > 0 && (size_t)written < capacity &&
           mkdir(path, 0700) == 0;
}

static int make_child_path(char *path, size_t capacity, const char *root,
                           const char *name)
{
    int written = snprintf(path, capacity, "%s/%s", root, name);
    return written > 0 && (size_t)written < capacity;
}

static int make_url(char *url, size_t capacity, unsigned port,
                    const char *route)
{
    int written = snprintf(url, capacity, "http://127.0.0.1:%u%s",
                           port, route);
    return written > 0 && (size_t)written < capacity;
}

static PipelineStatus run_pipeline(
    unsigned port, const char *route, const char *output_root, size_t depth,
    int same_domain, long timeout, const char *const *keywords,
    size_t keyword_count, PipelineResult *result)
{
    char url[256];
    PipelineConfig config = {0};
    if (!make_url(url, sizeof(url), port, route)) {
        return PIPELINE_ERROR_INTERNAL;
    }
    config.start_url = url;
    config.output_root = output_root;
    config.report_filename = "report.csv";
    config.max_depth = depth;
    config.same_domain = same_domain;
    config.timeout_seconds = timeout;
    config.keywords = keywords;
    config.keyword_count = keyword_count;
    (void)pipeline_result_init(result);
    return pipeline_run(&config, result);
}

static size_t csv_record_count(const char *path)
{
    FILE *file = fopen(path, "rb");
    int previous = -1, current;
    size_t count = 0U;
    int failed;
    if (file == NULL) { return SIZE_MAX; }
    while ((current = fgetc(file)) != EOF) {
        if (previous == '\r' && current == '\n') { ++count; }
        previous = current;
    }
    failed = ferror(file);
    if (fclose(file) != 0) { failed = 1; }
    if (failed) { return SIZE_MAX; }
    return count;
}

static int file_contains(const char *path, const char *needle)
{
    char contents[32768];
    size_t used;
    FILE *file = fopen(path, "rb");
    int failed;
    if (file == NULL || needle == NULL) {
        if (file != NULL) { (void)fclose(file); }
        return 0;
    }
    used = fread(contents, 1U, sizeof(contents) - 1U, file);
    contents[used] = '\0';
    failed = ferror(file);
    if (fclose(file) != 0) { failed = 1; }
    if (failed) { return 0; }
    return strstr(contents, needle) != NULL;
}

static size_t file_occurrences(const char *path, const char *needle)
{
    char contents[32768];
    const char *cursor;
    size_t used, count = 0U, needle_length;
    FILE *file;
    int failed;
    if (path == NULL || needle == NULL || needle[0] == '\0') { return SIZE_MAX; }
    file = fopen(path, "rb");
    if (file == NULL) { return SIZE_MAX; }
    used = fread(contents, 1U, sizeof(contents) - 1U, file);
    contents[used] = '\0';
    failed = ferror(file);
    if (fclose(file) != 0) { failed = 1; }
    if (failed) { return SIZE_MAX; }
    needle_length = strlen(needle);
    cursor = contents;
    while ((cursor = strstr(cursor, needle)) != NULL) {
        ++count;
        cursor += needle_length;
    }
    return count;
}

static size_t child_entry_count(const char *path)
{
    DIR *directory = opendir(path);
    struct dirent *entry;
    size_t count = 0U;
    if (directory == NULL) { return SIZE_MAX; }
    while ((entry = readdir(directory)) != NULL) {
        if (strcmp(entry->d_name, ".") != 0 &&
            strcmp(entry->d_name, "..") != 0) {
            ++count;
        }
    }
    if (closedir(directory) != 0) { return SIZE_MAX; }
    return count;
}

static int write_text_file(const char *path, const char *text)
{
    FILE *file = fopen(path, "wb");
    size_t length;
    int ok;
    if (file == NULL) { return 0; }
    length = strlen(text);
    ok = fwrite(text, 1U, length, file) == length;
    if (fclose(file) != 0) { ok = 0; }
    return ok;
}

static int file_equals_text(const char *path, const char *text)
{
    char contents[256];
    size_t expected = strlen(text);
    FILE *file = fopen(path, "rb");
    int ok;
    if (file == NULL) { return 0; }
    ok = expected < sizeof(contents) &&
         fread(contents, 1U, expected, file) == expected &&
         memcmp(contents, text, expected) == 0 &&
         fgetc(file) == EOF;
    if (fclose(file) != 0) { ok = 0; }
    return ok;
}

static int write_bytes_file(const char *path, const unsigned char *bytes,
                            size_t size)
{
    FILE *file = fopen(path, "wb");
    int ok;
    if (file == NULL) { return 0; }
    ok = fwrite(bytes, 1U, size, file) == size;
    if (fclose(file) != 0) { ok = 0; }
    return ok;
}

static int file_equals_bytes(const char *path, const unsigned char *bytes,
                            size_t size)
{
    unsigned char contents[256];
    FILE *file = fopen(path, "rb");
    int ok;
    if (file == NULL || size > sizeof(contents)) {
        if (file != NULL) { (void)fclose(file); }
        return 0;
    }
    ok = fread(contents, 1U, size, file) == size &&
         memcmp(contents, bytes, size) == 0 && fgetc(file) == EOF;
    if (fclose(file) != 0) { ok = 0; }
    return ok;
}

static int canonical_file_matches(const char *output_root,
                                  const char *type_directory,
                                  const char *extension,
                                  const char *expected_digest,
                                  const unsigned char *expected_bytes,
                                  size_t expected_size)
{
    char path[2048], actual_digest[65] = {0};
    size_t actual_size = 0U;
    int written = snprintf(path, sizeof(path), "%s/%s/%s.%s", output_root,
                           type_directory, expected_digest, extension);
    return written > 0 && (size_t)written < sizeof(path) &&
           file_equals_bytes(path, expected_bytes, expected_size) &&
           sha256_file(path, actual_digest, &actual_size) == SHA256_OK &&
           actual_size == expected_size &&
           strcmp(actual_digest, expected_digest) == 0;
}

static void print_demo_directory(const char *output_root,
                                 const char *directory_name)
{
    char directory_path[1024];
    DIR *directory;
    struct dirent *entry;
    size_t entries = 0U;
    int written = snprintf(directory_path, sizeof(directory_path), "%s/%s",
                           output_root, directory_name);
    if (written <= 0 || (size_t)written >= sizeof(directory_path)) {
        printf("  %s/ (path unavailable)\n", directory_name);
        return;
    }
    printf("  %s/\n", directory_name);
    directory = opendir(directory_path);
    if (directory == NULL) {
        puts("    (could not list directory)");
        return;
    }
    while ((entry = readdir(directory)) != NULL) {
        char file_path[2048];
        char digest[65] = {0};
        size_t file_size = 0U;
        struct stat info;
        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        ++entries;
        written = snprintf(file_path, sizeof(file_path), "%s/%s",
                           directory_path, entry->d_name);
        if (written <= 0 || (size_t)written >= sizeof(file_path) ||
            stat(file_path, &info) != 0 || !S_ISREG(info.st_mode)) {
            printf("    %s (metadata unavailable)\n", entry->d_name);
            continue;
        }
        if (sha256_file(file_path, digest, &file_size) == SHA256_OK) {
            printf("    %s (%zu bytes, SHA-256 %s)\n",
                   entry->d_name, file_size, digest);
        } else {
            printf("    %s (%zu bytes, SHA-256 unavailable)\n",
                   entry->d_name, file_size);
        }
    }
    if (closedir(directory) != 0) {
        puts("    (directory close failed)");
    }
    if (entries == 0U) {
        puts("    (empty)");
    }
}

static void print_demo_output(const char *output_root, const char *report_path)
{
    static const char *const directory_names[] = {
        "pdf", "docx", "zip", "staging"
    };
    char report[1024];
    char line[2048];
    FILE *file;
    size_t index, lines = 0U;
    int written;

    printf("Controlled output tree: %s/\n", output_root);
    puts("  report.csv");
    for (index = 0U;
         index < sizeof(directory_names) / sizeof(directory_names[0]);
         ++index) {
        print_demo_directory(output_root, directory_names[index]);
    }
    written = snprintf(report, sizeof(report), "%s/report.csv", output_root);
    if (written <= 0 || (size_t)written >= sizeof(report)) {
        puts("CSV sample unavailable: report path is too long");
        return;
    }
    file = fopen(report, "rb");
    if (file == NULL) {
        puts("CSV sample unavailable: report could not be opened");
        return;
    }
    printf("CSV header and representative rows (%s):\n", report_path);
    while (fgets(line, sizeof(line), file) != NULL) {
        if (lines < 3U || strstr(line, "\"DUPLICATE\"") != NULL) {
            fputs("  ", stdout);
            fputs(line, stdout);
        }
        ++lines;
    }
    if (ferror(file)) {
        puts("  (CSV read failed)");
    }
    if (fclose(file) != 0) {
        puts("  (CSV close failed)");
    }
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

static void test_full_bounded_pipeline(unsigned port)
{
    static const unsigned char expected_pdf[] = "shared-pdf-bytes";
    static const unsigned char expected_docx[] = "lecture-docx";
    static const unsigned char expected_zip[] = "archive-zip";
    static const char expected_pdf_digest[] =
        "da609456d4b256ec70a2611b56b6bac2e9ea3162460752a384ae7d3a97cc6cfd";
    static const char expected_docx_digest[] =
        "7f525b7a512e13d07382af92a865628161eff9ed8ca8558a64e8a8577ae924af";
    static const char expected_zip_digest[] =
        "904369fb47f28dd75a6bdf43fa7064031f37e4b7da52078bbc8067466538abd1";
    char output[512], report[1024], directory[1024];
    char duplicate_path[1024];
    PipelineResult result = {0};
    PipelineStatus status;
    CHECK(make_output_directory(output, sizeof(output)));
    CHECK(make_child_path(report, sizeof(report), output, "report.csv"));
    download_fixture_reset_counts();
    status = run_pipeline(port, "/site", output, 1U, 1, 3L, NULL, 0U,
                          &result);
    CHECK(status == PIPELINE_OK);
    CHECK(result.status == PIPELINE_OK);
    CHECK(result.stats.html_pages_visited == 3U);
    CHECK(result.stats.html_pages_failed == 0U);
    CHECK(result.stats.html_links_discovered == 9U);
    CHECK(result.stats.unique_document_urls_discovered == 4U);
    CHECK(result.stats.document_source_relationships_discovered == 6U);
    CHECK(result.stats.unique_document_urls_selected == 4U);
    CHECK(result.stats.source_document_relationships_selected == 6U);
    CHECK(result.stats.newly_stored_documents == 3U);
    CHECK(result.stats.duplicate_content_documents == 1U);
    CHECK(result.stats.failed_document_downloads == 0U);
    CHECK(result.stats.documents_skipped_limit == 0U);
    CHECK(result.stats.successful_document_bytes_downloaded ==
          2U * (sizeof("shared-pdf-bytes") - 1U) +
              (sizeof("lecture-docx") - 1U) +
              (sizeof("archive-zip") - 1U));
    CHECK(result.stats.bytes_newly_stored ==
          (sizeof("shared-pdf-bytes") - 1U) +
              (sizeof("lecture-docx") - 1U) +
              (sizeof("archive-zip") - 1U));
    CHECK(result.stats.csv_rows_written == 6U);
    CHECK(result.report_path != NULL);
    CHECK(csv_record_count(report) == 7U);
    CHECK(file_contains(report, "DOWNLOADED_NEW"));
    CHECK(file_contains(report, "DUPLICATE"));
    CHECK(file_occurrences(report, "\"DUPLICATE\"") == 1U);
    CHECK(download_fixture_request_count(DFX_SITE) == 1U);
    CHECK(download_fixture_request_count(DFX_COURSES) == 1U);
    CHECK(download_fixture_request_count(DFX_RESEARCH) == 1U);
    CHECK(download_fixture_request_count(DFX_RULES_PDF) == 1U);
    CHECK(download_fixture_request_count(DFX_RESEARCH_PDF) == 1U);
    CHECK(download_fixture_request_count(DFX_LECTURE_DOCX) == 1U);
    CHECK(download_fixture_request_count(DFX_MATERIAL_ZIP) == 1U);
    CHECK(make_child_path(directory, sizeof(directory), output, "pdf"));
    CHECK(child_entry_count(directory) == 1U);
    CHECK(make_child_path(directory, sizeof(directory), output, "docx"));
    CHECK(child_entry_count(directory) == 1U);
    CHECK(make_child_path(directory, sizeof(directory), output, "zip"));
    CHECK(child_entry_count(directory) == 1U);
    CHECK(make_child_path(directory, sizeof(directory), output, "staging"));
    CHECK(child_entry_count(directory) == 0U);
    CHECK(canonical_file_matches(output, "pdf", "pdf", expected_pdf_digest,
                                 expected_pdf, sizeof(expected_pdf) - 1U));
    CHECK(canonical_file_matches(output, "docx", "docx",
                                 expected_docx_digest, expected_docx,
                                 sizeof(expected_docx) - 1U));
    CHECK(canonical_file_matches(output, "zip", "zip", expected_zip_digest,
                                 expected_zip, sizeof(expected_zip) - 1U));
    CHECK(make_child_path(duplicate_path, sizeof(duplicate_path), output,
                          "pdf"));
    {
        char full_duplicate_path[1536];
        int written = snprintf(full_duplicate_path, sizeof(full_duplicate_path),
                               "%s/%s.pdf", duplicate_path,
                               expected_pdf_digest);
        CHECK(written > 0 && (size_t)written < sizeof(full_duplicate_path));
        if (written > 0 && (size_t)written < sizeof(full_duplicate_path)) {
            CHECK(file_occurrences(report, full_duplicate_path) == 4U);
        }
    }
    pipeline_result_free(&result);
    pipeline_result_free(&result);
    CHECK(result.report_path == NULL && result.orphaned_path_count == 0U);
}

static void test_depth_and_empty_cases(unsigned port)
{
    char output[512], report[1024];
    PipelineResult result = {0};
    PipelineStatus status;

    CHECK(make_output_directory(output, sizeof(output)));
    CHECK(make_child_path(report, sizeof(report), output, "report.csv"));
    download_fixture_reset_counts();
    status = run_pipeline(port, "/site", output, 0U, 1, 3L, NULL, 0U,
                          &result);
    CHECK(status == PIPELINE_OK);
    CHECK(result.stats.html_pages_visited == 1U);
    CHECK(result.stats.unique_document_urls_discovered == 2U);
    CHECK(result.stats.document_source_relationships_discovered == 2U);
    CHECK(result.stats.csv_rows_written == 2U);
    CHECK(download_fixture_request_count(DFX_COURSES) == 0U);
    CHECK(download_fixture_request_count(DFX_RESEARCH) == 0U);
    CHECK(download_fixture_request_count(DFX_RULES_PDF) == 1U);
    pipeline_result_free(&result);

    CHECK(make_output_directory(output, sizeof(output)));
    CHECK(make_child_path(report, sizeof(report), output, "report.csv"));
    download_fixture_reset_counts();
    status = run_pipeline(port, "/site", output, 2U, 1, 3L, NULL, 0U,
                          &result);
    CHECK(status == PIPELINE_OK);
    CHECK(result.stats.html_pages_visited == 3U);
    CHECK(result.stats.csv_rows_written == 6U);
    pipeline_result_free(&result);

    CHECK(make_output_directory(output, sizeof(output)));
    CHECK(make_child_path(report, sizeof(report), output, "report.csv"));
    download_fixture_reset_counts();
    status = run_pipeline(port, "/empty-site", output, 0U, 1, 3L, NULL, 0U,
                          &result);
    CHECK(status == PIPELINE_OK);
    CHECK(result.stats.html_pages_visited == 1U);
    CHECK(result.stats.unique_document_urls_discovered == 0U);
    CHECK(result.stats.csv_rows_written == 0U);
    CHECK(csv_record_count(report) == 1U);
    pipeline_result_free(&result);
}

static void test_bfs_order_and_depth(unsigned port)
{
    char output[512], report[1024];
    PipelineResult result = {0};
    PipelineStatus status;

    CHECK(make_output_directory(output, sizeof(output)));
    CHECK(make_child_path(report, sizeof(report), output, "report.csv"));
    download_fixture_reset_counts();
    status = run_pipeline(port, "/bfs-root", output, 1U, 1, 3L,
                          NULL, 0U, &result);
    CHECK(status == PIPELINE_OK);
    CHECK(result.stats.html_pages_visited == 3U);
    CHECK(result.stats.unique_document_urls_discovered == 0U);
    CHECK(download_fixture_request_total() == 3U);
    CHECK(download_fixture_request_order(0U) == DFX_BFS_ROOT);
    CHECK(download_fixture_request_order(1U) == DFX_BFS_A);
    CHECK(download_fixture_request_order(2U) == DFX_BFS_B);
    pipeline_result_free(&result);

    CHECK(make_output_directory(output, sizeof(output)));
    CHECK(make_child_path(report, sizeof(report), output, "report.csv"));
    download_fixture_reset_counts();
    status = run_pipeline(port, "/bfs-root", output, 2U, 1, 3L,
                          NULL, 0U, &result);
    CHECK(status == PIPELINE_OK);
    CHECK(result.stats.html_pages_visited == 4U);
    CHECK(result.stats.unique_document_urls_discovered == 1U);
    CHECK(result.stats.document_source_relationships_discovered == 1U);
    CHECK(result.stats.csv_rows_written == 1U);
    if (!file_contains(report, ",2,\"DOWNLOADED_NEW\",")) {
        FILE *debug_report = fopen(report, "rb");
        if (debug_report != NULL) {
            char line[2048];
            while (fgets(line, sizeof(line), debug_report) != NULL) {
                fputs(line, stderr);
            }
            (void)fclose(debug_report);
        }
    }
    CHECK(file_contains(report, ",2,\"DOWNLOADED_NEW\","));
    CHECK(download_fixture_request_total() == 5U);
    CHECK(download_fixture_request_order(0U) == DFX_BFS_ROOT);
    CHECK(download_fixture_request_order(1U) == DFX_BFS_A);
    CHECK(download_fixture_request_order(2U) == DFX_BFS_B);
    CHECK(download_fixture_request_order(3U) == DFX_BFS_GRANDCHILD);
    CHECK(download_fixture_request_order(4U) == DFX_PDF);
    pipeline_result_free(&result);
}

static void test_filtering_and_failures(unsigned port)
{
    static const char *const lecture_keyword[] = {"LECTURE"};
    static const char *const source_keyword[] = {"/courses"};
    static const char *const no_match_keyword[] = {"no-such-document"};
    char output[512], report[1024];
    PipelineResult result = {0};
    PipelineStatus status;

    CHECK(make_output_directory(output, sizeof(output)));
    CHECK(make_child_path(report, sizeof(report), output, "report.csv"));
    download_fixture_reset_counts();
    status = run_pipeline(port, "/site", output, 1U, 1, 3L,
                          lecture_keyword, 1U, &result);
    CHECK(status == PIPELINE_OK);
    CHECK(result.stats.unique_document_urls_selected == 1U);
    CHECK(result.stats.source_document_relationships_selected == 1U);
    CHECK(result.stats.csv_rows_written == 1U);
    CHECK(download_fixture_request_count(DFX_LECTURE_DOCX) == 1U);
    CHECK(download_fixture_request_count(DFX_RULES_PDF) == 0U);
    CHECK(download_fixture_request_count(DFX_MATERIAL_ZIP) == 0U);
    pipeline_result_free(&result);

    CHECK(make_output_directory(output, sizeof(output)));
    CHECK(make_child_path(report, sizeof(report), output, "report.csv"));
    download_fixture_reset_counts();
    status = run_pipeline(port, "/site", output, 1U, 1, 3L,
                          source_keyword, 1U, &result);
    CHECK(status == PIPELINE_OK);
    CHECK(result.stats.unique_document_urls_selected == 2U);
    CHECK(result.stats.source_document_relationships_selected == 2U);
    CHECK(result.stats.csv_rows_written == 2U);
    CHECK(download_fixture_request_count(DFX_RULES_PDF) == 1U);
    CHECK(download_fixture_request_count(DFX_LECTURE_DOCX) == 1U);
    CHECK(download_fixture_request_count(DFX_RESEARCH_PDF) == 0U);
    CHECK(download_fixture_request_count(DFX_MATERIAL_ZIP) == 0U);
    pipeline_result_free(&result);

    CHECK(make_output_directory(output, sizeof(output)));
    CHECK(make_child_path(report, sizeof(report), output, "report.csv"));
    download_fixture_reset_counts();
    status = run_pipeline(port, "/site", output, 1U, 1, 3L,
                          no_match_keyword, 1U, &result);
    CHECK(status == PIPELINE_OK);
    CHECK(result.stats.unique_document_urls_selected == 0U);
    CHECK(result.stats.csv_rows_written == 0U);
    CHECK(csv_record_count(report) == 1U);
    CHECK(download_fixture_request_count(DFX_RULES_PDF) == 0U);
    pipeline_result_free(&result);

    CHECK(make_output_directory(output, sizeof(output)));
    CHECK(make_child_path(report, sizeof(report), output, "report.csv"));
    download_fixture_reset_counts();
    status = run_pipeline(port, "/failure-site", output, 0U, 1, 3L,
                          NULL, 0U, &result);
    CHECK(status == PIPELINE_PARTIAL);
    CHECK(result.stats.html_pages_visited == 1U);
    CHECK(result.stats.unique_document_urls_discovered == 8U);
    CHECK(result.stats.failed_document_downloads == 8U);
    CHECK(result.stats.csv_rows_written == 8U);
    CHECK(csv_record_count(report) == 9U);
    CHECK(download_fixture_request_count(DFX_NOT_FOUND) == 2U);
    CHECK(download_fixture_request_count(DFX_SERVER_ERROR) == 1U);
    CHECK(download_fixture_request_count(DFX_WRONG_MIME) == 2U);
    CHECK(download_fixture_request_count(DFX_EMPTY) == 1U);
    CHECK(download_fixture_request_count(DFX_MISSING_MIME) == 1U);
    CHECK(download_fixture_request_count(DFX_PARTIAL) == 1U);
    pipeline_result_free(&result);

    CHECK(make_output_directory(output, sizeof(output)));
    CHECK(make_child_path(report, sizeof(report), output, "report.csv"));
    download_fixture_reset_counts();
    status = run_pipeline(port, "/partial-crawl-site", output, 1U, 1, 3L,
                          NULL, 0U, &result);
    CHECK(status == PIPELINE_PARTIAL);
    CHECK(result.stats.html_pages_visited == 1U);
    CHECK(result.stats.html_pages_failed == 1U);
    CHECK(result.stats.unique_document_urls_discovered == 1U);
    CHECK(result.stats.newly_stored_documents == 1U);
    CHECK(result.stats.failed_document_downloads == 0U);
    CHECK(result.stats.csv_rows_written == 1U);
    CHECK(download_fixture_request_count(DFX_PARTIAL_CRAWL_SITE) == 1U);
    CHECK(download_fixture_request_count(DFX_FAILED_HTML_CHILD) == 1U);
    CHECK(download_fixture_request_count(DFX_RULES_PDF) == 1U);
    pipeline_result_free(&result);
}

static void test_preexisting_content(unsigned port)
{
    char output[512], report[1024], pdf_directory[1024], source[1024];
    char stored[1024], digest[65];
    const unsigned char *bytes;
    size_t size = 0U, hashed_size = 0U;
    PipelineResult result = {0};
    PipelineStatus status;
    int written;

    bytes = download_fixture_small_data(&size);
    CHECK(bytes != NULL && size != 0U);
    CHECK(make_output_directory(output, sizeof(output)));
    CHECK(make_child_path(pdf_directory, sizeof(pdf_directory), output, "pdf"));
    CHECK(mkdir(pdf_directory, 0700) == 0);
    CHECK(make_child_path(source, sizeof(source), output, "seed.bin"));
    CHECK(write_bytes_file(source, bytes, size));
    CHECK(sha256_file(source, digest, &hashed_size) == SHA256_OK);
    CHECK(hashed_size == size);
    written = snprintf(stored, sizeof(stored), "%s/%s.pdf",
                       pdf_directory, digest);
    CHECK(written > 0 && (size_t)written < sizeof(stored));
    CHECK(write_bytes_file(stored, bytes, size));
    CHECK(unlink(source) == 0);
    CHECK(make_child_path(report, sizeof(report), output, "report.csv"));
    download_fixture_reset_counts();
    status = run_pipeline(port, "/staging-site", output, 0U, 1, 3L,
                          NULL, 0U, &result);
    CHECK(status == PIPELINE_OK);
    CHECK(result.stats.successful_document_bytes_downloaded == size);
    CHECK(result.stats.newly_stored_documents == 0U);
    CHECK(result.stats.duplicate_content_documents == 1U);
    CHECK(result.stats.bytes_newly_stored == 0U);
    CHECK(result.stats.csv_rows_written == 1U);
    CHECK(file_contains(report, "DUPLICATE"));
    CHECK(file_equals_bytes(stored, bytes, size));
    CHECK(child_entry_count(pdf_directory) == 1U);
    CHECK(download_fixture_request_count(DFX_PDF) == 1U);
    pipeline_result_free(&result);
}

static void test_redirects_and_scope(unsigned port)
{
    char output[512], report[1024], url[256];
    PipelineResult result = {0};
    PipelineStatus status;

    CHECK(make_output_directory(output, sizeof(output)));
    CHECK(make_child_path(report, sizeof(report), output, "report.csv"));
    download_fixture_reset_counts();
    status = run_pipeline(port, "/redirected-site", output, 0U, 1, 3L,
                          NULL, 0U, &result);
    CHECK(status == PIPELINE_OK);
    CHECK(result.stats.html_pages_visited == 1U);
    CHECK(result.stats.unique_document_urls_discovered == 1U);
    CHECK(download_fixture_request_count(DFX_REDIRECTED_SITE) == 1U);
    CHECK(download_fixture_request_count(DFX_REDIRECTED_PAGE) == 1U);
    CHECK(download_fixture_request_count(DFX_REDIRECTED_PDF) == 1U);
    CHECK(make_url(url, sizeof(url), port, "/dir/site.html"));
    CHECK(file_contains(report, url));
    pipeline_result_free(&result);

    CHECK(make_output_directory(output, sizeof(output)));
    CHECK(make_child_path(report, sizeof(report), output, "report.csv"));
    download_fixture_reset_counts();
    status = run_pipeline(port, "/binary-redirect-site", output, 0U, 1, 3L,
                          NULL, 0U, &result);
    CHECK(status == PIPELINE_OK);
    CHECK(download_fixture_request_count(DFX_RELATIVE_REDIRECT) == 1U);
    CHECK(download_fixture_request_count(DFX_PDF) == 1U);
    pipeline_result_free(&result);

    CHECK(make_output_directory(output, sizeof(output)));
    CHECK(make_child_path(report, sizeof(report), output, "report.csv"));
    download_fixture_reset_counts();
    status = run_pipeline(port, "/scope-site", output, 0U, 1, 3L,
                          NULL, 0U, &result);
    CHECK(status == PIPELINE_PARTIAL);
    CHECK(result.stats.failed_document_downloads == 3U);
    CHECK(download_fixture_request_count(DFX_OFF_HOST_REDIRECT) == 1U);
    CHECK(download_fixture_request_count(DFX_LOCALHOST_REDIRECT) == 1U);
    CHECK(download_fixture_request_count(DFX_OFF_PORT_REDIRECT) == 1U);
    CHECK(download_fixture_request_count(DFX_HOST_TARGET) == 0U);
    CHECK(download_fixture_request_count(DFX_PORT_TARGET) == 0U);
    CHECK(download_fixture_request_count(DFX_NOT_FOUND) == 0U);
    pipeline_result_free(&result);

    CHECK(make_output_directory(output, sizeof(output)));
    CHECK(make_child_path(report, sizeof(report), output, "report.csv"));
    download_fixture_reset_counts();
    status = run_pipeline(port, "/scope-site", output, 0U, 0, 3L,
                          NULL, 0U, &result);
    CHECK(status == PIPELINE_PARTIAL);
    CHECK(result.stats.failed_document_downloads == 1U);
    CHECK(result.stats.newly_stored_documents == 1U);
    CHECK(result.stats.duplicate_content_documents == 1U);
    CHECK(download_fixture_request_count(DFX_HOST_TARGET) == 2U);
    CHECK(download_fixture_request_count(DFX_PORT_TARGET) == 0U);
    pipeline_result_free(&result);
}

static void test_limits_and_preflight(unsigned port)
{
    char output[512], report[1024], staging[1024], occupied[1024];
    PipelineResult result = {0};
    PipelineStatus status;
    size_t index;

    CHECK(make_output_directory(output, sizeof(output)));
    CHECK(make_child_path(report, sizeof(report), output, "report.csv"));
    download_fixture_reset_counts();
    status = run_pipeline(port, "/limit-site", output, 0U, 1, 3L,
                          NULL, 0U, &result);
    CHECK(status == PIPELINE_LIMIT_REACHED);
    CHECK(result.stats.unique_document_urls_selected == 11U);
    CHECK(result.stats.documents_skipped_limit == 1U);
    CHECK(result.stats.newly_stored_documents == 10U);
    CHECK(result.stats.csv_rows_written == 11U);
    CHECK(csv_record_count(report) == 12U);
    for (index = 0U; index < 10U; ++index) {
        CHECK(download_fixture_request_count(DFX_ITEM_BASE + (unsigned)index) ==
              1U);
    }
    CHECK(download_fixture_request_count(DFX_ITEM_BASE + 10U) == 0U);
    pipeline_result_free(&result);

    CHECK(make_output_directory(output, sizeof(output)));
    CHECK(make_child_path(staging, sizeof(staging), output, "staging"));
    CHECK(mkdir(staging, 0700) == 0);
    CHECK(make_child_path(occupied, sizeof(occupied), staging,
                          "stage-000001.pdf"));
    CHECK(write_text_file(occupied, "keep-this-file"));
    CHECK(make_child_path(report, sizeof(report), output, "report.csv"));
    download_fixture_reset_counts();
    status = run_pipeline(port, "/staging-site", output, 0U, 1, 3L,
                          NULL, 0U, &result);
    CHECK(status == PIPELINE_OK);
    CHECK(file_equals_text(occupied, "keep-this-file"));
    CHECK(child_entry_count(staging) == 1U);
    pipeline_result_free(&result);

    CHECK(make_output_directory(output, sizeof(output)));
    CHECK(make_child_path(report, sizeof(report), output, "report.csv"));
    CHECK(write_text_file(report, "preexisting"));
    download_fixture_reset_counts();
    status = run_pipeline(port, "/site", output, 1U, 1, 3L,
                          NULL, 0U, &result);
    CHECK(status == PIPELINE_ERROR_REPORT);
    CHECK(download_fixture_request_count(DFX_SITE) == 0U);
    CHECK(file_equals_text(report, "preexisting"));
    pipeline_result_free(&result);

    CHECK(snprintf(output, sizeof(output), "%s/missing-output-root",
                   test_root) > 0);
    download_fixture_reset_counts();
    status = run_pipeline(port, "/site", output, 1U, 1, 3L,
                          NULL, 0U, &result);
    CHECK(status == PIPELINE_ERROR_FILESYSTEM);
    CHECK(download_fixture_request_total() == 0U);
    pipeline_result_free(&result);
}

static void test_timeout_and_cli(unsigned port, const char *cli_path)
{
    char output[512], report[1024], url[256];
    PipelineResult result = {0};
    PipelineStatus status;

    CHECK(make_output_directory(output, sizeof(output)));
    CHECK(make_child_path(report, sizeof(report), output, "report.csv"));
    download_fixture_reset_counts();
    status = run_pipeline(port, "/timeout-site", output, 0U, 1, 1L,
                          NULL, 0U, &result);
    CHECK(status == PIPELINE_PARTIAL);
    CHECK(result.stats.failed_document_downloads == 1U);
    CHECK(download_fixture_request_count(DFX_SLOW) == 1U);
    CHECK(csv_record_count(report) == 2U);
    pipeline_result_free(&result);

    CHECK(make_output_directory(output, sizeof(output)));
    CHECK(make_child_path(report, sizeof(report), output, "report.csv"));
    CHECK(make_url(url, sizeof(url), port, "/site"));
    {
        int descriptors[2];
        pid_t child;
        int child_status = 0;
        char output_text[8192] = {0};
        size_t used = 0U;
        ssize_t got;
        if (pipe(descriptors) != 0) {
            CHECK(0);
            return;
        }
        child = fork();
        if (child < 0) {
            CHECK(0);
            (void)close(descriptors[0]);
            (void)close(descriptors[1]);
            return;
        }
        if (child == 0) {
            (void)close(descriptors[0]);
            (void)dup2(descriptors[1], STDOUT_FILENO);
            (void)dup2(descriptors[1], STDERR_FILENO);
            (void)close(descriptors[1]);
            execl(cli_path, cli_path, url, output, "report.csv", "1", "1",
                  (char *)NULL);
            _exit(127);
        }
        (void)close(descriptors[1]);
        while ((got = read(descriptors[0], output_text + used,
                           sizeof(output_text) - used - 1U)) > 0) {
            used += (size_t)got;
            if (used >= sizeof(output_text) - 1U) { break; }
        }
        (void)close(descriptors[0]);
        CHECK(waitpid(child, &child_status, 0) == child);
        output_text[used] = '\0';
        CHECK(WIFEXITED(child_status) && WEXITSTATUS(child_status) == 0);
        CHECK(strstr(output_text,
                     "Pipeline status: pipeline completed successfully") != NULL);
        CHECK(strstr(output_text, "HTML pages visited: 3") != NULL);
        CHECK(strstr(output_text, "CSV rows written: 6") != NULL);
        CHECK(csv_record_count(report) == 7U);
        printf("Controlled CLI invocation: %s %s %s report.csv 1 1\n",
               cli_path, url, output);
        if (WIFEXITED(child_status)) {
            printf("Controlled CLI exit code: %d\n",
                   WEXITSTATUS(child_status));
        } else {
            puts("Controlled CLI did not exit normally");
        }
        puts("controlled DeepDownload-C CLI run:");
        fputs(output_text, stdout);
        print_demo_output(output, report);
    }

    CHECK(make_output_directory(output, sizeof(output)));
    CHECK(make_child_path(report, sizeof(report), output, "report.csv"));
    CHECK(make_url(url, sizeof(url), port, "/site"));
    download_fixture_reset_counts();
    {
        int descriptors[2];
        pid_t child;
        int child_status = 0;
        char output_text[8192] = {0};
        size_t used = 0U;
        ssize_t got;
        if (pipe(descriptors) != 0) {
            CHECK(0);
            return;
        }
        child = fork();
        if (child < 0) {
            CHECK(0);
            (void)close(descriptors[0]);
            (void)close(descriptors[1]);
            return;
        }
        if (child == 0) {
            (void)close(descriptors[0]);
            (void)dup2(descriptors[1], STDOUT_FILENO);
            (void)dup2(descriptors[1], STDERR_FILENO);
            (void)close(descriptors[1]);
            execl(cli_path, cli_path, url, output, "report.csv", "1", "1",
                  "--keyword", "LECTURE", (char *)NULL);
            _exit(127);
        }
        (void)close(descriptors[1]);
        while ((got = read(descriptors[0], output_text + used,
                           sizeof(output_text) - used - 1U)) > 0) {
            used += (size_t)got;
            if (used >= sizeof(output_text) - 1U) { break; }
        }
        (void)close(descriptors[0]);
        CHECK(waitpid(child, &child_status, 0) == child);
        output_text[used] = '\0';
        CHECK(WIFEXITED(child_status) && WEXITSTATUS(child_status) == 0);
        CHECK(strstr(output_text, "Unique document URLs selected: 1") != NULL);
        CHECK(strstr(output_text, "Newly stored documents: 1") != NULL);
        CHECK(strstr(output_text, "CSV rows written: 1") != NULL);
        CHECK(csv_record_count(report) == 2U);
        CHECK(download_fixture_request_count(DFX_LECTURE_DOCX) == 1U);
        CHECK(download_fixture_request_count(DFX_RULES_PDF) == 0U);
        CHECK(download_fixture_request_count(DFX_RESEARCH_PDF) == 0U);
        CHECK(download_fixture_request_count(DFX_MATERIAL_ZIP) == 0U);
        printf("Keyword-filtered CLI invocation: %s %s %s report.csv 1 1 "
               "--keyword LECTURE\n", cli_path, url, output);
        if (WIFEXITED(child_status)) {
            printf("Keyword-filtered CLI exit code: %d\n",
                   WEXITSTATUS(child_status));
        } else {
            puts("Keyword-filtered CLI did not exit normally");
        }
        fputs(output_text, stdout);
    }

    CHECK(make_output_directory(output, sizeof(output)));
    CHECK(make_url(url, sizeof(url), port, "/site"));
    download_fixture_reset_counts();
    {
        pid_t child = fork();
        int child_status = 0;
        if (child < 0) {
            CHECK(0);
            return;
        }
        if (child == 0) {
            int sink = open("/dev/null", O_WRONLY);
            if (sink >= 0) {
                (void)dup2(sink, STDOUT_FILENO);
                (void)dup2(sink, STDERR_FILENO);
                (void)close(sink);
            }
            execl(cli_path, cli_path, url, output, "invalid.csv", "4",
                  (char *)NULL);
            _exit(127);
        }
        CHECK(waitpid(child, &child_status, 0) == child);
        CHECK(WIFEXITED(child_status) && WEXITSTATUS(child_status) == 2);
        CHECK(download_fixture_request_total() == 0U);
        printf("Invalid CLI invocation: %s %s %s invalid.csv 4; exit code %d, "
               "fixture requests %zu\n", cli_path, url, output,
               WIFEXITED(child_status) ? WEXITSTATUS(child_status) : -1,
               download_fixture_request_total());
    }
}

int main(int argc, char **argv)
{
    unsigned port;
    char template_path[] = "/tmp/deepdownload-pipeline-XXXXXX";
    if (argc != 2) {
        fprintf(stderr, "usage: %s PATH_TO_DEEPDOWNLOAD_CLI\n", argv[0]);
        return 2;
    }
    if (mkdtemp(template_path) == NULL) {
        perror("mkdtemp");
        return 2;
    }
    (void)snprintf(test_root, sizeof(test_root), "%s", template_path);
    port = download_fixture_start();
    CHECK(port != 0U);

    test_full_bounded_pipeline(port);
    test_depth_and_empty_cases(port);
    test_bfs_order_and_depth(port);
    test_filtering_and_failures(port);
    test_preexisting_content(port);
    test_redirects_and_scope(port);
    test_limits_and_preflight(port);
    test_timeout_and_cli(port, argv[1]);

    download_fixture_stop();
    http_cleanup();
    remove_tree(test_root);
    printf("pipeline integration: %u checks, %u failures\n", checks, failures);
    return failures == 0U ? 0 : 1;
}
#endif
