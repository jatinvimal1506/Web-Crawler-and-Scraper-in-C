#include "http.h"
#include <stdio.h>

int main(int argc, char **argv)
{
    if (argc < 2) {
        printf("Usage: %s <URL>\n", argv[0]);
        return 1;
    }

    const char *url = argv[1];
    HttpResponse resp = {0};

    printf("Downloading: %s ...\n", url);
    HttpStatus status = http_download(url, 10, &resp);

    printf("Status:       %d (%s)\n", status, http_status_string(status));
    printf("HTTP Code:    %ld\n", resp.status_code);
    printf("Final URL:    %s\n", resp.final_url ? resp.final_url : "(null)");
    printf("Content-Type: %s\n", resp.content_type ? resp.content_type : "(null)");
    printf("Bytes:        %zu\n", resp.size);

    if (status == HTTP_OK && resp.body != NULL) {
        printf("\n--- First 300 characters of HTML ---\n");
        printf("%.300s\n...\n", resp.body);
    } else {
        printf("Error detail: %s\n", resp.detail);
    }

    http_response_free(&resp);
    http_cleanup();
    return (status == HTTP_OK) ? 0 : 1;
}
