#ifndef DEEPDOWNLOAD_VISITED_H
#define DEEPDOWNLOAD_VISITED_H
#include <stddef.h>

typedef enum {
    VISITED_OK = 0,
    VISITED_ADDED,
    VISITED_PRESENT,
    VISITED_NOT_FOUND,
    VISITED_ERROR_INVALID_ARGUMENT,
    VISITED_ERROR_INVALID_URL,
    VISITED_ERROR_OUT_OF_MEMORY,
    VISITED_ERROR_OVERFLOW
} VisitedStatus;

typedef struct VisitedNode VisitedNode;
typedef struct {
    VisitedNode *head;
    size_t count;
} VisitedSet;
VisitedStatus visited_init(VisitedSet *set);
VisitedStatus visited_size(const VisitedSet *set, size_t *size);
VisitedStatus visited_contains(const VisitedSet *set, const char *url);//checks whether a normalized URL is already recorded
VisitedStatus visited_insert(VisitedSet *set, const char *url);// stores a new normalized URL.
void visited_free(VisitedSet *set);
#endif
