





#include "html.h"

#include <assert.h>
#include <stddef.h>
#include <stdio.h>

void *__real_malloc(size_t size);
void *__real_realloc(void *pointer, size_t size);
void __real_free(void *pointer);
void *__wrap_malloc(size_t size);
void *__wrap_realloc(void *pointer, size_t size);
void __wrap_free(void *pointer);

static size_t allocation_count;
static size_t fail_at;
static size_t live_allocations;

void *__wrap_malloc(size_t size)
{
    void *pointer;
    ++allocation_count;
    pointer = allocation_count == fail_at ? NULL : __real_malloc(size);
    if (pointer != NULL) {
        ++live_allocations;
    }
    return pointer;
}

void *__wrap_realloc(void *pointer, size_t size)
{
    int was_null = pointer == NULL;
    void *grown;
    ++allocation_count;

    assert(size > 0);
    grown = allocation_count == fail_at ? NULL : __real_realloc(pointer, size);
    if (grown != NULL && was_null) {
        ++live_allocations;
    }
    return grown;
}

void __wrap_free(void *pointer)
{
    if (pointer != NULL) {
        assert(live_allocations > 0);
        --live_allocations;
    }
    __real_free(pointer);
}

int main(void)
{
    const char *input =
        "<a href=/1&#63;q=one&amp;x=two><a href='/2'><a href='/3'><a href='/4'>"
        "<a href='/5'><a href='/6'><a href='/7'><a href='/8'>"
        "<a href='/9'><a href='https://other.test/10'>";
    const char *base = "https://example.com/dir/page.html";
    HtmlLinks links = {0};
    size_t total, i;
    assert(html_extract_links(input, base, &links) == HTML_OK);
    assert(links.count == 10);
    total = allocation_count;
    html_links_free(&links);
    assert(live_allocations == 0);

    for (i = 1; i <= total; ++i) {
        allocation_count = 0;
        fail_at = i;
        assert(html_extract_links(input, base, &links) ==
               HTML_ERROR_OUT_OF_MEMORY);
        assert(links.urls == NULL && links.count == 0);
        html_links_free(&links);
        assert(live_allocations == 0);
    }
    fail_at = 0;
    assert(html_extract_links(input, base, &links) == HTML_OK);
    assert(links.count == 10);
    html_links_free(&links);
    assert(live_allocations == 0);
    printf("PASS: all %zu allocation failure points leave empty output "
           "and no tracked allocations\n", total);
    return 0;
}
