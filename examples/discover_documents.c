#include "document.h"
#include "http.h"
#include <stdio.h>
#include <string.h>

static int parse_depth(const char *text, size_t *depth)
{
    size_t value = 0, i;
    if (text == NULL || text[0] == '\0') { return 0; }
    for (i = 0; text[i] != '\0'; ++i) {
        unsigned digit;
        if (text[i] < '0' || text[i] > '9') { return 0; }
        digit = (unsigned)(text[i] - '0');
        if (value > (10U - digit) / 10U) { return 0; }
        value = value * 10U + digit;
        if (value > 10) { return 0; }
    }
    *depth = value;
    return 1;
}

int main(int argc, char **argv)
{
    CrawlerConfig config = {NULL, 1, 1, 3};
    CrawlerStats stats = {0};
    DocumentResults results = {0};
    DocumentStatus status;
    size_t i, pdf = 0, docx = 0, zip = 0;
    if (argc < 2 || argc > 4 ||
        (argc >= 3 && !parse_depth(argv[2], &config.max_depth)) ||
        (argc == 4 && strcmp(argv[3], "0") != 0 && strcmp(argv[3], "1") != 0)) {
        fprintf(stderr, "Usage: %s CONTROLLED_URL [DEPTH 0..10] [SAME_DOMAIN 0|1]\n",
                argv[0]);
        return 2;
    }
    config.start_url = argv[1];
    if (argc == 4) { config.same_domain = argv[3][0] - '0'; }
    if (document_results_init(&results) != DOCUMENT_OK) { return 2; }

    status = document_discover(&config, &results, &stats);
    for (i = 0; i < document_results_count(&results); ++i) {
        const DocumentRecord *record = document_results_at(&results, i);
        if (record->type == DOCUMENT_TYPE_PDF) { ++pdf; }
        else if (record->type == DOCUMENT_TYPE_DOCX) { ++docx; }
        else if (record->type == DOCUMENT_TYPE_ZIP) { ++zip; }
    }
    printf("Document discovery: %s\n"
           "HTML pages visited: %zu\nHTML pages failed: %zu\n"
           "HTML links discovered: %zu\nMaximum depth attempted: %zu\n"
           "HTML bytes downloaded: %zu\nUnique document URLs: %zu\n"
           "Source-document relationships: %zu\nPDF: %zu  DOCX: %zu  ZIP: %zu\n",
           document_status_message(status), stats.pages_visited, stats.pages_failed,
           stats.links_discovered, stats.max_depth_reached, stats.bytes_downloaded,
           document_results_count(&results), document_results_source_count(&results),
           pdf, docx, zip);

    for (i = 0; i < document_results_count(&results); ++i) {
        const DocumentRecord *record = document_results_at(&results, i);
        size_t j;
        printf("\n%s [%s]\n", record->url, document_type_name(record->type));
        for (j = 0; j < record->source_count; ++j) {
            printf("  source depth %zu: %s\n", record->sources[j].depth,
                   record->sources[j].page_url);
        }
    }
    if (status != DOCUMENT_OK) {
        puts("\nResults may be partial or budget-limited; inspect the status.");
    }
    document_results_free(&results);
    document_results_free(&results);
    http_cleanup();
    return status == DOCUMENT_OK ? 0 : 1;
}
