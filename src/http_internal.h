#ifndef HTTP_INTERNAL_H
#define HTTP_INTERNAL_H
#include "http.h"
#include "url.h"
/* One checked transfer, no automatic redirect. *location is an owned copy
 * (or NULL); caller frees it even on failure. Not part of the public API. */
HttpStatus http_download_once(const char *url, long timeout_seconds,
                             HttpResponse *response, char **location);
/* Shared internals for Phase 9's binary transport. */
HttpStatus http_prepare_curl(void);
int http_scope_allows(const ParsedUrl *seed, const ParsedUrl *target,
                      int same_domain, int previously_https);
#endif
