#ifndef HTTP_INTERNAL_H
#define HTTP_INTERNAL_H
#include "http.h"
/* One checked transfer, no automatic redirect. *location is an owned copy
 * (or NULL); caller frees it even on failure. Not part of the public API. */
HttpStatus http_download_once(const char *url, long timeout_seconds,
                             HttpResponse *response, char **location);
#endif
