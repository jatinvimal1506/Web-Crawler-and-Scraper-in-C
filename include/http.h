#ifndef HTTP_H
#define HTTP_H

/*
 * Part 3: HTTP downloader.
 *
 * Give it one URL, get back either the page's HTML or a clear reason why
 * the page could not be fetched. It never exits the program and never
 * crashes on bad input or a bad network.
 */

#include <stddef.h>
#include "project.h"     /* CrawlerStatus, so we can report in project terms */

#ifdef __cplusplus
extern "C" {
#endif

/* Why a download succeeded or failed. */
typedef enum {
    HTTP_OK = 0,                  /* got a 2xx answer with an HTML body       */
    HTTP_ERR_INVALID_ARGUMENT,    /* NULL/empty url, NULL response, timeout<=0 */
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
    HTTP_ERR_NETWORK              /* any other network failure                 */
} HttpStatus;

/* Pages larger than this are refused (protects memory). 5 MB. */
#define HTTP_MAX_BODY_BYTES ((size_t)5 * 1024 * 1024)

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
 *   timeout_seconds must be > 0; use CrawlerConfig.timeout_seconds
 *   response        filled in; see HttpResponse
 * Returns HTTP_OK or the reason for failure.
 */
HttpStatus http_download(const char *url, long timeout_seconds,
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
