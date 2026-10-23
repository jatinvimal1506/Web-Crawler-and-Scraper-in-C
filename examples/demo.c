#include "project.h"
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

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
    if (argv[1][0] == '\0') {
        fprintf(stderr, "START_URL must not be empty.\n");
        return 2;
    }
    config.start_url = argv[1];
    config.max_depth = 1u;
    if (argc == 3) {
        char *end = NULL;
        uintmax_t depth;
        const unsigned char *p = (const unsigned char *)argv[2];

        if (*p == '\0') {
            fprintf(stderr, "MAX_DEPTH must be a non-negative integer.\n");
            return 2;
        }
        for (; *p != '\0'; ++p) {
            if (!isdigit(*p)) {
                fprintf(stderr, "MAX_DEPTH must be a non-negative integer.\n");
                return 2;
            }
        }
        errno = 0;
        depth = strtoumax(argv[2], &end, 10);
        if (errno == ERANGE || end == argv[2] || *end != '\0' || depth > SIZE_MAX) {
            fprintf(stderr, "MAX_DEPTH is outside the supported range.\n");
            return 2;
        }
        config.max_depth = (size_t)depth;
    }
    config.same_domain = 1;
    config.timeout_seconds = 15;

    status = crawler_run(&config, &stats);
    if (status == CRAWLER_NOT_IMPLEMENTED) {
        printf("Project foundation is ready; crawling is not implemented yet.\n");
        printf("Start URL: %s\nMaximum depth: %zu\n", config.start_url,
               config.max_depth);
        return 0;
    }
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
