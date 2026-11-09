#include "document.h"
#include "crawler_fixture.h"
#include "phase5_check.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
int main(void)
{
    puts("SKIP document allocation fixture requires POSIX sockets; native Windows NOT VERIFIED.");
    return 0;
}
#else
static size_t allocation_calls, fail_at, live_allocations;

void *document_test_malloc(size_t size)
{
    void *result;
    if (++allocation_calls == fail_at) { return NULL; }
    result = malloc(size);
    CHECK(result != NULL);
    ++live_allocations;
    return result;
}

void *document_test_realloc(void *old, size_t size)
{
    void *result;
    CHECK(size != 0);
    if (++allocation_calls == fail_at) { return NULL; }
    result = realloc(old, size);
    CHECK(result != NULL);
    if (old == NULL) { ++live_allocations; }
    return result;
}

void document_test_free(void *pointer)
{
    if (pointer != NULL) {
        CHECK(live_allocations != 0);
        --live_allocations;
    }
    free(pointer);
}

int main(void)
{
    DocumentResults results = {0};
    CrawlerStats stats;
    CrawlerConfig config;
    char url[160], saved[160];
    size_t failure_points, i;
    unsigned discarded[32];
    unsigned port = crawler_fixture_start();
    (void)snprintf(url, sizeof(url), "http://127.0.0.1:%u/doc-home", port);
    memcpy(saved, url, strlen(url) + 1);
    config = (CrawlerConfig){url, 1, 1, 2};
    CHECK(document_results_init(&results) == DOCUMENT_OK);

    allocation_calls = 0;
    fail_at = 0;
    CHECK(document_discover(&config, &results, &stats) == DOCUMENT_OK);
    failure_points = allocation_calls;
    CHECK(failure_points != 0 && document_results_count(&results) == 4);
    CHECK(document_results_source_count(&results) == 6);
    CHECK(live_allocations != 0);
    CHECK(strcmp(url, saved) == 0);
    document_results_free(&results);
    CHECK(live_allocations == 0);
    (void)crawler_fixture_requests(discarded, 32);

    for (i = 1; i <= failure_points; ++i) {
        CHECK(document_results_init(&results) == DOCUMENT_OK);
        allocation_calls = 0;
        fail_at = i;
        CHECK(document_discover(&config, &results, &stats) ==
              DOCUMENT_ERROR_OUT_OF_MEMORY);
        CHECK(document_results_count(&results) == 0);
        CHECK(document_results_source_count(&results) == 0);
        CHECK(results.items == NULL && results.capacity == 0);
        CHECK(live_allocations == 0);
        CHECK(strcmp(url, saved) == 0);
        document_results_free(&results);
        (void)crawler_fixture_requests(discarded, 32);
    }

    fail_at = 0;
    allocation_calls = 0;
    CHECK(document_results_init(&results) == DOCUMENT_OK);
    CHECK(document_discover(&config, &results, &stats) == DOCUMENT_OK);
    CHECK(document_results_count(&results) == 4);
    document_results_free(&results);
    document_results_free(&results);
    CHECK(live_allocations == 0);
    crawler_fixture_stop();
    printf("PASS document allocation tests: %zu checks; %zu injected C-module allocation failures; rollback empty and retry clean\n",
           checks, failure_points);
    return 0;
}
#endif
