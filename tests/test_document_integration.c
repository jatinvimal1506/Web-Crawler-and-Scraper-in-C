#include "document.h"
#include "crawler_fixture.h"
#include "phase5_check.h"
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
int main(void)
{
    puts("SKIP document local integration fixture requires POSIX sockets; native Windows NOT VERIFIED.");
    return 0;
}
#else
static unsigned port;
static char seed[128];

static void request_order(const unsigned *expected, size_t count)
{
    unsigned actual[32];
    size_t got = crawler_fixture_requests(actual, 32), i;
    CHECK(got == count);
    for (i = 0; i < count; ++i) { CHECK(actual[i] == expected[i]); }
}

static DocumentStatus run(const char *path, size_t depth, int same_domain,
                          DocumentResults *results, CrawlerStats *stats)
{
    char url[160], original[160];
    CrawlerConfig config;
    (void)snprintf(url, sizeof(url), "http://127.0.0.1:%u%s", port, path);
    memcpy(original, url, strlen(url) + 1);
    config = (CrawlerConfig){url, depth, same_domain, 2};
    CHECK(document_results_init(results) == DOCUMENT_OK);
    {
        DocumentStatus status = document_discover(&config, results, stats);
        CHECK(strcmp(url, original) == 0);
        return status;
    }
}

