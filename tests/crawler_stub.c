/* In-memory HTTP fixture for the basic API and exhaustive engine OOM tests.
 * This is NEVER archived into the production library. */
#include "http.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "crawler_stub.h"

int crawler_stub_mode;
size_t crawler_stub_calls;

static char *copy_text(const char *text)
{
    size_t size = strlen(text) + 1;
    char *copy = malloc(size);
    if (copy != NULL) {
        memcpy(copy, text, size);
    }
    return copy;
}

void http_response_free(HttpResponse *response)
{
    if (response != NULL) {
        free(response->body);
        free(response->final_url);
        free(response->content_type);
        *response = (HttpResponse){0};
    }
}

HttpStatus http_download(const char *url, long timeout, HttpResponse *response)
{
    const char *body = "";
    (void)timeout;
    ++crawler_stub_calls;
    *response = (HttpResponse){0};
    if (strcmp(url, "https://example.test/root") == 0) {
        body = "<a href='/a'>a</a><a href='/b'>b</a>"
               "<a href='HTTPS://EXAMPLE.TEST:443/root#loop'>root</a>";
    } else if (strcmp(url, "https://example.test/a") == 0) {
        body = "<a href='/b#dup'>b</a>";
    }
    if (crawler_stub_mode != 0) {
        size_t i, used = 0, count = crawler_stub_mode == 1 ? 1000 : 1001;
        response->body = malloc(40000);
        if (response->body == NULL) {
            return HTTP_ERR_OUT_OF_MEMORY;
        }
        for (i = 0; i < count; ++i) {
            int written = snprintf(response->body + used, 40000 - used,
                                   "<a href='/candidate/%zu'>x</a>", i);
            if (written < 0 || (size_t)written >= 40000 - used) {
                http_response_free(response);
                return HTTP_ERR_NETWORK;
            }
            used += (size_t)written;
        }
    } else {
        response->body = copy_text(body);
    }
    response->final_url = copy_text(url);
    response->content_type = copy_text("text/html");
    if (response->body == NULL || response->final_url == NULL ||
        response->content_type == NULL) {
        http_response_free(response);
        return HTTP_ERR_OUT_OF_MEMORY;
    }
    response->size = strlen(response->body);
    response->status_code = 200;
    return HTTP_OK;
}
