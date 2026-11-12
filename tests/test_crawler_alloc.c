#include "project.h"
#include "queue.h"
#include "visited.h"
#include "phase5_check.h"
#include "crawler_stub.h"
#include <limits.h>
#include <stdint.h>

static size_t attempts, fail_at, live;
static size_t queue_failures, visited_failures, cross_failures;
static int just_enqueued, forced_depth, force_depth, force_dequeue_error;
void *crawler_test_malloc(size_t size);
void *crawler_test_realloc(void *pointer, size_t size);
void *crawler_test_calloc(size_t count, size_t size);
void crawler_test_free(void *pointer);
QueueStatus crawler_test_enqueue(UrlQueue *, const char *, int);
QueueStatus crawler_test_dequeue(UrlQueue *, UrlQueueItem *);
VisitedStatus crawler_test_insert(VisitedSet *, const char *);

void *crawler_test_malloc(size_t size)
{
    void *pointer;
    if (++attempts == fail_at) {
        return NULL;
    }
    pointer = malloc(size);
    CHECK(pointer != NULL);
    ++live;
    return pointer;
}
void *crawler_test_realloc(void *pointer, size_t size)
{
    int was_null = pointer == NULL;
    void *grown;
    CHECK(size != 0);
    if (++attempts == fail_at) {
        return NULL;
    }
    grown = realloc(pointer, size);
    CHECK(grown != NULL);
    if (was_null) {
        ++live;
    }
    return grown;
}
void *crawler_test_calloc(size_t count, size_t size)
{
    void *pointer;
    if (++attempts == fail_at) {
        return NULL;
    }
    pointer = calloc(count, size);
    CHECK(pointer != NULL);
    ++live;
    return pointer;
}
void crawler_test_free(void *pointer)
{
    if (pointer != NULL) {
        CHECK(live != 0);
        --live;
    }
    free(pointer);
}
QueueStatus crawler_test_enqueue(UrlQueue *queue, const char *url, int depth)
{
    QueueStatus status = queue_enqueue(queue, url, depth);
    just_enqueued = status == QUEUE_OK;
    if (status == QUEUE_ERROR_OUT_OF_MEMORY) {
        ++queue_failures;
    }
    return status;
}
VisitedStatus crawler_test_insert(VisitedSet *set, const char *url)
{
    VisitedStatus status = visited_insert(set, url);
    if (status == VISITED_ERROR_OUT_OF_MEMORY) {
        ++visited_failures;
        if (just_enqueued) {
            ++cross_failures;
        }
    }
    just_enqueued = 0;
    return status;
}
QueueStatus crawler_test_dequeue(UrlQueue *queue, UrlQueueItem *item)
{
    QueueStatus status;
    if (force_dequeue_error) {
        return QUEUE_ERROR_INVALID_ARGUMENT;
    }
    status = queue_dequeue(queue, item);
    if (status == QUEUE_OK && force_depth) {
        item->depth = forced_depth;
    }
    return status;
}

int main(void)
{
    CrawlerConfig config = {"HTTPS://EXAMPLE.TEST:443/root#seed", 2, 1, 1};
    CrawlerStats stats;
    size_t points, i;
    attempts = 0;
    CHECK(crawler_run(&config, &stats) == CRAWLER_OK);
    points = attempts;
    CHECK(stats.pages_visited == 3 && stats.pages_failed == 0);
    CHECK(stats.links_discovered == 4 && stats.max_depth_reached == 1);
    CHECK(crawler_stub_calls == 3 && live == 0);
    for (i = 1; i <= points; ++i) {
        attempts = 0;
        fail_at = i;
        just_enqueued = 0;
        CHECK(crawler_run(&config, &stats) == CRAWLER_OUT_OF_MEMORY);
        CHECK(live == 0);
        CHECK(stats.pages_visited + stats.pages_failed <= CRAWLER_MAX_PAGES);
        CHECK(stats.links_discovered <= CRAWLER_MAX_CANDIDATES);
    }
    CHECK(queue_failures > 0 && visited_failures > 0 && cross_failures > 0);
    fail_at = 0;
    crawler_stub_calls = 0;
    CHECK(crawler_run(&config, &stats) == CRAWLER_OK && live == 0);
    CHECK(crawler_stub_calls == 3);
    force_depth = 1;
    forced_depth = INT_MAX;
    config.max_depth = SIZE_MAX;
    CHECK(crawler_run(&config, &stats) == CRAWLER_LIMIT_REACHED);
    CHECK(stats.max_depth_reached == (size_t)INT_MAX && live == 0);
    forced_depth = -1;
    CHECK(crawler_run(&config, &stats) == CRAWLER_INTERNAL_ERROR && live == 0);
    CHECK(stats.pages_visited == 0 && stats.pages_failed == 0);
    force_depth = 0;
    force_dequeue_error = 1;
    CHECK(crawler_run(&config, &stats) == CRAWLER_INTERNAL_ERROR && live == 0);
    force_dequeue_error = 0;
    crawler_stub_mode = 1;
    CHECK(crawler_run(&config, &stats) == CRAWLER_LIMIT_REACHED && live == 0);
    CHECK(stats.pages_visited == 1 && stats.pages_failed == 0);
    crawler_stub_mode = 2;
    CHECK(crawler_run(&config, &stats) == CRAWLER_LIMIT_REACHED && live == 0);
    CHECK(stats.pages_visited == 0 && stats.pages_failed == 1);
    printf("PASS crawler allocation: %zu checks; %zu injected positions; "
           "queue OOM=%zu, visited OOM=%zu, post-enqueue OOM=%zu; "
           "INT_MAX/invariant/budget cleanup; zero tracked allocations\n",
           checks, points, queue_failures, visited_failures, cross_failures);
    return 0;
}
