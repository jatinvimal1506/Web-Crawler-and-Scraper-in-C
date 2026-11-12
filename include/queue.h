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
    char *url;
    int depth;
} UrlQueueItem;
typedef struct QueueNode QueueNode;
typedef struct {
    QueueNode *head;
    QueueNode *tail;
    size_t count;
} UrlQueue;
QueueStatus queue_init(UrlQueue *queue);
QueueStatus queue_size(const UrlQueue *queue, size_t *size);
QueueStatus queue_is_empty(const UrlQueue *queue, int *empty);
QueueStatus queue_enqueue(UrlQueue *queue, const char *url, int depth);
QueueStatus queue_dequeue(UrlQueue *queue, UrlQueueItem *item);
void queue_item_free(UrlQueueItem *item);
void queue_free(UrlQueue *queue);
#endif
