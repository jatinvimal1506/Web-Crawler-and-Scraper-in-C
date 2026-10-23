
#include "url.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>

int main(void)
{
    ParsedUrl url = {0};
    int status = url_parse(
        "https://example.com/products",
        &url
    );
    assert(status == 0);
    assert(strcmp(url.scheme, "https") == 0);
    assert(strcmp(url.host, "example.com") == 0);
    assert(strcmp(url.path, "/products") == 0);
    printf("Scheme: %s\n", url.scheme);
    printf("Host:   %s\n", url.host);
    printf("Path:   %s\n", url.path);
    url_free(&url);
    printf("URL parsing test passed.\n");
    return 0;
}
