#include "visited.h"
#include "phase5_check.h"
#include <stdint.h>
#include <string.h>

int main(void)
{
    VisitedSet set = {0};
    size_t size = 99, i;
    char owned[] = "HTTPS://Example.COM:443/docs#top";
    const char *bad[] = {"", "/relative", "//example.com/x", "javascript:x",
                         "mailto:a@b.com", "ftp://example.com/x", "https://",
                         "https://example.com/bad%ZZ"};
    const char *distinct[] = {
        "https://example.com/other", "https://example.com/docs/",
        "https://example.com/docs?x=1", "https://example.com/docs?x=2",
        "https://example.com/docs?x=1&y=2",
        "https://example.com/docs?y=2&x=1",
        "http://example.com/docs", "https://example.com/%7E",
        "https://example.com/~"
    };
    CHECK(visited_init(&set) == VISITED_OK);
    CHECK(visited_size(&set, &size) == VISITED_OK && size == 0);
    CHECK(visited_contains(&set, "https://example.com/docs") == VISITED_NOT_FOUND);
    CHECK(visited_init(NULL) == VISITED_ERROR_INVALID_ARGUMENT);
    CHECK(visited_size(NULL, &size) == VISITED_ERROR_INVALID_ARGUMENT);
    CHECK(visited_size(&set, NULL) == VISITED_ERROR_INVALID_ARGUMENT);
    CHECK(visited_insert(NULL, owned) == VISITED_ERROR_INVALID_ARGUMENT);
    CHECK(visited_contains(NULL, owned) == VISITED_ERROR_INVALID_ARGUMENT);
    CHECK(visited_insert(&set, NULL) == VISITED_ERROR_INVALID_ARGUMENT);
    CHECK(visited_contains(&set, NULL) == VISITED_ERROR_INVALID_ARGUMENT);
    for (i = 0; i < sizeof(bad) / sizeof(bad[0]); ++i) {
        CHECK(visited_insert(&set, bad[i]) == VISITED_ERROR_INVALID_URL);
        CHECK(visited_contains(&set, bad[i]) == VISITED_ERROR_INVALID_URL);
    }
    {
        char too_long[2200];
        memset(too_long, 'a', sizeof(too_long));
        memcpy(too_long, "https://example.com/", 20);
        too_long[sizeof(too_long) - 1] = '\0';
        CHECK(visited_insert(&set, too_long) == VISITED_ERROR_INVALID_URL);
    }
    CHECK(visited_size(&set, &size) == VISITED_OK && size == 0);
    CHECK(visited_insert(&set, owned) == VISITED_ADDED);
    CHECK(strcmp(owned, "HTTPS://Example.COM:443/docs#top") == 0);
    owned[8] = 'X';
    CHECK(visited_contains(&set, "https://example.com/docs") == VISITED_PRESENT);
    CHECK(visited_insert(&set, "https://EXAMPLE.COM:443/docs#other") == VISITED_PRESENT);
    CHECK(visited_size(&set, &size) == VISITED_OK && size == 1);
    CHECK(visited_init(&set) == VISITED_ERROR_INVALID_ARGUMENT);
    CHECK(visited_insert(&set, "HTTP://EXAMPLE.COM:80/plain#top") == VISITED_ADDED);
    CHECK(visited_contains(&set, "http://example.com/plain") == VISITED_PRESENT);
    for (i = 0; i < sizeof(distinct) / sizeof(distinct[0]); ++i) {
        CHECK(visited_contains(&set, distinct[i]) == VISITED_NOT_FOUND);
        CHECK(visited_insert(&set, distinct[i]) == VISITED_ADDED);
        CHECK(visited_insert(&set, distinct[i]) == VISITED_PRESENT);
    }
    CHECK(visited_size(&set, &size) == VISITED_OK && size == 11);
    visited_free(&set);
    visited_free(&set);
    visited_free(NULL);
    CHECK(visited_init(&set) == VISITED_OK);
    for (i = 0; i < 1200; ++i) {
        char url[80];
        (void)snprintf(url, sizeof(url), "https://example.com/item/%zu", i);
        CHECK(visited_insert(&set, url) == VISITED_ADDED);
    }
    CHECK(visited_size(&set, &size) == VISITED_OK && size == 1200);
    for (i = 0; i < 1200; ++i) {
        char url[80];
        (void)snprintf(url, sizeof(url), "HTTPS://EXAMPLE.COM:443/item/%zu#x", i);
        CHECK(visited_contains(&set, url) == VISITED_PRESENT);
        CHECK(visited_insert(&set, url) == VISITED_PRESENT);
    }
    CHECK(visited_size(&set, &size) == VISITED_OK && size == 1200);
    visited_free(&set);

    set.count = SIZE_MAX;
    CHECK(visited_insert(&set, "https://example.com/overflow") == VISITED_ERROR_OVERFLOW);
    CHECK(set.head == NULL && set.count == SIZE_MAX);
    set.count = 0;
    CHECK(visited_insert(&set, "https://example.com/reused") == VISITED_ADDED);
    visited_free(&set);
    printf("PASS visited: %zu checks, 1200 canonical keys, equivalence/distinctions/ownership\n", checks);
    return 0;
}
