#include "report.h"

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

static ReportRow sample_row(void)
{
    ReportRow row = {
        "https://university.example/resources/Final-Exam.PDF",
        DOCUMENT_TYPE_PDF,
        "https://university.example/courses/CS101",
        2U,
        REPORT_ROW_DOWNLOAD_FAILED,
        "",
        0U,
        "",
        ""
    };
    return row;
}

static void test_matching(void)
{
    ReportRow row = sample_row();
    ReportFilter filter = {NULL, 0U};
    int matches = 0;
    char mutable_keyword[] = "EXAM";
    char keyword_before[sizeof(mutable_keyword)];
    const char *one_keyword[] = {mutable_keyword};
    const char *two_keywords[] = {"unmatched", "cs101"};
    const char *no_keywords[] = {"not-present"};
    const char *overlap_match[] = {"aaaab"};
    const char *overlap_miss[] = {"aaaac"};

    CHECK(report_row_matches(&row, &filter, &matches) == REPORT_OK);
    CHECK(matches == 1);

    memcpy(keyword_before, mutable_keyword, sizeof(mutable_keyword));
    filter.keywords = one_keyword;
    filter.keyword_count = 1U;
    CHECK(report_row_matches(&row, &filter, &matches) == REPORT_OK);
    CHECK(matches == 1); /* ASCII case-insensitive match in document URL. */
    CHECK(memcmp(keyword_before, mutable_keyword, sizeof(mutable_keyword)) == 0);

    filter.keywords = two_keywords;
    filter.keyword_count = 2U;
    CHECK(report_row_matches(&row, &filter, &matches) == REPORT_OK);
    CHECK(matches == 1); /* Any keyword may match the source-page URL. */

    filter.keywords = no_keywords;
    filter.keyword_count = 1U;
    CHECK(report_row_matches(&row, &filter, &matches) == REPORT_OK);
    CHECK(matches == 0);

    filter.keywords = NULL;
    filter.keyword_count = 0U;
    row.document_url = "https://example.test/Case";
    filter.keywords = one_keyword;
    filter.keyword_count = 1U;
    mutable_keyword[0] = 'c';
    mutable_keyword[1] = 'A';
    mutable_keyword[2] = 's';
    mutable_keyword[3] = 'e';
    mutable_keyword[4] = '\0';
    CHECK(report_row_matches(&row, &filter, &matches) == REPORT_OK);
    CHECK(matches == 1);

    row.document_url = "aaaaaaaaaaaaaaaaab";
    filter.keywords = overlap_match;
    filter.keyword_count = 1U;
    CHECK(report_row_matches(&row, &filter, &matches) == REPORT_OK);
    CHECK(matches == 1);
    filter.keywords = overlap_miss;
    CHECK(report_row_matches(&row, &filter, &matches) == REPORT_OK);
    CHECK(matches == 0);
}

