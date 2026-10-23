#include "project.h"
#include <assert.h>

int main(void)
{
    CrawlerStats stats = {99, 99, 99, 99, 99};
    CrawlerConfig config = {"https://example.org", 2, 1, 15};

    assert(crawler_run(NULL, &stats) == CRAWLER_INVALID_ARGUMENT);
    assert(stats.pages_visited == 0 && stats.pages_failed == 0 &&
           stats.links_discovered == 0 && stats.max_depth_reached == 0 &&
           stats.bytes_downloaded == 0);

    stats = (CrawlerStats){99, 99, 99, 99, 99};
    assert(crawler_run(&config, NULL) == CRAWLER_INVALID_ARGUMENT);
    assert(crawler_run(&config, &stats) == CRAWLER_NOT_IMPLEMENTED);
    assert(stats.pages_visited == 0 && stats.pages_failed == 0 &&
           stats.links_discovered == 0 && stats.max_depth_reached == 0 &&
           stats.bytes_downloaded == 0);

    config.start_url = NULL;
    assert(crawler_run(&config, &stats) == CRAWLER_INVALID_ARGUMENT);
    config.start_url = "";
    assert(crawler_run(&config, &stats) == CRAWLER_INVALID_ARGUMENT);
    config.start_url = "https://example.org";
    config.same_domain = 2;
    assert(crawler_run(&config, &stats) == CRAWLER_INVALID_ARGUMENT);
    config.same_domain = 1;
    config.timeout_seconds = 0;
    assert(crawler_run(&config, &stats) == CRAWLER_INVALID_ARGUMENT);

    assert(crawler_status_string(CRAWLER_OK)[0] != '\0');
    assert(crawler_status_string(CRAWLER_INVALID_ARGUMENT)[0] != '\0');
    assert(crawler_status_string(CRAWLER_OUT_OF_MEMORY)[0] != '\0');
    assert(crawler_status_string(CRAWLER_NETWORK_ERROR)[0] != '\0');
    assert(crawler_status_string(CRAWLER_PARSE_ERROR)[0] != '\0');
    assert(crawler_status_string(CRAWLER_NOT_IMPLEMENTED)[0] != '\0');
    assert(crawler_status_string((CrawlerStatus)999)[0] != '\0');
    return 0;
}
