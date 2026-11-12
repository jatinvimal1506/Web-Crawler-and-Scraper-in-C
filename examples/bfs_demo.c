#include "project.h"
#include "http.h"
#include <stdio.h>
#include <string.h>


static int demo_depth(const char *text, size_t *depth)
{
    if (strcmp(text, "10") == 0) {
        *depth = 10;
        return 1;
    }
    if (text[0] >= '0' && text[0] <= '9' && text[1] == '\0') {
        *depth = (size_t)(text[0] - '0');
        return 1;
    }
    return 0;
}

int main(int argc, char **argv)
{
    CrawlerConfig config = {NULL, 1, 1, 3};
    CrawlerStats stats;
    CrawlerStatus status;
    if (argc < 2 || argc > 4 ||
        (argc >= 3 && !demo_depth(argv[2], &config.max_depth)) ||
        (argc == 4 && strcmp(argv[3], "0") != 0 && strcmp(argv[3], "1") != 0)) {
        fprintf(stderr, "Usage: %s CONTROLLED_URL [DEPTH 0..10] [SAME_DOMAIN 0|1]\n", argv[0]);
        return 2;
    }
    config.start_url = argv[1];
    if (argc == 4) { config.same_domain = argv[3][0] - '0'; }
    puts("DeepDownload-C Phase 7: validated single-threaded BFS (no file downloads)");
    puts("Use only controlled/permissioned sites. Port pinned; redirects checked before each request.");
    status = crawler_run(&config, &stats);
    printf("Status: %s\nPages visited: %zu\nPages failed: %zu\n"
           "Links discovered: %zu\nMaximum depth attempted: %zu\nBytes downloaded: %zu\n",
           crawler_status_string(status), stats.pages_visited, stats.pages_failed,
           stats.links_discovered, stats.max_depth_reached, stats.bytes_downloaded);
    puts("Limits: 100 page attempts, 1000 href candidates, 1000 scheduled/alias keys.");
    if (status != CRAWLER_OK) {
        puts("Crawl was not wholly successful/exhaustive; inspect status and partial stats.");
    }
    http_cleanup();
    return status == CRAWLER_OK ? 0 : 1;
}
