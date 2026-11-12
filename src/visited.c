#include "visited.h"
#include "url.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct VisitedNode {
    char *key;
    struct VisitedNode *next;
};

VisitedStatus visited_init(VisitedSet *set)
{
    if (set == NULL || set->head != NULL || set->count != 0) {
        return VISITED_ERROR_INVALID_ARGUMENT;
    }
    *set = (VisitedSet){0};
    return VISITED_OK;
}

VisitedStatus visited_size(const VisitedSet *set, size_t *size)
{
    if (set == NULL || size == NULL) {
        return VISITED_ERROR_INVALID_ARGUMENT;
    }
    *size = set->count;
    return VISITED_OK;
}

static VisitedStatus normalize_key(const char *url, char **key)
{
    UrlStatus status = url_normalize(url, key);
    if (status == URL_ERROR_OUT_OF_MEMORY) {
        return VISITED_ERROR_OUT_OF_MEMORY;
    }
    return status == URL_OK ? VISITED_OK : VISITED_ERROR_INVALID_URL;
}

static int has_key(const VisitedSet *set, const char *key)
{
    const VisitedNode *node;
    for (node = set->head; node != NULL; node = node->next) {
        if (strcmp(node->key, key) == 0) {
            return 1;
        }
    }
    return 0;
}

VisitedStatus visited_contains(const VisitedSet *set, const char *url)
{
    char *key = NULL;
    VisitedStatus status;
    int present;
    if (set == NULL || url == NULL) {
        return VISITED_ERROR_INVALID_ARGUMENT;
    }
    status = normalize_key(url, &key);
    if (status != VISITED_OK) {
        return status;
    }
    present = has_key(set, key);
    free(key);
    return present ? VISITED_PRESENT : VISITED_NOT_FOUND;
}

VisitedStatus visited_insert(VisitedSet *set, const char *url)
{
    char *key = NULL;
    VisitedNode *node;
    VisitedStatus status;
    if (set == NULL || url == NULL) {
        return VISITED_ERROR_INVALID_ARGUMENT;
    }
    status = normalize_key(url, &key);
    if (status != VISITED_OK) {
        return status;
    }
    if (has_key(set, key)) {
        free(key);
        return VISITED_PRESENT;
    }
    if (set->count == SIZE_MAX) {
        free(key);
        return VISITED_ERROR_OVERFLOW;
    }
    node = malloc(sizeof(*node));
    if (node == NULL) {
        free(key);
        return VISITED_ERROR_OUT_OF_MEMORY;
    }
    node->key = key;
    node->next = set->head;
    set->head = node;
    ++set->count;
    return VISITED_ADDED;
}

void visited_free(VisitedSet *set)
{
    if (set != NULL) {
        VisitedNode *node = set->head;
        while (node != NULL) {
            VisitedNode *next = node->next;
            free(node->key);
            free(node);
            node = next;
        }
        *set = (VisitedSet){0};
    }
}