int main(void)
{
    DocumentResults results = {0};
    CrawlerStats stats;
    const DocumentRecord *record;
    char expected_rules[200], expected_zip[200], expected_lecture[200];
    char expected_research[200], expected_landing[200], expected_paper[240];
    char expected_courses[200], expected_research_page[200];
    char expected_home[200], expected_canonical_doc[240];
    char expected_archive[220], expected_scope_doc[240];
    size_t body_home, body_courses, body_research;
    port = crawler_fixture_start();
    (void)snprintf(seed, sizeof(seed), "http://127.0.0.1:%u", port);
    (void)snprintf(expected_rules, sizeof(expected_rules), "%s/documents/rules.pdf", seed);
    (void)snprintf(expected_zip, sizeof(expected_zip), "%s/files/material.zip", seed);
    (void)snprintf(expected_lecture, sizeof(expected_lecture),
                   "%s/documents/lecture.docx", seed);
    (void)snprintf(expected_research, sizeof(expected_research),
                   "%s/documents/research.pdf", seed);
    (void)snprintf(expected_courses, sizeof(expected_courses), "%s/courses", seed);
    (void)snprintf(expected_research_page, sizeof(expected_research_page),
                   "%s/research", seed);
    (void)snprintf(expected_home, sizeof(expected_home), "%s/doc-home", seed);
    (void)snprintf(expected_canonical_doc, sizeof(expected_canonical_doc),
                   "http://localhost:%u/files/same.PDF?download=1", port);
    (void)snprintf(expected_landing, sizeof(expected_landing),
                   "%s/docs/folder/landing", seed);
    (void)snprintf(expected_paper, sizeof(expected_paper),
                   "%s/docs/folder/paper.PDF?download=1", seed);
    (void)snprintf(expected_archive, sizeof(expected_archive),
                   "%s/docs/archive.ZIP", seed);
    body_home = crawler_fixture_body_size(FIX_DOC_HOME);
    body_courses = crawler_fixture_body_size(FIX_DOC_COURSES);
    body_research = crawler_fixture_body_size(FIX_DOC_RESEARCH);

    CHECK(run("/doc-home", 0, 1, &results, &stats) == DOCUMENT_OK);
    CHECK(document_results_count(&results) == 2);
    CHECK(document_results_source_count(&results) == 2);
    CHECK(stats.pages_visited == 1 && stats.pages_failed == 0 &&
          stats.links_discovered == 4 && stats.max_depth_reached == 0 &&
          stats.bytes_downloaded == body_home);
    request_order((unsigned[]){FIX_DOC_HOME}, 1);
    document_results_free(&results);

    CHECK(run("/doc-home", 1, 1, &results, &stats) == DOCUMENT_OK);
    CHECK(stats.pages_visited == 3 && stats.pages_failed == 0 &&
          stats.links_discovered == 9 && stats.max_depth_reached == 1 &&
          stats.bytes_downloaded == body_home + body_courses + body_research);
    CHECK(document_results_count(&results) == 4);
    CHECK(document_results_source_count(&results) == 6);
    request_order((unsigned[]){FIX_DOC_HOME, FIX_DOC_COURSES, FIX_DOC_RESEARCH}, 3);
    record = document_results_at(&results, 0);
    CHECK(record != NULL && strcmp(record->url, expected_rules) == 0);
    CHECK(record->type == DOCUMENT_TYPE_PDF && record->source_count == 3);
    CHECK(strcmp(record->sources[0].page_url, expected_home) == 0 &&
          record->sources[0].depth == 0);
    CHECK(strcmp(record->sources[1].page_url, expected_courses) == 0);
    CHECK(strcmp(record->sources[2].page_url, expected_research_page) == 0);
    CHECK(record->sources[1].depth == 1 && record->sources[2].depth == 1);
    record = document_results_at(&results, 1);
    CHECK(record != NULL && strcmp(record->url, expected_zip) == 0 &&
          record->type == DOCUMENT_TYPE_ZIP && record->source_count == 1);
    record = document_results_at(&results, 2);
    CHECK(record != NULL && strcmp(record->url, expected_lecture) == 0 &&
          record->type == DOCUMENT_TYPE_DOCX && record->source_count == 1);
    record = document_results_at(&results, 3);
    CHECK(record != NULL && strcmp(record->url, expected_research) == 0 &&
          record->type == DOCUMENT_TYPE_PDF && record->source_count == 1);
    CHECK(document_discover(&(CrawlerConfig){seed, 1, 1, 2}, &results, &stats) ==
          DOCUMENT_ERROR_INVALID_ARGUMENT);
    CHECK(document_results_count(&results) == 4 && stats.pages_visited == 0);
    document_results_free(&results);
    document_results_free(&results);

    CHECK(run("/doc-depth/0", 0, 1, &results, &stats) == DOCUMENT_OK);
    CHECK(document_results_count(&results) == 1);
    CHECK(document_results_at(&results, 0)->sources[0].depth == 0);
    request_order((unsigned[]){FIX_DOC_DEPTH_0}, 1);
    document_results_free(&results);
    CHECK(run("/doc-depth/0", 1, 1, &results, &stats) == DOCUMENT_OK);
    CHECK(document_results_count(&results) == 2);
    CHECK(document_results_at(&results, 1)->sources[0].depth == 1);
    request_order((unsigned[]){FIX_DOC_DEPTH_0, FIX_DOC_DEPTH_1}, 2);
    document_results_free(&results);
    CHECK(run("/doc-depth/0", 2, 1, &results, &stats) == DOCUMENT_OK);
    CHECK(document_results_count(&results) == 3);
    CHECK(document_results_at(&results, 2)->sources[0].depth == 2);
    request_order((unsigned[]){FIX_DOC_DEPTH_0, FIX_DOC_DEPTH_1, FIX_DOC_DEPTH_2}, 3);
    document_results_free(&results);

    CHECK(run("/doc-redirect", 1, 1, &results, &stats) == DOCUMENT_OK);
    CHECK(stats.pages_visited == 2 && stats.pages_failed == 0);
    CHECK(document_results_count(&results) == 4);
    CHECK(strcmp(document_results_at(&results, 0)->url, expected_paper) == 0);
    CHECK(strcmp(document_results_at(&results, 1)->url, expected_archive) == 0);
    CHECK(strcmp(document_results_at(&results, 0)->sources[0].page_url,
                 expected_landing) == 0);
    CHECK(document_results_at(&results, 0)->sources[0].depth == 0);
    request_order((unsigned[]){FIX_DOC_REDIRECT, FIX_DOC_LANDING, FIX_DOC_COURSES}, 3);
    document_results_free(&results);

    CHECK(run("/doc-scope", 1, 1, &results, &stats) == DOCUMENT_OK);
    CHECK(document_results_count(&results) == 0);
    request_order((unsigned[]){FIX_DOC_SCOPE}, 1);
    document_results_free(&results);
    CHECK(run("/doc-scope", 1, 0, &results, &stats) == DOCUMENT_OK);
    CHECK(document_results_count(&results) == 1);
    (void)snprintf(expected_scope_doc, sizeof(expected_scope_doc),
                   "http://localhost:%u/files/offscope.pdf", port);
    CHECK(strcmp(document_results_at(&results, 0)->url, expected_scope_doc) == 0);
    CHECK(document_results_at(&results, 0)->sources[0].depth == 0);
    request_order((unsigned[]){FIX_DOC_SCOPE}, 1);
    document_results_free(&results);

    CHECK(run("/doc-canon", 1, 0, &results, &stats) == DOCUMENT_OK);
    CHECK(document_results_count(&results) == 1);
    CHECK(document_results_source_count(&results) == 1);
    CHECK(strcmp(document_results_at(&results, 0)->url, expected_canonical_doc) == 0);
    request_order((unsigned[]){FIX_DOC_CANON}, 1);
    document_results_free(&results);

    CHECK(run("/doc-query", 1, 1, &results, &stats) == DOCUMENT_OK);
    CHECK(document_results_count(&results) == 2);
    CHECK(document_results_source_count(&results) == 2);
    request_order((unsigned[]){FIX_DOC_QUERY}, 1);
    document_results_free(&results);

    CHECK(run("/empty", 2, 1, &results, &stats) == DOCUMENT_OK);
    CHECK(document_results_count(&results) == 0 && stats.pages_visited == 1);
    request_order((unsigned[]){FIX_EMPTY}, 1);
    document_results_free(&results);

    CHECK(run("/candidate-limit", 1, 1, &results, &stats) == DOCUMENT_LIMIT_REACHED);
    CHECK(document_results_count(&results) == 0);
    CHECK(stats.pages_failed == 1 && stats.pages_visited == 0);
    request_order((unsigned[]){FIX_CANDIDATE_LIMIT}, 1);
    document_results_free(&results);

    CHECK(run("/doc-fail", 1, 1, &results, &stats) == DOCUMENT_PARTIAL);
    CHECK(stats.pages_visited == 2 && stats.pages_failed == 1);
    CHECK(document_results_count(&results) == 2);
    request_order((unsigned[]){FIX_DOC_FAIL, FIX_DOC_COURSES, FIX_MISSING}, 3);
    document_results_free(&results);

    CHECK(run("/doc-bulk", 0, 1, &results, &stats) == DOCUMENT_OK);
    CHECK(stats.pages_visited == 1 && stats.pages_failed == 0 &&
          stats.links_discovered == CRAWLER_MAX_CANDIDATES);
    CHECK(document_results_count(&results) == DOCUMENT_MAX_OCCURRENCES);
    CHECK(document_results_source_count(&results) == DOCUMENT_MAX_OCCURRENCES);
    request_order((unsigned[]){FIX_DOC_BULK}, 1);
    CHECK(strstr(document_results_at(&results, 0)->url, "document-0000.PDF?download=1") != NULL);
    CHECK(strstr(document_results_at(&results, 999)->url, "document-0999.PDF?download=1") != NULL);
    document_results_free(&results);

    crawler_fixture_stop();
    printf("PASS document integration: %zu checks; BFS provenance, redirects, scope, partial, 1000-result budget\n",
           checks);
    return 0;
}
#endif
