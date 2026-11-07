#ifndef PHASE5_CHECK_H
#define PHASE5_CHECK_H
#include <stdio.h>
#include <stdlib.h>
static size_t checks;
/* Unlike assert(), checks and function calls remain active with NDEBUG. */
#define CHECK(condition) do { \
    ++checks; \
    if (!(condition)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        exit(EXIT_FAILURE); \
    } \
} while (0)
#endif
