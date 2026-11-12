#include "queue.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct QueueNode {
    UrlQueueItem item;
    struct QueueNode *next;
};

QueueStatus queue_init(UrlQueue *queue)
{
    if (queue == NULL || queue->head != NULL || queue->tail != NULL ||
        queue->count != 0) {
        return QUEUE_ERROR_INVALID_ARGUMENT;
    }
    *queue = (UrlQueue){0};
    return QUEUE_OK;
}

QueueStatus queue_size(const UrlQueue *queue, size_t *size)
{
    if (queue == NULL || size == NULL) {
        return QUEUE_ERROR_INVALID_ARGUMENT;
    }
    *size = queue->count;
    return QUEUE_OK;
}

QueueStatus queue_is_empty(const UrlQueue *queue, int *empty)
{
    if (queue == NULL || empty == NULL) {
        return QUEUE_ERROR_INVALID_ARGUMENT;
    }
    *empty = queue->count == 0;
    return QUEUE_OK;
}

QueueStatus queue_enqueue(UrlQueue *queue, const char *url, int depth)
{
    QueueNode *node;
    size_t length;
    if (queue == NULL || url == NULL || *url == '\0' || depth < 0) {
        return QUEUE_ERROR_INVALID_ARGUMENT;
    }
    if (queue->count == SIZE_MAX) {
        return QUEUE_ERROR_OVERFLOW;
    }
    length = strlen(url);
    if (length == SIZE_MAX) {
        return QUEUE_ERROR_OVERFLOW;
    }
    node = malloc(sizeof(*node));
    if (node == NULL) {
        return QUEUE_ERROR_OUT_OF_MEMORY;
    }
    node->item.url = malloc(length + 1);
    if (node->item.url == NULL) {
        free(node);
        return QUEUE_ERROR_OUT_OF_MEMORY;
    }
    memcpy(node->item.url, url, length + 1);
    node->item.depth = depth;
    node->next = NULL;

    if (queue->tail != NULL) {
        queue->tail->next = node;
    } else {
        queue->head = node;
    }
    queue->tail = node;
    ++queue->count;
    return QUEUE_OK;
}

QueueStatus queue_dequeue(UrlQueue *queue, UrlQueueItem *item)
{
    QueueNode *node;
    if (queue == NULL || item == NULL || item->url != NULL) {
        return QUEUE_ERROR_INVALID_ARGUMENT;
    }
    if (queue->head == NULL) {
        return QUEUE_EMPTY;
    }
    node = queue->head;
    *item = node->item;
    queue->head = node->next;
    if (queue->head == NULL) {
        queue->tail = NULL;
    }
    --queue->count;
    free(node);
    return QUEUE_OK;
}

void queue_item_free(UrlQueueItem *item)
{
    if (item != NULL) {
        free(item->url);
        *item = (UrlQueueItem){0};
    }
}

void queue_free(UrlQueue *queue)
{
    if (queue != NULL) {
        QueueNode *node = queue->head;
        while (node != NULL) {
            QueueNode *next = node->next;
            queue_item_free(&node->item);
            free(node);
            node = next;
        }
        *queue = (UrlQueue){0};
    }
}
