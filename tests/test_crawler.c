#include "project.h"
#include <assert.h>
#include <stddef.h>

int main(void)
{
    CrawlerStats stats = {99, 99, 99, 99, 99};
    CrawlerConfig config = {"", 0, 1, 1};
    assert(crawler_run(&config, &stats) == CRAWLER_INVALID_ARGUMENT);
    assert(stats.pages_visited == 0);
    assert(crawler_run(NULL, &stats) == CRAWLER_INVALID_ARGUMENT);
    assert(crawler_status_string(CRAWLER_OK) != NULL);
    return 0;
}

