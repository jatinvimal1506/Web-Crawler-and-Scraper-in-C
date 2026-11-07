#ifndef DEEPDOWNLOAD_VISITED_H
#define DEEPDOWNLOAD_VISITED_H
#include <stddef.h>

typedef enum {
    VISITED_OK = 0, /* init/size success */
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

/*
 * Start with VisitedSet set = {0}; fields are private-by-convention.
 * Do not shallow-copy populated sets. Init rejects nonempty sets. Free is
 * NULL-safe, repeatable and resets for reuse. The caller owns the container.
 */
VisitedStatus visited_init(VisitedSet *set);
VisitedStatus visited_size(const VisitedSet *set, size_t *size);

/*
 * Absolute HTTP(S) strings only; input is borrowed/unchanged. url_normalize()
 * supplies canonical keys, including case/default-port/fragment behavior.
 * No new percent, slash or query-order rules are invented here.
 *
 * contains: PRESENT / NOT_FOUND; insert: ADDED / PRESENT. All other statuses
 * are failures, NEVER duplicates. NULL is INVALID_ARGUMENT; malformed,
 * relative, unsupported or too-long URLs are INVALID_URL. URL-normalizer
 * allocation failures propagate as OUT_OF_MEMORY.
 *
 * A singly linked list deliberately favors clarity over hash-table scale:
 * normalization O(L), membership/insertion O(n*L) worst case. Contains may
 * allocate a temporary normalized key; insert owns its canonical string.
 * No pointers into the set escape. Duplicate/failure never changes the set.
 */
VisitedStatus visited_contains(const VisitedSet *set, const char *url);
VisitedStatus visited_insert(VisitedSet *set, const char *url);
void visited_free(VisitedSet *set); /* O(n) nodes plus their owned strings. */
#endif
