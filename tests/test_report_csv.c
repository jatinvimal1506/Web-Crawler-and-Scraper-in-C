#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#include "report.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static unsigned checks;
static unsigned failures;

#define CHECK(expression) do { \
    ++checks; \
    if (!(expression)) { \
        ++failures; \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expression); \
    } \
} while (0)

static const char header[] =
    "document_url,document_type,source_page_url,source_depth,status,"
    "sha256,size_bytes,stored_path,error\r\n";

static int make_path(char *output, size_t capacity,
                     const char *root, const char *name)
{
    int written = snprintf(output, capacity, "%s/%s", root, name);
    return written >= 0 && (size_t)written < capacity;
}

static int write_file(const char *path, const char *bytes, size_t length)
{
    FILE *file = fopen(path, "wb");
    int ok;
    if (file == NULL) { return 0; }
    ok = fwrite(bytes, 1, length, file) == length;
    if (fclose(file) != 0) { ok = 0; }
    return ok;
}

static int file_equals(const char *path, const char *expected,
                       size_t expected_length)
{
    FILE *file = fopen(path, "rb");
    long size;
    char *contents;
    int equal;
    if (file == NULL) { return 0; }
    if (fseek(file, 0, SEEK_END) != 0 || (size = ftell(file)) < 0 ||
        (size_t)size != expected_length || fseek(file, 0, SEEK_SET) != 0) {
        (void)fclose(file);
        return 0;
    }
    contents = (char *)malloc(expected_length + 1U);
    if (contents == NULL) {
        (void)fclose(file);
        return 0;
    }
    equal = fread(contents, 1, expected_length, file) == expected_length &&
            memcmp(contents, expected, expected_length) == 0;
    if (fclose(file) != 0) { equal = 0; }
    free(contents);
    return equal;
}

static int count_crlf_records(const char *path, size_t *records)
{
    FILE *file = fopen(path, "rb");
    int previous = -1, current;
    int read_failed, close_failed;
    size_t count = 0;
    if (file == NULL || records == NULL) {
        if (file != NULL) { (void)fclose(file); }
        return 0;
    }
    while ((current = fgetc(file)) != EOF) {
        if (previous == '\r' && current == '\n') { ++count; }
        previous = current;
    }
    read_failed = ferror(file);
    close_failed = fclose(file) != 0;
    if (read_failed || close_failed) { return 0; }
    *records = count;
    return 1;
}

static void free_result(ReportResult *result)
{
    report_result_free(result);
    report_result_free(result);
    CHECK(result->output_path == NULL);
    CHECK(result->status == REPORT_OK);
}

