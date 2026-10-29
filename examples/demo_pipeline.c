#include "http.h"
#include "project.h"
#include "url.h"

#include <stdio.h>
#include <string.h>

#define DEFAULT_URL "https://example.com"
#define DOWNLOAD_TIMEOUT_SECONDS 15L
#define HTML_PREVIEW_BYTES 300

static int is_network_failure(HttpStatus status)
{
    switch (status) {
    case HTTP_ERR_INIT:
    case HTTP_ERR_DNS:
    case HTTP_ERR_CONNECT:
    case HTTP_ERR_TIMEOUT:
    case HTTP_ERR_TLS:
    case HTTP_ERR_TOO_MANY_REDIRECTS:
    case HTTP_ERR_NETWORK:
        return 1;
    default:
        return 0;
    }
}

int main(int argc, char **argv)
{
    const char *url = DEFAULT_URL;
    CrawlerConfig config;
    CrawlerStats stats = {0};
    CrawlerStatus crawler_status;
    ParsedUrl parsed = {0};
    UrlStatus url_status;
    HttpResponse response = {0};
    HttpStatus http_status;
    int exit_code = 1;

    if (argc > 2) {
        fprintf(stderr, "Usage: %s [URL]\n", argv[0]);
        return 2;
    }
    if (argc == 2) {
        url = argv[1];
    }

    printf("==========================================\n");
    printf("     WEB CRAWLER & SCRAPER IN C\n");
    printf("          LIVE DEMO PIPELINE\n");
    printf("==========================================\n\n");

    /* ==========================================================
     * Phase 1: CLI and Public API
     * Demonstrates input argument handling and calling crawler_run()
     * ========================================================== */
    printf("[PHASE 1] CLI AND PUBLIC API\n\n");
    printf("Input URL: %s\n", url[0] != '\0' ? url : "(empty)");
    printf("Calling the existing project API (crawler_run)...\n");

    config.start_url = url;
    config.max_depth = 0;
    config.same_domain = 1;
    config.timeout_seconds = DOWNLOAD_TIMEOUT_SECONDS;
    crawler_status = crawler_run(&config, &stats);
    printf("Status   : %s\n", crawler_status_string(crawler_status));
    if (crawler_status == CRAWLER_NOT_IMPLEMENTED) {
        printf("Result   : API accepted configuration (crawl engine integration pending for Phase 6)\n");
    } else if (crawler_status != CRAWLER_OK) {
        printf("Result   : API rejected configuration (%s)\n",
               crawler_status_string(crawler_status));
    }

    /* ==========================================================
     * Phase 2: URL Parsing and Validation
     * Uses url_parse() to validate the URL and extract components
     * ========================================================== */
    printf("\n[PHASE 2] URL PARSING\n\n");
    url_status = url_parse(url, &parsed);
    if (url_status == URL_OK) {
        printf("Scheme : %s\n", parsed.scheme);
        printf("Host   : %s\n", parsed.host);
        if (parsed.port != NULL) {
            printf("Port   : %s\n", parsed.port);
        } else if (parsed.scheme != NULL && strcmp(parsed.scheme, "https") == 0) {
            printf("Port   : 443 (default for https)\n");
        } else if (parsed.scheme != NULL && strcmp(parsed.scheme, "http") == 0) {
            printf("Port   : 80 (default for http)\n");
        } else {
            printf("Port   : (not specified)\n");
        }
        printf("Path   : %s\n", parsed.path);
        printf("\nURL Parsing: SUCCESS\n");
    } else {
        printf("URL Parsing: FAILURE (%s)\n",
               url_status_message(url_status));
    }
    url_free(&parsed);

    /* ==========================================================
     * Phase 3: HTTP Downloader
     * Uses http_download() via libcurl to fetch the webpage
     * ========================================================== */
    printf("\n[PHASE 3] HTTP DOWNLOADER\n\n");
    printf("Downloading webpage...\n\n");
    http_status = http_download(url, DOWNLOAD_TIMEOUT_SECONDS, &response);

    if (response.status_code > 0) {
        printf("HTTP Status : %ld\n", response.status_code);
    } else {
        printf("HTTP Status : not received\n");
    }
    if (http_status == HTTP_OK) {
        printf("Content Size: %zu bytes\n", response.size);
        if (response.final_url != NULL) {
            printf("Final URL   : %s\n", response.final_url);
        }
        if (response.content_type != NULL) {
            printf("Content-Type: %s\n", response.content_type);
        }

        printf("\nHTML Preview:\n");
        if (response.size == 0) {
            printf("(empty response body)\n");
        } else {
            size_t preview_size = response.size < HTML_PREVIEW_BYTES
                ? response.size : HTML_PREVIEW_BYTES;
            (void)fwrite(response.body, 1, preview_size, stdout);
            if (response.body[preview_size - 1] != '\n') {
                putchar('\n');
            }
            if (preview_size < response.size) {
                printf("... (preview limited to %d bytes)\n",
                       HTML_PREVIEW_BYTES);
            }
        }
        printf("\nDownload: SUCCESS\n");
        exit_code = 0;
    } else if (http_status == HTTP_ERR_HTTP_STATUS) {
        printf("\nDownload: HTTP FAILURE (%s)\n",
               http_status_string(http_status));
        if (response.detail[0] != '\0') {
            printf("Details: %s\n", response.detail);
        }
    } else if (is_network_failure(http_status)) {
        printf("\nDownload: NETWORK FAILURE (%s)\n",
               http_status_string(http_status));
        if (response.detail[0] != '\0') {
            printf("Details: %s\n", response.detail);
        }
    } else {
        printf("\nDownload: FAILURE (%s)\n", http_status_string(http_status));
        if (response.detail[0] != '\0') {
            printf("Details: %s\n", response.detail);
        }
    }

    http_response_free(&response);
    http_cleanup();

    printf("\n==========================================\n");
    printf("            DEMO COMPLETED\n");
    printf("==========================================\n");
    return exit_code;
}
