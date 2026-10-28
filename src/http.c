/*
 * Part 3: HTTP Downloader Implementation
 *
 * Implements single-page downloading over HTTP/HTTPS using libcurl.
 * Handles timeouts, redirects (up to 5 hops), size limits (5 MB max),
 * Content-Type verification for HTML, and structured error reporting.
 */

#include "http.h"
#include "url.h"

#include <curl/curl.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Track whether libcurl global initialization has been performed. */
static int s_curl_initialized = 0;

/*
 * Allocate a new copy of a string.
 * Uses standard C11 functions to ensure strict portability.
 */
static char *copy_string(const char *src)
{
    if (src == NULL) {
        return NULL;
    }
    size_t len = strlen(src);
    char *dst = (char *)malloc(len + 1);
    if (dst == NULL) {
        return NULL;
    }
    memcpy(dst, src, len);
    dst[len] = '\0';
    return dst;
}

/*
 * Internal dynamic buffer for collecting the downloaded response body.
 */
typedef struct {
    char   *data;
    size_t  size;
    size_t  capacity;
    int     too_large;
    int     oom;
} BodyBuffer;

/*
 * libcurl write callback invoked as chunks of the response body arrive.
 */
static size_t http_write_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t bytes = size * nmemb;
    BodyBuffer *buf = (BodyBuffer *)userp;

    /* Enforce project body size limit (5 MB) */
    if (buf->size + bytes > HTTP_MAX_BODY_BYTES) {
        buf->too_large = 1;
        return 0; /* Aborting returns CURLE_WRITE_ERROR */
    }

    size_t needed = buf->size + bytes + 1;
    if (needed > buf->capacity) {
        size_t new_cap = buf->capacity ? buf->capacity * 2 : 16384;
        while (new_cap < needed) {
            new_cap *= 2;
        }
        char *new_data = (char *)realloc(buf->data, new_cap);
        if (new_data == NULL) {
            buf->oom = 1;
            return 0; /* Out of memory abort */
        }
        buf->data = new_data;
        buf->capacity = new_cap;
    }

    memcpy(buf->data + buf->size, contents, bytes);
    buf->size += bytes;
    buf->data[buf->size] = '\0';

    return bytes;
}

/*
 * Check whether a Content-Type string indicates HTML.
 * Accepts "text/html" and "application/xhtml+xml" (case-insensitive).
 */
static int is_html_content_type(const char *ct)
{
    if (ct == NULL) {
        return 0;
    }

    static const char html_mime[] = "text/html";
    static const char xhtml_mime[] = "application/xhtml+xml";
    size_t ct_len = strlen(ct);
    size_t h_len = sizeof(html_mime) - 1;
    size_t x_len = sizeof(xhtml_mime) - 1;

    for (size_t i = 0; i < ct_len; i++) {
        if (i + h_len <= ct_len) {
            int match = 1;
            for (size_t j = 0; j < h_len; j++) {
                if (tolower((unsigned char)ct[i + j]) != html_mime[j]) {
                    match = 0;
                    break;
                }
            }
            if (match) {
                return 1;
            }
        }
        if (i + x_len <= ct_len) {
            int match = 1;
            for (size_t j = 0; j < x_len; j++) {
                if (tolower((unsigned char)ct[i + j]) != xhtml_mime[j]) {
                    match = 0;
                    break;
                }
            }
            if (match) {
                return 1;
            }
        }
    }
    return 0;
}

/*
 * Fallback check when the server omits Content-Type header:
 * inspects the first non-whitespace characters for HTML doctype or tag.
 */
static int is_body_html_heuristic(const char *body, size_t size)
{
    if (body == NULL || size == 0) {
        return 0;
    }
    size_t i = 0;
    while (i < size && isspace((unsigned char)body[i])) {
        i++;
    }
    if (i >= size) {
        return 0;
    }
    if (size - i >= 9) {
        char prefix[10];
        for (int k = 0; k < 9; k++) {
            prefix[k] = (char)tolower((unsigned char)body[i + k]);
        }
        prefix[9] = '\0';
        if (memcmp(prefix, "<!doctype", 9) == 0) {
            return 1;
        }
    }
    if (size - i >= 5) {
        char prefix[6];
        for (int k = 0; k < 5; k++) {
            prefix[k] = (char)tolower((unsigned char)body[i + k]);
        }
        prefix[5] = '\0';
        if (memcmp(prefix, "<html", 5) == 0) {
            return 1;
        }
    }
    return 0;
}

/*
 * Ensure libcurl's global state is initialized.
 */
static int ensure_curl_initialized(void)
{
    if (!s_curl_initialized) {
        if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
            return -1;
        }
        s_curl_initialized = 1;
    }
    return 0;
}

/*
 * Release libcurl global resources at program exit.
 */
void http_cleanup(void)
{
    if (s_curl_initialized) {
        curl_global_cleanup();
        s_curl_initialized = 0;
    }
}

/*
 * Release every allocated component of an HttpResponse.
 */
