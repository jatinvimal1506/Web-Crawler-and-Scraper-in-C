#include "queue.h"
#include "visited.h"
#include "phase5_check.h"
#include <string.h>

/* Only queue/visited/url test objects redirect malloc/free here. */
static size_t allocations, fail_at, live;
void *phase5_test_malloc(size_t size);
void phase5_test_free(void *pointer);

void *phase5_test_malloc(size_t size)
{
    void *pointer;
    ++allocations;
    if (fail_at != 0 && allocations == fail_at) {
        return NULL;
    }
    pointer = malloc(size);
    CHECK(pointer != NULL); /* An unrelated real OOM must fail the test too. */
    ++live;
    return pointer;
}

void phase5_test_free(void *pointer)
{
    if (pointer != NULL) {
        CHECK(live != 0);
        --live;
    }
    free(pointer);
}

static void seed_queue(UrlQueue *queue)
{
    fail_at = 0;
    CHECK(queue_enqueue(queue, "A", 0) == QUEUE_OK);
    CHECK(queue_enqueue(queue, "B", 1) == QUEUE_OK);
}

static void drain_queue(UrlQueue *queue, int has_c)
{
    UrlQueueItem item = {0};
    const char *expected[] = {"A", "B", "C"};
    size_t i;
    for (i = 0; i < (has_c ? 3U : 2U); ++i) {
        CHECK(queue_dequeue(queue, &item) == QUEUE_OK);
        CHECK(strcmp(item.url, expected[i]) == 0 && item.depth == (int)i);
        queue_item_free(&item);
    }
    CHECK(queue_dequeue(queue, &item) == QUEUE_EMPTY);
    queue_free(queue);
    queue_free(queue);
    CHECK(live == 0);
}

static size_t test_queue_failures(void)
{
    UrlQueue queue = {0};
    size_t points, i;
    seed_queue(&queue);
    allocations = 0;
    CHECK(queue_enqueue(&queue, "C", 2) == QUEUE_OK);
    points = allocations;
    drain_queue(&queue, 1);
    CHECK(points == 2); /* Node and owned string. */
    for (i = 1; i <= points; ++i) {
        size_t baseline;
        seed_queue(&queue);
        baseline = live;
        allocations = 0;
        fail_at = i;
        CHECK(queue_enqueue(&queue, "C", 2) == QUEUE_ERROR_OUT_OF_MEMORY);
        CHECK(queue.count == 2 && live == baseline);
        fail_at = 0;
        drain_queue(&queue, 0); /* Existing A/B and depths/order unchanged. */
        /* A failure does not prevent a later successful enqueue. */
        CHECK(queue_enqueue(&queue, "retry", 2) == QUEUE_OK);
        queue_free(&queue);
        CHECK(live == 0);
    }
    /* Free an undrained queue under accounting, not just drained outputs. */
    seed_queue(&queue);
    queue_free(&queue);
    queue_free(&queue);
    CHECK(live == 0);
    return points;
}

static VisitedStatus insert_new(VisitedSet *set)
{
    return visited_insert(set, "https://example.com/new?x=1#top");
}
static VisitedStatus insert_duplicate(VisitedSet *set)
{
    return visited_insert(set, "HTTPS://EXAMPLE.COM:443/seed#other");
}
static VisitedStatus contains_new(VisitedSet *set)
{
    return visited_contains(set, "https://example.com/new?x=1#top");
}
static VisitedStatus contains_existing(VisitedSet *set)
{
    return visited_contains(set, "HTTPS://EXAMPLE.COM:443/seed#other");
}

static size_t test_visited_failures(VisitedStatus (*action)(VisitedSet *),
                                    VisitedStatus expected)
{
    VisitedSet set = {0};
    size_t points, i;
    fail_at = 0;
    CHECK(visited_insert(&set, "https://example.com/seed") == VISITED_ADDED);
    allocations = 0;
    CHECK(action(&set) == expected);
    points = allocations;
    CHECK(points > 0);
    visited_free(&set);
    CHECK(live == 0);
    for (i = 1; i <= points; ++i) {
        size_t baseline;
        CHECK(visited_insert(&set, "https://example.com/seed") == VISITED_ADDED);
        baseline = live;
        allocations = 0;
        fail_at = i;
        CHECK(action(&set) == VISITED_ERROR_OUT_OF_MEMORY);
        CHECK(live == baseline && set.count == 1);
        fail_at = 0;
        CHECK(visited_contains(&set, "https://example.com/seed") == VISITED_PRESENT);
        CHECK(visited_contains(&set, "https://example.com/new?x=1") == VISITED_NOT_FOUND);
        CHECK(action(&set) == expected); /* Successful retry on same object. */
        visited_free(&set);
        visited_free(&set);
        CHECK(live == 0);
    }
    return points;
}

int main(void)
{
    size_t queue_points = test_queue_failures();
    size_t insert_points = test_visited_failures(insert_new, VISITED_ADDED);
    size_t duplicate_points = test_visited_failures(insert_duplicate, VISITED_PRESENT);
    size_t missing_points = test_visited_failures(contains_new, VISITED_NOT_FOUND);
    size_t present_points = test_visited_failures(contains_existing, VISITED_PRESENT);
    CHECK(live == 0);
    printf("PASS Phase 5 allocation: %zu checks; failure points queue=%zu, "
           "insert=%zu, duplicate=%zu, contains-missing=%zu, contains-present=%zu; "
           "zero tracked allocations\n", checks, queue_points, insert_points,
           duplicate_points, missing_points, present_points);
    return 0;
}
