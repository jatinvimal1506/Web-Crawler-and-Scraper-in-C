#ifndef HTTP_INTERNAL_H
#define HTTP_INTERNAL_H
#include "http.h"
#include "url.h"


HttpStatus http_download_once(const char *url, long timeout_seconds,
                             HttpResponse *response, char **location);

HttpStatus http_prepare_curl(void);
int http_scope_allows(const ParsedUrl *seed, const ParsedUrl *target,
                      int same_domain, int previously_https);
#endif
