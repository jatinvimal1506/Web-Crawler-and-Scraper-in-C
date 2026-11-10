#include "pipeline.h"

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

static PipelineConfig valid_config(void)
{
    PipelineConfig config = {0};
    config.start_url = "https://example.test/start";
    config.output_root = ".";
    config.report_filename = "report.csv";
    config.max_depth = 1U;
    config.same_domain = 1;
    config.timeout_seconds = 3L;
    return config;
}

static void expect_invalid(PipelineConfig config)
{
    PipelineResult result = {0};
    CHECK(pipeline_run(&config, &result) ==
          PIPELINE_ERROR_INVALID_ARGUMENT);
    CHECK(result.status == PIPELINE_ERROR_INVALID_ARGUMENT);
    CHECK(result.stats.html_pages_visited == 0U);
    CHECK(result.stats.unique_document_urls_discovered == 0U);
    CHECK(result.report_path == NULL);
    pipeline_result_free(&result);
}

int main(void)
{
    PipelineResult result = {0};
    PipelineConfig config = valid_config();
    static const char *const empty_keyword[] = {""};
    static const char *const too_many_keywords[] = {
        "a", "b", "c", "d", "e", "f", "g", "h", "i"
    };

    CHECK(pipeline_result_init(NULL) == PIPELINE_ERROR_INVALID_ARGUMENT);
    CHECK(pipeline_result_init(&result) == PIPELINE_OK);
    CHECK(pipeline_status_message(PIPELINE_OK) != NULL);
    CHECK(pipeline_status_message((PipelineStatus)-1) != NULL);
    CHECK(pipeline_run(NULL, &result) == PIPELINE_ERROR_INVALID_ARGUMENT);
    CHECK(result.status == PIPELINE_ERROR_INVALID_ARGUMENT);
    pipeline_result_free(&result);
    pipeline_result_free(&result);
    CHECK(result.report_path == NULL && result.orphaned_path_count == 0U);

    config = valid_config();
    config.start_url = "ftp://example.test/file.pdf";
    expect_invalid(config);
    config = valid_config();
    config.same_domain = 2;
    expect_invalid(config);
    config = valid_config();
    config.timeout_seconds = 0L;
    expect_invalid(config);
    config = valid_config();
    config.timeout_seconds = -1L;
    expect_invalid(config);
    config = valid_config();
    config.timeout_seconds = 3601L;
    expect_invalid(config);
    config = valid_config();
    config.report_filename = "../report.csv";
    expect_invalid(config);
    config = valid_config();
    config.keywords = empty_keyword;
    config.keyword_count = 1U;
    expect_invalid(config);
    config = valid_config();
    config.keywords = too_many_keywords;
    config.keyword_count = 9U;
    expect_invalid(config);

    result = (PipelineResult){0};
    CHECK(pipeline_result_init(&result) == PIPELINE_OK);
    result.stats.html_pages_visited = 1U;
    CHECK(pipeline_run(&config, &result) == PIPELINE_ERROR_INVALID_ARGUMENT);
    CHECK(result.stats.html_pages_visited == 1U);
    result.stats.html_pages_visited = 0U;
    pipeline_result_free(&result);

    printf("pipeline unit: %u checks, %u failures\n", checks, failures);
    return failures == 0U ? 0 : 1;
}
