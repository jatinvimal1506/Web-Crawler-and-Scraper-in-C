#ifndef HTTP_H
#define HTTP_H


#include <stddef.h>
#include "project.h"

#ifdef __cplusplus
extern "C" {
#endif


typedef enum {
    HTTP_OK = 0,
    HTTP_ERR_INVALID_ARGUMENT,
    HTTP_ERR_BAD_URL,
    HTTP_ERR_OUT_OF_MEMORY,
    HTTP_ERR_INIT,
    HTTP_ERR_DNS,
    HTTP_ERR_CONNECT,
    HTTP_ERR_TIMEOUT,
    HTTP_ERR_TLS,
    HTTP_ERR_TOO_MANY_REDIRECTS,
    HTTP_ERR_HTTP_STATUS,
    HTTP_ERR_TOO_LARGE,
    HTTP_ERR_NOT_HTML,
    HTTP_ERR_NETWORK,
    HTTP_ERR_SCOPE
} HttpStatus;


#define HTTP_MAX_BODY_BYTES ((size_t)5 * 1024 * 1024)
#define HTTP_MAX_TIMEOUT_SECONDS 3600L
typedef struct {
    char  *body;
    size_t size;
    long   status_code;
    char  *final_url;
    char  *content_type;
    char   detail[256];
} HttpResponse;
HttpStatus http_download(const char *url, long timeout_seconds,
                         HttpResponse *response);
HttpStatus http_download_scoped(const char *url, long timeout_seconds,
                               const char *seed_url, int same_domain,
                               HttpResponse *response);
void http_response_free(HttpResponse *response);
const char *http_status_string(HttpStatus status);
CrawlerStatus http_to_crawler_status(HttpStatus status);
void http_cleanup(void);
#ifdef __cplusplus
}
#endif
#endif
