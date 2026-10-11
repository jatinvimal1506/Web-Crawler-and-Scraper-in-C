#include "project.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(const char *program)
{
    fprintf(stderr, "Usage: %s START_URL [MAX_DEPTH]\n", program);
}

int main(int argc, char **argv)
{
    CrawlerConfig config;
    CrawlerStats stats;
    CrawlerStatus status;

    if (argc < 2 || argc > 3) {
        usage(argv[0]);
        return 2;
    }
    config.start_url = argv[1];
    config.max_depth = argc == 3 ? (size_t)strtoul(argv[2], NULL, 10) : 1u;
    config.same_domain = 1;
    config.timeout_seconds = 15;

    status = crawler_run(&config, &stats);
    if (status != CRAWLER_OK) {
        fprintf(stderr, "Crawl failed: %s\n", crawler_status_string(status));
        return 1;
    }
    printf("Pages visited: %zu\nPages failed: %zu\nLinks discovered: %zu\n"
           "Maximum depth: %zu\nBytes downloaded: %zu\n",
           stats.pages_visited, stats.pages_failed, stats.links_discovered,
           stats.max_depth_reached, stats.bytes_downloaded);
    return 0;
}