int main(void)
{
    static const char digest[] =
        "0123456789abcdef0123456789abcdef"
        "0123456789abcdef0123456789abcdef";
    ReportRow rows[2] = {
        {
            "https://university.example/a,\"exam\".pdf",
            DOCUMENT_TYPE_PDF,
            "https://university.example/source\r\none",
            1U,
            REPORT_ROW_DOWNLOADED_NEW,
            digest,
            321U,
            "pdf/a,\"b.pdf",
            ""
        },
        {
            "=1+1",
            DOCUMENT_TYPE_ZIP,
            " \t@SUM(1,2)",
            2U,
            REPORT_ROW_DUPLICATE,
            digest,
            78U,
            " \t=HYPERLINK(\"bad\")",
            ""
        }
    };
    ReportRow many[REPORT_MAX_ROWS];
    ReportFilter no_filter = {NULL, 0U};
    const char *exam_keywords[] = {"EXAM"};
    const char *missing_keywords[] = {"not-found"};
    ReportFilter exam_filter = {exam_keywords, 1U};
    ReportFilter missing_filter = {missing_keywords, 1U};
    ReportResult result;
    char root_template[] = "build/report-csv-XXXXXX";
    char root[128], path[512], missing_dir[512];
    char long_name[REPORT_MAX_FILENAME_BYTES + 1U];
    char expected[4096], expected_first[3072];
    size_t record_count, index;
    int written;

    CHECK(mkdtemp(root_template) != NULL);
    (void)snprintf(root, sizeof(root), "%s", root_template);
    CHECK(report_result_init(NULL) == REPORT_ERROR_INVALID_ARGUMENT);
    CHECK(report_result_init(&result) == REPORT_OK);
    CHECK(report_result_init(&result) == REPORT_OK);

    written = snprintf(
        expected, sizeof(expected),
        "%s\"https://university.example/a,\"\"exam\"\".pdf\",\"PDF\","
        "\"https://university.example/source\r\none\",1,"
        "\"DOWNLOADED_NEW\",\"%s\",321,\"pdf/a,\"\"b.pdf\",\"\"\r\n"
        "\"'=1+1\",\"ZIP\",\"' \t@SUM(1,2)\",2,\"DUPLICATE\","
        "\"%s\",78,\"' \t=HYPERLINK(\"\"bad\"\")\",\"\"\r\n",
        header, digest, digest);
    CHECK(written >= 0 && (size_t)written < sizeof(expected));
    {
        int first_written = snprintf(
            expected_first, sizeof(expected_first),
            "%s\"https://university.example/a,\"\"exam\"\".pdf\",\"PDF\","
            "\"https://university.example/source\r\none\",1,"
            "\"DOWNLOADED_NEW\",\"%s\",321,\"pdf/a,\"\"b.pdf\",\"\"\r\n",
            header, digest);
        CHECK(first_written >= 0 &&
              (size_t)first_written < sizeof(expected_first));
    }

    CHECK(report_write_csv(root, "all.csv", rows, 2U, &no_filter, &result) ==
          REPORT_OK);
    CHECK(result.status == REPORT_OK && result.rows_written == 2U &&
          result.rows_excluded == 0U && result.output_path != NULL);
    CHECK(make_path(path, sizeof(path), root, "all.csv"));
    CHECK(strcmp(result.output_path, path) == 0);
    CHECK(file_equals(path, expected, (size_t)written));
    CHECK(strcmp(rows[0].document_url,
                 "https://university.example/a,\"exam\".pdf") == 0);
    CHECK(strcmp(rows[1].stored_path, " \t=HYPERLINK(\"bad\")") == 0);
    free_result(&result);

    {
        ReportRow source_pairs[2] = {rows[0], rows[0]};
        char expected_pairs[4096];
        int pair_written;
        source_pairs[1].source_page_url =
            "https://university.example/second-source";
        source_pairs[1].source_depth = 3U;
        pair_written = snprintf(
            expected_pairs, sizeof(expected_pairs),
            "%s\"https://university.example/a,\"\"exam\"\".pdf\",\"PDF\","
            "\"https://university.example/source\r\none\",1,"
            "\"DOWNLOADED_NEW\",\"%s\",321,\"pdf/a,\"\"b.pdf\",\"\"\r\n"
            "\"https://university.example/a,\"\"exam\"\".pdf\",\"PDF\","
            "\"https://university.example/second-source\",3,"
            "\"DOWNLOADED_NEW\",\"%s\",321,\"pdf/a,\"\"b.pdf\",\"\"\r\n",
            header, digest, digest);
        CHECK(pair_written >= 0 &&
              (size_t)pair_written < sizeof(expected_pairs));
        CHECK(report_write_csv(root, "sources.csv", source_pairs, 2U,
                               &no_filter, &result) == REPORT_OK);
        CHECK(result.rows_written == 2U && result.rows_excluded == 0U);
        CHECK(make_path(path, sizeof(path), root, "sources.csv"));
        CHECK(file_equals(path, expected_pairs, (size_t)pair_written));
        free_result(&result);
    }

    CHECK(report_write_csv(root, "exam.csv", rows, 2U, &exam_filter, &result) ==
          REPORT_OK);
    CHECK(result.rows_written == 1U && result.rows_excluded == 1U);
    CHECK(make_path(path, sizeof(path), root, "exam.csv"));
    CHECK(file_equals(path, expected_first, strlen(expected_first)));
    free_result(&result);

    CHECK(report_write_csv(root, "none.csv", rows, 2U, &missing_filter,
                           &result) == REPORT_OK);
    CHECK(result.rows_written == 0U && result.rows_excluded == 2U);
    CHECK(make_path(path, sizeof(path), root, "none.csv"));
    CHECK(file_equals(path, header, sizeof(header) - 1U));
    free_result(&result);

    CHECK(report_write_csv(root, "empty.csv", NULL, 0U, &no_filter, &result) ==
          REPORT_OK);
    CHECK(result.rows_written == 0U && result.rows_excluded == 0U);
    CHECK(make_path(path, sizeof(path), root, "empty.csv"));
    CHECK(file_equals(path, header, sizeof(header) - 1U));
    free_result(&result);

    CHECK(make_path(path, sizeof(path), root, "collision.csv"));
    CHECK(write_file(path, "keep", 4U));
    CHECK(report_write_csv(root, "collision.csv", rows, 2U, &no_filter,
                           &result) == REPORT_ERROR_EXISTS);
    CHECK(result.output_path == NULL);
    CHECK(file_equals(path, "keep", 4U));
    free_result(&result);

    {
        static const char *const invalid_names[] = {
            "", ".", "..", ".csv", "../bad.csv", "..\\bad.csv",
            "C:bad.csv", "C:\\bad.csv", "sub/bad.csv", "bad.txt",
            "CON.csv", "PRN.csv", "AUX.csv", "NUL.csv", "COM1.csv",
            "LPT9.csv"
        };
        for (index = 0;
             index < sizeof(invalid_names) / sizeof(invalid_names[0]);
             ++index) {
            CHECK(report_write_csv(root, invalid_names[index], rows, 2U,
                                   &no_filter, &result) ==
                  REPORT_ERROR_INVALID_ARGUMENT);
            CHECK(result.output_path == NULL);
            free_result(&result);
        }
    }
    memset(long_name, 'a', sizeof(long_name) - 1U);
    memcpy(long_name + sizeof(long_name) - 5U, ".csv", 5U);
    CHECK(report_write_csv(root, long_name, rows, 2U, &no_filter, &result) ==
          REPORT_OK);
    CHECK(result.output_path != NULL);
    free_result(&result);
    CHECK(make_path(path, sizeof(path), root, long_name));

    CHECK(make_path(missing_dir, sizeof(missing_dir), root, "missing-dir"));
    CHECK(report_write_csv(missing_dir, "bad.csv", rows, 2U, &no_filter,
                           &result) ==
          REPORT_ERROR_FILESYSTEM);
    free_result(&result);
    CHECK(report_write_csv(root, "badrows.csv", NULL, 1U, &no_filter,
                           &result) == REPORT_ERROR_INVALID_ARGUMENT);
    free_result(&result);
    CHECK(report_write_csv(root, "badfilter.csv", rows, 2U, NULL, &result) ==
          REPORT_ERROR_INVALID_FILTER);
    free_result(&result);
    {
        ReportRow invalid_row = rows[0];
        CHECK(make_path(path, sizeof(path), root, "badsha.csv"));
        invalid_row.sha256_hex = "not-a-sha256";
        CHECK(report_write_csv(root, "badsha.csv", &invalid_row, 1U,
                               &no_filter, &result) ==
              REPORT_ERROR_INVALID_ROW);
        CHECK(result.output_path == NULL && access(path, F_OK) != 0);
        free_result(&result);

        CHECK(make_path(path, sizeof(path), root, "missing-path.csv"));
        invalid_row = rows[0];
        invalid_row.stored_path = "";
        CHECK(report_write_csv(root, "missing-path.csv", &invalid_row, 1U,
                               &no_filter, &result) ==
              REPORT_ERROR_INVALID_ROW);
        CHECK(result.output_path == NULL && access(path, F_OK) != 0);
        free_result(&result);

        CHECK(make_path(path, sizeof(path), root, "false-failure.csv"));
        invalid_row = rows[1];
        invalid_row.sha256_hex = "";
        CHECK(report_write_csv(root, "false-failure.csv", &invalid_row, 1U,
                               &no_filter, &result) ==
              REPORT_ERROR_INVALID_ROW);
        CHECK(result.output_path == NULL && access(path, F_OK) != 0);
        free_result(&result);
    }

    for (index = 0; index < REPORT_MAX_ROWS; ++index) {
        many[index] = rows[0];
        many[index].source_page_url = "https://university.example/plain";
    }
    CHECK(report_write_csv(root, "thousand.csv", many, REPORT_MAX_ROWS,
                           &no_filter, &result) == REPORT_OK);
    CHECK(result.rows_written == REPORT_MAX_ROWS);
    CHECK(count_crlf_records(result.output_path, &record_count));
    CHECK(record_count == REPORT_MAX_ROWS + 1U);
    free_result(&result);
    CHECK(report_write_csv(root, "over-limit.csv", many, REPORT_MAX_ROWS + 1U,
                           &no_filter, &result) == REPORT_ERROR_TOO_MANY_ROWS);
    free_result(&result);

    CHECK(make_path(path, sizeof(path), root, "all.csv"));
    CHECK(remove(path) == 0);
    CHECK(make_path(path, sizeof(path), root, "sources.csv"));
    CHECK(remove(path) == 0);
    CHECK(make_path(path, sizeof(path), root, "exam.csv"));
    CHECK(remove(path) == 0);
    CHECK(make_path(path, sizeof(path), root, "none.csv"));
    CHECK(remove(path) == 0);
    CHECK(make_path(path, sizeof(path), root, "empty.csv"));
    CHECK(remove(path) == 0);
    CHECK(make_path(path, sizeof(path), root, "collision.csv"));
    CHECK(remove(path) == 0);
    CHECK(make_path(path, sizeof(path), root, long_name));
    CHECK(remove(path) == 0);
    CHECK(make_path(path, sizeof(path), root, "thousand.csv"));
    CHECK(remove(path) == 0);
    CHECK(rmdir(root) == 0);

    printf("report CSV integration: %u checks, %u failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
#else
#include <stdio.h>
int main(void)
{
    puts("SKIP: Phase 11 CSV filesystem tests require POSIX mkdtemp");
    return 0;
}
#endif
