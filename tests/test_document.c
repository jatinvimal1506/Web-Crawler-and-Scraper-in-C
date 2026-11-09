#include "document.h"
#include "phase5_check.h"
#include <string.h>

static void classify(const char *url, DocumentStatus status, DocumentType type)
{
    DocumentType actual = DOCUMENT_TYPE_PDF;
    CHECK(document_type_from_url(url, &actual) == status);
    CHECK(actual == type);
}

int main(void)
{
    DocumentResults results = {0};
    CrawlerStats stats = {9, 9, 9, 9, 9};
    CrawlerConfig invalid = {NULL, 0, 1, 1};

    classify("https://example.edu/docs/report.pdf", DOCUMENT_OK, DOCUMENT_TYPE_PDF);
    classify("https://example.edu/docs/NOTES.DOCX?download=1", DOCUMENT_OK,
             DOCUMENT_TYPE_DOCX);
    classify("https://example.edu/files/archive.ZIP#part", DOCUMENT_OK,
             DOCUMENT_TYPE_ZIP);
    classify("https://example.edu/docs/report.pdf?download=1", DOCUMENT_OK,
             DOCUMENT_TYPE_PDF);
    classify("https://example.edu/report.pdfx", DOCUMENT_OK, DOCUMENT_TYPE_UNKNOWN);
    classify("https://example.edu/archive.zip.html", DOCUMENT_OK, DOCUMENT_TYPE_UNKNOWN);
    classify("https://example.edu/notes.doc", DOCUMENT_OK, DOCUMENT_TYPE_UNKNOWN);
    classify("https://example.edu/download?id=123", DOCUMENT_OK, DOCUMENT_TYPE_UNKNOWN);
    classify("https://example.edu/", DOCUMENT_OK, DOCUMENT_TYPE_UNKNOWN);
    classify("relative.pdf", DOCUMENT_ERROR_INVALID_URL, DOCUMENT_TYPE_UNKNOWN);
    classify("ftp://example.edu/report.pdf", DOCUMENT_ERROR_INVALID_URL,
             DOCUMENT_TYPE_UNKNOWN);
    classify("", DOCUMENT_ERROR_INVALID_ARGUMENT, DOCUMENT_TYPE_UNKNOWN);
    classify(NULL, DOCUMENT_ERROR_INVALID_ARGUMENT, DOCUMENT_TYPE_UNKNOWN);
    CHECK(document_type_from_url("https://example.edu/x.pdf", NULL) ==
          DOCUMENT_ERROR_INVALID_ARGUMENT);
    CHECK(document_results_init(NULL) == DOCUMENT_ERROR_INVALID_ARGUMENT);
    CHECK(document_results_init(&results) == DOCUMENT_OK);
    CHECK(document_results_count(&results) == 0);
    CHECK(document_results_source_count(&results) == 0);
    CHECK(document_results_at(&results, 0) == NULL);
    CHECK(document_discover(&invalid, &results, &stats) == DOCUMENT_ERROR_INVALID_ARGUMENT);
    CHECK(stats.pages_visited == 0 && stats.pages_failed == 0);
    CHECK(document_results_count(&results) == 0);
    CHECK(strcmp(document_type_name(DOCUMENT_TYPE_PDF), "PDF") == 0);
    CHECK(strcmp(document_type_name(DOCUMENT_TYPE_DOCX), "DOCX") == 0);
    CHECK(strcmp(document_type_name(DOCUMENT_TYPE_ZIP), "ZIP") == 0);
    CHECK(strcmp(document_type_name(DOCUMENT_TYPE_UNKNOWN), "UNKNOWN") == 0);
    CHECK(strcmp(document_status_message(DOCUMENT_LIMIT_REACHED),
                 "document/crawl budget reached (incomplete)") == 0);
    document_results_free(&results);
    document_results_free(&results);
    CHECK(document_results_init(&results) == DOCUMENT_OK);
    document_results_free(&results);
    printf("PASS document unit tests: %zu checks (suffix/query/case/invalid/init/access/cleanup)\n",
           checks);
    return 0;
}