static void test_invalid_filters_and_rows(void)
{
    ReportRow row = sample_row();
    ReportRow metadata_only = {0};
    ReportFilter filter = {NULL, 0U};
    int matches = 1;
    char too_long[REPORT_MAX_KEYWORD_BYTES + 2U];
    char long_field[REPORT_MAX_FIELD_BYTES + 2U];
    char control_keyword[] = {'a', '\t', 'b', '\0'};
    char non_ascii_keyword[] = {(char)0xc3, (char)0xa9, '\0'};
    const char *null_item[] = {NULL};
    const char *empty_item[] = {""};
    const char *long_item[] = {too_long};
    const char *control_item[] = {control_keyword};
    const char *non_ascii_item[] = {non_ascii_keyword};
    const char *eight_items[REPORT_MAX_KEYWORDS];
    size_t index;

    memset(too_long, 'a', sizeof(too_long) - 1U);
    too_long[sizeof(too_long) - 1U] = '\0';
    for (index = 0; index < REPORT_MAX_KEYWORDS; ++index) {
        eight_items[index] = "x";
    }

    CHECK(report_row_matches(&row, NULL, &matches) ==
          REPORT_ERROR_INVALID_FILTER);
    CHECK(matches == 0);

    filter.keywords = NULL;
    filter.keyword_count = 1U;
    CHECK(report_row_matches(&row, &filter, &matches) ==
          REPORT_ERROR_INVALID_FILTER);

    filter.keywords = null_item;
    CHECK(report_row_matches(&row, &filter, &matches) ==
          REPORT_ERROR_INVALID_FILTER);
    filter.keywords = empty_item;
    CHECK(report_row_matches(&row, &filter, &matches) ==
          REPORT_ERROR_INVALID_FILTER);
    filter.keywords = long_item;
    CHECK(report_row_matches(&row, &filter, &matches) ==
          REPORT_ERROR_INVALID_FILTER);
    filter.keywords = control_item;
    CHECK(report_row_matches(&row, &filter, &matches) ==
          REPORT_ERROR_INVALID_FILTER);
    filter.keywords = non_ascii_item;
    CHECK(report_row_matches(&row, &filter, &matches) ==
          REPORT_ERROR_INVALID_FILTER);

    filter.keywords = eight_items;
    filter.keyword_count = REPORT_MAX_KEYWORDS;
    CHECK(report_row_matches(&row, &filter, &matches) == REPORT_OK);
    filter.keyword_count = REPORT_MAX_KEYWORDS + 1U;
    CHECK(report_row_matches(&row, &filter, &matches) ==
          REPORT_ERROR_INVALID_FILTER);

    CHECK(report_row_matches(NULL, &filter, &matches) ==
          REPORT_ERROR_INVALID_ROW);
    CHECK(report_row_matches(&row, &filter, NULL) ==
          REPORT_ERROR_INVALID_ARGUMENT);
    row.sha256_hex = "not-a-lowercase-sha256";
    filter.keyword_count = 0U;
    CHECK(report_row_matches(&row, &filter, &matches) == REPORT_OK);
    CHECK(matches == 1); /* Filtering does not depend on download outcome. */

    row = sample_row();
    row.document_url = "";
    CHECK(report_row_matches(&row, &filter, &matches) ==
          REPORT_ERROR_INVALID_ROW);
    memset(long_field, 'x', sizeof(long_field) - 1U);
    long_field[REPORT_MAX_FIELD_BYTES] = '\0';
    row = sample_row();
    row.document_url = long_field;
    CHECK(report_row_matches(&row, &filter, &matches) == REPORT_OK);
    CHECK(matches == 1);
    long_field[REPORT_MAX_FIELD_BYTES] = 'x';
    long_field[REPORT_MAX_FIELD_BYTES + 1U] = '\0';
    CHECK(report_row_matches(&row, &filter, &matches) ==
          REPORT_ERROR_INVALID_ROW);

    metadata_only.document_url = "https://example.test/report.pdf";
    metadata_only.source_page_url = "https://example.test/source";
    CHECK(report_row_matches(&metadata_only, &filter, &matches) == REPORT_OK);
    CHECK(matches == 1); /* Phase 12 can filter before download results exist. */
}

static void test_status_names(void)
{
    CHECK(strcmp(report_row_status_name(REPORT_ROW_DOWNLOADED_NEW),
                 "DOWNLOADED_NEW") == 0);
    CHECK(strcmp(report_row_status_name(REPORT_ROW_DUPLICATE),
                 "DUPLICATE") == 0);
    CHECK(strcmp(report_row_status_name(REPORT_ROW_DOWNLOAD_FAILED),
                 "DOWNLOAD_FAILED") == 0);
    CHECK(strcmp(report_row_status_name(REPORT_ROW_SKIPPED_LIMIT),
                 "SKIPPED_LIMIT") == 0);
    CHECK(report_status_message(REPORT_OK) != NULL);
    CHECK(report_status_message((ReportStatus)-1) != NULL);
}

int main(void)
{
    test_matching();
    test_invalid_filters_and_rows();
    test_status_names();
    printf("report unit: %u checks, %u failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
