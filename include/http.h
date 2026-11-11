#ifndef HTTP_H
#define HTTP_H


#include <stddef.h>
#include "project.h"     /* CrawlerStatus, so we can report in project terms */

#ifdef __cplusplus
extern "C" {
#endif

/* Why a download succeeded or failed. */
typedef enum {
    HTTP_OK = 0,                  /* got a 2xx answer with an HTML body       */
    HTTP_ERR_INVALID_ARGUMENT,    /* NULL/empty url/response, timeout outside 1..3600 */
    HTTP_ERR_BAD_URL,             /* url_parse() (Part 2) rejected the URL     */
    HTTP_ERR_OUT_OF_MEMORY,       /* malloc/realloc failed                     */
    HTTP_ERR_INIT,                /* libcurl could not start                   */
    HTTP_ERR_DNS,                 /* website name could not be found           */
    HTTP_ERR_CONNECT,             /* server refused or could not be reached    */
    HTTP_ERR_TIMEOUT,             /* took longer than timeout_seconds          */
    HTTP_ERR_TLS,                 /* HTTPS certificate / handshake problem     */
    HTTP_ERR_TOO_MANY_REDIRECTS,  /* redirect loop or more than 5 hops         */
    HTTP_ERR_HTTP_STATUS,         /* server answered 3xx(no target)/4xx/5xx    */
    HTTP_ERR_TOO_LARGE,           /* page bigger than HTTP_MAX_BODY_BYTES      */
    HTTP_ERR_NOT_HTML,            /* Content-Type says image/pdf/json/etc.     */
    HTTP_ERR_NETWORK,             /* any other network failure                 */
    HTTP_ERR_SCOPE               /* scoped target refused BEFORE request      */
} HttpStatus;

/* Pages larger than this are refused (protects memory). 5 MB. */
#define HTTP_MAX_BODY_BYTES ((size_t)5 * 1024 * 1024)
#define HTTP_MAX_TIMEOUT_SECONDS 3600L

/*
 * Everything we learned from one download.
 *
 * OWNERSHIP: the strings below are allocated by http_download(). The caller
 * must release them with http_response_free(). Always pass a response that is
 * either zeroed ( HttpResponse r = {0}; ) or already freed - http_download()
 * overwrites it without freeing the old contents.
 *
 * After success:  body is a NUL-terminated string of `size` bytes.
 * After failure:  body is NULL and size is 0. status_code, final_url,
 *                 content_type and detail are filled in when known, because
 *                 they help explain the failure.
 * Metadata-copy OOM: all owned fields are freed, status_code and size are zero,
 * final_url/content_type/body are NULL, and detail explains the OOM.
 * A missing Content-Type header is NOT OOM: content_type may remain NULL.
 */
typedef struct {
    char  *body;          /* the page's HTML (NUL-terminated)                  */
    size_t size;          /* number of bytes in body (not counting the NUL)    */
    long   status_code;   /* HTTP code such as 200 or 404; 0 if none received  */
    char  *final_url;     /* URL after following redirects (feed it to url_parse) */
    char  *content_type;  /* e.g. "text/html; charset=UTF-8"; may be NULL      */
    char   detail[256];   /* short human-readable explanation of a failure     */
} HttpResponse;

/*
 * Download one page.
 *   url             absolute http:// or https:// URL (checked with url_parse)
 *   timeout_seconds must be 1..HTTP_MAX_TIMEOUT_SECONDS (3600), inclusive
 *   response        filled in; see HttpResponse
 * Returns HTTP_OK or the reason for failure.
 */
HttpStatus http_download(const char *url, long timeout_seconds,
                         HttpResponse *response);

/* Additive crawler transport: validate initial URL and each redirect BEFORE
 * transfer, exact seed host if same_domain=1, effective seed port in both
 * modes. Five redirects maximum; timeout is per hop. HTTPS->HTTP is refused.
 * seed_url and url are borrowed absolute HTTP(S) URLs. Same response ownership
 * as http_download. This is URL scope, NOT IP/DNS/SSRF/robots enforcement. */
HttpStatus http_download_scoped(const char *url, long timeout_seconds,
                               const char *seed_url, int same_domain,
                               HttpResponse *response);

/* Free everything inside response and reset it. NULL is allowed. */
void http_response_free(HttpResponse *response);

/* Short static message for a status. Do not free it. */
const char *http_status_string(HttpStatus status);

/* Translate into the project-wide status from project.h. */
CrawlerStatus http_to_crawler_status(HttpStatus status);

/* Optional: call once at program exit to release libcurl's global state. */
void http_cleanup(void);

#ifdef __cplusplus
}
#endif
#endif