void http_response_free(HttpResponse *response)
{
    if (response == NULL) {
        return;
    }

    free(response->body);
    free(response->final_url);
    free(response->content_type);

    response->body = NULL;
    response->size = 0;
    response->status_code = 0;
    response->final_url = NULL;
    response->content_type = NULL;
    response->detail[0] = '\0';
}

/*
 * Map HttpStatus to a static human-readable description.
 */
const char *http_status_string(HttpStatus status)
{
    switch (status) {
    case HTTP_OK:
        return "success";
    case HTTP_ERR_INVALID_ARGUMENT:
        return "invalid argument";
    case HTTP_ERR_BAD_URL:
        return "invalid or rejected URL";
    case HTTP_ERR_OUT_OF_MEMORY:
        return "out of memory";
    case HTTP_ERR_INIT:
        return "downloader initialization failed";
    case HTTP_ERR_DNS:
        return "DNS resolution failed";
    case HTTP_ERR_CONNECT:
        return "connection failed";
    case HTTP_ERR_TIMEOUT:
        return "transfer timed out";
    case HTTP_ERR_TLS:
        return "TLS/SSL error";
    case HTTP_ERR_TOO_MANY_REDIRECTS:
        return "too many redirects";
    case HTTP_ERR_HTTP_STATUS:
        return "non-2xx HTTP status";
    case HTTP_ERR_TOO_LARGE:
        return "response body exceeded size limit";
    case HTTP_ERR_NOT_HTML:
        return "Content-Type is not HTML";
    case HTTP_ERR_NETWORK:
        return "network error";
    default:
        return "unknown HTTP status";
    }
}

/*
 * Translate an HttpStatus into the crawler's unified CrawlerStatus.
 */
CrawlerStatus http_to_crawler_status(HttpStatus status)
{
    switch (status) {
    case HTTP_OK:
        return CRAWLER_OK;
    case HTTP_ERR_INVALID_ARGUMENT:
    case HTTP_ERR_BAD_URL:
        return CRAWLER_INVALID_ARGUMENT;
    case HTTP_ERR_OUT_OF_MEMORY:
        return CRAWLER_OUT_OF_MEMORY;
    case HTTP_ERR_NOT_HTML:
        return CRAWLER_PARSE_ERROR;
    case HTTP_ERR_INIT:
    case HTTP_ERR_DNS:
    case HTTP_ERR_CONNECT:
    case HTTP_ERR_TIMEOUT:
    case HTTP_ERR_TLS:
    case HTTP_ERR_TOO_MANY_REDIRECTS:
    case HTTP_ERR_HTTP_STATUS:
    case HTTP_ERR_TOO_LARGE:
    case HTTP_ERR_NETWORK:
    default:
        return CRAWLER_NETWORK_ERROR;
    }
}

/*
 * Download one page using libcurl.
 */
