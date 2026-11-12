#include "queue.h"
#include "phase5_check.h"
#include <limits.h>
#include <stdint.h>
#include <string.h>

int main(void)
{
    UrlQueue queue = {0};
    UrlQueueItem item = {0};
    size_t size = 99, i;
    int empty = 0;
    char input[] = "https://example.com/owned";
    const char *urls[] = {"A", "B", "C", "large"};
    const int depths[] = {0, 1, 2, INT_MAX};
    CHECK(queue_init(&queue) == QUEUE_OK);
    CHECK(queue_size(&queue, &size) == QUEUE_OK && size == 0);
    CHECK(queue_is_empty(&queue, &empty) == QUEUE_OK && empty);
    CHECK(queue_dequeue(&queue, &item) == QUEUE_EMPTY);
    CHECK(item.url == NULL && item.depth == 0);
    CHECK(queue_init(NULL) == QUEUE_ERROR_INVALID_ARGUMENT);
    CHECK(queue_size(NULL, &size) == QUEUE_ERROR_INVALID_ARGUMENT);
    CHECK(queue_size(&queue, NULL) == QUEUE_ERROR_INVALID_ARGUMENT);
    CHECK(queue_is_empty(NULL, &empty) == QUEUE_ERROR_INVALID_ARGUMENT);
    CHECK(queue_is_empty(&queue, NULL) == QUEUE_ERROR_INVALID_ARGUMENT);
    CHECK(queue_enqueue(NULL, "A", 0) == QUEUE_ERROR_INVALID_ARGUMENT);
    CHECK(queue_enqueue(&queue, NULL, 0) == QUEUE_ERROR_INVALID_ARGUMENT);
    CHECK(queue_enqueue(&queue, "", 0) == QUEUE_ERROR_INVALID_ARGUMENT);
    CHECK(queue_enqueue(&queue, "A", -1) == QUEUE_ERROR_INVALID_ARGUMENT);
    CHECK(queue_dequeue(NULL, &item) == QUEUE_ERROR_INVALID_ARGUMENT);
    CHECK(queue_dequeue(&queue, NULL) == QUEUE_ERROR_INVALID_ARGUMENT);
    for (i = 0; i < 4; ++i) {
        CHECK(queue_enqueue(&queue, urls[i], depths[i]) == QUEUE_OK);
        CHECK(queue_size(&queue, &size) == QUEUE_OK && size == i + 1);
    }
    CHECK(queue_init(&queue) == QUEUE_ERROR_INVALID_ARGUMENT);
    CHECK(queue_enqueue(&queue, "bad", INT_MIN) == QUEUE_ERROR_INVALID_ARGUMENT);
    CHECK(queue_enqueue(&queue, NULL, 2) == QUEUE_ERROR_INVALID_ARGUMENT);
    CHECK(queue_size(&queue, &size) == QUEUE_OK && size == 4);
    CHECK(queue_is_empty(&queue, &empty) == QUEUE_OK && !empty);
    for (i = 0; i < 4; ++i) {
        CHECK(queue_dequeue(&queue, &item) == QUEUE_OK);
        CHECK(strcmp(item.url, urls[i]) == 0 && item.depth == depths[i]);
        CHECK(queue_dequeue(&queue, &item) == QUEUE_ERROR_INVALID_ARGUMENT);
        CHECK(queue_size(&queue, &size) == QUEUE_OK && size == 3 - i);
        queue_item_free(&item);
        queue_item_free(&item);
        CHECK(item.url == NULL && item.depth == 0);
    }
    CHECK(queue.head == NULL && queue.tail == NULL);
    CHECK(queue_enqueue(&queue, input, 12345) == QUEUE_OK);
    CHECK(strcmp(input, "https://example.com/owned") == 0);
    input[8] = 'X';
    CHECK(queue_dequeue(&queue, &item) == QUEUE_OK);
    CHECK(item.url != input);
    CHECK(strcmp(item.url, "https://example.com/owned") == 0);
    CHECK(item.depth == 12345);
    queue_free(&queue);
    CHECK(strcmp(item.url, "https://example.com/owned") == 0);
    queue_item_free(&item);


    for (i = 0; i < 1200; ++i) {
        char url[80];
        (void)snprintf(url, sizeof(url), "https://example.com/%zu", i);
        CHECK(queue_enqueue(&queue, url, (int)i) == QUEUE_OK);
    }
    CHECK(queue_size(&queue, &size) == QUEUE_OK && size == 1200);
    for (i = 0; i < 1200; ++i) {
        char expected[80];
        (void)snprintf(expected, sizeof(expected), "https://example.com/%zu", i);
        CHECK(queue_dequeue(&queue, &item) == QUEUE_OK);
        CHECK(strcmp(item.url, expected) == 0 && item.depth == (int)i);
        queue_item_free(&item);
        if (i == 0) {
            CHECK(queue_enqueue(&queue, "last", INT_MAX) == QUEUE_OK);
        }
    }
    CHECK(queue_dequeue(&queue, &item) == QUEUE_OK);
    CHECK(strcmp(item.url, "last") == 0 && item.depth == INT_MAX);
    queue_item_free(&item);
    CHECK(queue_dequeue(&queue, &item) == QUEUE_EMPTY);
    CHECK(queue_enqueue(&queue, "undrained", 2) == QUEUE_OK);
    queue_free(&queue);
    queue_free(&queue);
    queue_free(NULL);
    queue_item_free(NULL);
    CHECK(queue_init(&queue) == QUEUE_OK);

    queue.count = SIZE_MAX;
    CHECK(queue_enqueue(&queue, "overflow", 0) == QUEUE_ERROR_OVERFLOW);
    CHECK(queue.head == NULL && queue.tail == NULL && queue.count == SIZE_MAX);
    queue.count = 0;
    CHECK(queue_enqueue(&queue, "reused", 0) == QUEUE_OK);
    queue_free(&queue);
    printf("PASS queue: %zu checks, 1200 pending URLs, FIFO/depth/ownership/boundaries\n", checks);
    return 0;
}
