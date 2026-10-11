#include "project.h"

const char *crawler_status_string(CrawlerStatus status)
{
    switch (status) {
    case CRAWLER_OK: return "success";
    case CRAWLER_INVALID_ARGUMENT: return "invalid argument";
    case CRAWLER_OUT_OF_MEMORY: return "out of memory";
    case CRAWLER_NETWORK_ERROR: return "network error";
    case CRAWLER_PARSE_ERROR: return "parse error";
    case CRAWLER_NOT_IMPLEMENTED: return "not implemented";
    default: return "unknown error";
    }
}

/* TODO: integrate URL, HTTP, HTML, queue and visited-set modules. */
CrawlerStatus crawler_run(const CrawlerConfig *config, CrawlerStats *stats)
{
    if (stats != NULL) {
        *stats = (CrawlerStats){0};
    }
    if (config == NULL || stats == NULL || config->start_url == NULL ||
        config->start_url[0] == '\0') {
        return CRAWLER_INVALID_ARGUMENT;
    }
    return CRAWLER_NOT_IMPLEMENTED;
}