HttpStatus http_download(const char *url, long timeout_seconds,
                         HttpResponse *response)
{
    /* STEP 1: Validate argument pointers and bounds. */
    if (response == NULL) {
        return HTTP_ERR_INVALID_ARGUMENT;
    }

    /* Initialize response to a zeroed state. */
    *response = (HttpResponse){0};

    if (url == NULL || *url == '\0' || timeout_seconds <= 0) {
        snprintf(response->detail, sizeof(response->detail),
                 "Invalid argument: %s",
                 url == NULL ? "url is NULL" :
                 (*url == '\0' ? "url is empty" : "timeout_seconds must be > 0"));
        return HTTP_ERR_INVALID_ARGUMENT;
    }

    /* STEP 2: Validate the URL using Part 2's url_parse(). */
    ParsedUrl parsed = {0};
    UrlStatus url_status = url_parse(url, &parsed);
    if (url_status != URL_OK) {
        snprintf(response->detail, sizeof(response->detail),
                 "URL rejected by url_parse (%s): %.150s",
                 url_status_message(url_status), url);
        return HTTP_ERR_BAD_URL;
    }
    url_free(&parsed);

    /* STEP 3: Initialize libcurl global and easy session. */
    if (ensure_curl_initialized() != 0) {
        snprintf(response->detail, sizeof(response->detail),
                 "libcurl global initialization failed");
        return HTTP_ERR_INIT;
    }

    CURL *curl = curl_easy_init();
    if (curl == NULL) {
        snprintf(response->detail, sizeof(response->detail),
                 "libcurl easy session creation failed");
        return HTTP_ERR_INIT;
    }

    /* STEP 4: Configure download options. */
    char errbuf[CURL_ERROR_SIZE] = {0};
    BodyBuffer buf = {0};

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_seconds);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, timeout_seconds);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, http_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "WebCrawler/1.0 (+https://github.com/jatinvimal1506/Web-Crawler-and-Scraper-in-C)");
    curl_easy_setopt(curl, CURLOPT_MAXFILESIZE_LARGE, (curl_off_t)HTTP_MAX_BODY_BYTES);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

    /* STEP 5: Perform the HTTP request. */
    CURLcode res = curl_easy_perform(curl);

    /* STEP 6: Capture metadata (final URL, HTTP status code, Content-Type). */
    char *effective_url = NULL;
    curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &effective_url);
    if (effective_url != NULL) {
        response->final_url = copy_string(effective_url);
    }

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response->status_code);

    char *ct = NULL;
    curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE, &ct);
    if (ct != NULL) {
        response->content_type = copy_string(ct);
    }

    /* STEP 7: Check transfer outcome and map errors. */
    if (res != CURLE_OK) {
        free(buf.data);
        buf.data = NULL;
        buf.size = 0;

        if (buf.too_large || res == CURLE_FILESIZE_EXCEEDED) {
            snprintf(response->detail, sizeof(response->detail),
                     "Page exceeded maximum body size limit of %zu bytes", HTTP_MAX_BODY_BYTES);
            curl_easy_cleanup(curl);
            return HTTP_ERR_TOO_LARGE;
        }

        if (buf.oom || res == CURLE_OUT_OF_MEMORY) {
            snprintf(response->detail, sizeof(response->detail),
                     "Out of memory allocating response buffer");
            curl_easy_cleanup(curl);
            return HTTP_ERR_OUT_OF_MEMORY;
        }

        const char *err_msg = (errbuf[0] != '\0') ? errbuf : curl_easy_strerror(res);

        switch (res) {
        case CURLE_COULDNT_RESOLVE_HOST:
        case CURLE_COULDNT_RESOLVE_PROXY:
            snprintf(response->detail, sizeof(response->detail),
                     "DNS resolution failed: %.200s", err_msg);
            curl_easy_cleanup(curl);
            return HTTP_ERR_DNS;

        case CURLE_COULDNT_CONNECT:
            snprintf(response->detail, sizeof(response->detail),
                     "Connection failed: %.200s", err_msg);
            curl_easy_cleanup(curl);
            return HTTP_ERR_CONNECT;

        case CURLE_OPERATION_TIMEDOUT:
            snprintf(response->detail, sizeof(response->detail),
                     "Download timed out after %ld seconds: %.180s", timeout_seconds, err_msg);
            curl_easy_cleanup(curl);
            return HTTP_ERR_TIMEOUT;

        case CURLE_SSL_CONNECT_ERROR:
        case CURLE_PEER_FAILED_VERIFICATION:
#if defined(CURLE_SSL_CERTPROBLEM)
        case CURLE_SSL_CERTPROBLEM:
#endif
#if defined(CURLE_SSL_CIPHER)
        case CURLE_SSL_CIPHER:
#endif
#if defined(CURLE_USE_SSL_FAILED)
        case CURLE_USE_SSL_FAILED:
#endif
            snprintf(response->detail, sizeof(response->detail),
                     "TLS/SSL error: %.200s", err_msg);
            curl_easy_cleanup(curl);
            return HTTP_ERR_TLS;

        case CURLE_TOO_MANY_REDIRECTS:
            snprintf(response->detail, sizeof(response->detail),
                     "Too many redirects (exceeded 5 hops): %.180s", err_msg);
            curl_easy_cleanup(curl);
            return HTTP_ERR_TOO_MANY_REDIRECTS;

        default:
            snprintf(response->detail, sizeof(response->detail),
                     "Network transfer error: %.200s", err_msg);
            curl_easy_cleanup(curl);
            return HTTP_ERR_NETWORK;
        }
    }

    /* STEP 8: Verify HTTP status code (must be 2xx). */
    if (response->status_code < 200 || response->status_code >= 300) {
        free(buf.data);
        response->body = NULL;
        response->size = 0;
        snprintf(response->detail, sizeof(response->detail),
                 "Server returned HTTP status %ld", response->status_code);
        curl_easy_cleanup(curl);
        return HTTP_ERR_HTTP_STATUS;
    }

    /* STEP 9: Verify Content-Type is HTML. */
    int is_html = 0;
    if (ct != NULL) {
        is_html = is_html_content_type(ct);
    } else {
        is_html = is_body_html_heuristic(buf.data, buf.size);
    }

    if (!is_html) {
        free(buf.data);
        response->body = NULL;
        response->size = 0;
        snprintf(response->detail, sizeof(response->detail),
                 "Content-Type '%s' is not HTML", ct ? ct : "(none)");
        curl_easy_cleanup(curl);
        return HTTP_ERR_NOT_HTML;
    }

    /* STEP 10: Deliver successful response. */
    if (buf.data == NULL) {
        buf.data = (char *)calloc(1, 1);
        if (buf.data == NULL) {
            snprintf(response->detail, sizeof(response->detail),
                     "Out of memory allocating empty body string");
            curl_easy_cleanup(curl);
            return HTTP_ERR_OUT_OF_MEMORY;
        }
        buf.size = 0;
    }

    response->body = buf.data;
    response->size = buf.size;
    response->detail[0] = '\0';

    curl_easy_cleanup(curl);
    return HTTP_OK;
}
