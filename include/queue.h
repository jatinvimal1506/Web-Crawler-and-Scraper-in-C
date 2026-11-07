#ifndef DEEPDOWNLOAD_QUEUE_H
#define DEEPDOWNLOAD_QUEUE_H
#include <stddef.h>

typedef enum {
    QUEUE_OK = 0,
    QUEUE_ERROR_INVALID_ARGUMENT,
    QUEUE_EMPTY,
    QUEUE_ERROR_OUT_OF_MEMORY,
    QUEUE_ERROR_OVERFLOW
} QueueStatus;

typedef struct {
    char *url; /* Owned after dequeue; release with queue_item_free(). */
    int depth; /* Nonnegative; INT_MAX is supported. */
} UrlQueueItem;

typedef struct QueueNode QueueNode;
typedef struct {
    QueueNode *head;
    QueueNode *tail;
    size_t count;
} UrlQueue;

/*
 * Start with UrlQueue queue = {0}. Do not copy a populated queue by value or
 * modify its fields. Init rejects populated objects rather than leaking them.
 * No heap allocation is needed to initialize. Free resets for safe reuse.
 */
QueueStatus queue_init(UrlQueue *queue);
QueueStatus queue_size(const UrlQueue *queue, size_t *size);
QueueStatus queue_is_empty(const UrlQueue *queue, int *empty);

/*
 * Copy a borrowed, nonempty NUL-terminated string and associate its depth.
 * This container does not validate/normalize URL syntax: callers use url.h.
 * NULL inputs, empty strings and negative depths are invalid. Allocation or
 * count overflow leaves all existing items/order untouched. No fixed bound.
 * Linked-list linking is O(1); copying a URL of length L makes enqueue O(L).
 */
QueueStatus queue_enqueue(UrlQueue *queue, const char *url, int depth);

/*
 * O(1) FIFO removal. Initialize item as {0} (or free its last result first).
 * Transfers the owned string to item WITHOUT copying or freeing the string.
 * QUEUE_EMPTY and other failures leave queue/item unchanged. A populated
 * output is rejected. The caller owns the returned URL even after queue_free.
 */
QueueStatus queue_dequeue(UrlQueue *queue, UrlQueueItem *item);

/* NULL-safe, repeated cleanup is safe; resets all fields. O(1) / O(n). */
void queue_item_free(UrlQueueItem *item);
void queue_free(UrlQueue *queue);
#endif
