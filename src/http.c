/*
 * Part 3: HTTP Downloader Implementation
 *
 * Implements single-page downloading over HTTP/HTTPS using libcurl.
 * Handles timeouts, redirects (up to 5 hops), size limits (5 MB max),
 * Content-Type verification for HTML, and structured error reporting.
 */

#include "http.h"
#include "url.h"
#include "http_internal.h"

#include <curl/curl.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

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
    BodyBuffer *buf = (BodyBuffer *)userp;
    size_t bytes;
    if (size != 0 && nmemb > SIZE_MAX / size) {
        buf->too_large = 1;
        return 0;
    }
    bytes = size * nmemb;

    /* Enforce project body size limit (5 MB) */
    if (bytes > HTTP_MAX_BODY_BYTES - buf->size) {
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

/* Fixed bounded header storage avoids reallocating in the header callback.
 * Reset on status lines (e.g. proxy CONNECT or interim 100 response). */
typedef struct {
    char text[2049];
    int seen;
    int invalid;
} LocationHeader;

static size_t location_header(char *data, size_t size, size_t count, void *user)
{
    LocationHeader *header = user;
    size_t bytes, start, end, i;
    static const char key[] = "location:";
    if (size != 0 && count > SIZE_MAX / size) { return 0; }
    bytes = size * count;
    if (bytes >= 5 && memcmp(data, "HTTP/", 5) == 0) {
        *header = (LocationHeader){0};
    }
    if (bytes < sizeof(key) - 1) { return bytes; }
    for (i = 0; i < sizeof(key) - 1; ++i) {
        if (tolower((unsigned char)data[i]) != key[i]) { return bytes; }
    }
    start = sizeof(key) - 1;
    end = bytes;
    while (start < end && (data[start] == ' ' || data[start] == '\t')) { ++start; }
    while (end > start && (data[end - 1] == '\r' || data[end - 1] == '\n' ||
           data[end - 1] == ' ' || data[end - 1] == '\t')) { --end; }
    if (header->seen || end == start || end - start >= sizeof(header->text) ||
        memchr(data + start, '\0', end - start) != NULL) {
        header->invalid = 1;
        return 0; /* Fail closed on ambiguous/oversized Location. */
    }
    memcpy(header->text, data + start, end - start);
    header->text[end - start] = '\0';
    header->seen = 1;
    return bytes;
}

/* Test builds can force an option result without unsafe variadic forwarding.
 * No hook reference or code is compiled into the release object. */
#ifdef HTTP_OPTION_TEST
extern CURLcode http_test_option_result(CURLoption option);
#define OPTION_RESULT(option) http_test_option_result(option)
#else
#define OPTION_RESULT(option) CURLE_OK
#endif
#define SET_OPTION(option, value) do { \
    CURLcode setting = OPTION_RESULT(option); \
    if (setting == CURLE_OK) { setting = curl_easy_setopt(curl, option, value); } \
    if (setting != CURLE_OK) { \
        snprintf(response->detail, sizeof(response->detail), \
                 "Cannot configure %s: %.160s", #option, curl_easy_strerror(setting)); \
        curl_easy_cleanup(curl); \
        return setting == CURLE_OUT_OF_MEMORY ? HTTP_ERR_OUT_OF_MEMORY : HTTP_ERR_INIT; \
    } \
} while (0)

/*
 * Compare the entire media-type token, not a substring or a parameter value.
 * HTTP optional whitespace is space/tab; parameters follow the first ';'.
 */
static int is_html_content_type(const char *ct)
{
    if (ct == NULL) {
        return 0;
    }

    static const char *const accepted[] = {
        "text/html", "application/xhtml+xml"
    };
    const char *end;
    size_t length, i;
    while (*ct == ' ' || *ct == '\t') {
        ++ct;
    }
    end = strchr(ct, ';');
    if (end == NULL) {
        end = ct + strlen(ct);
    }
    while (end > ct && (end[-1] == ' ' || end[-1] == '\t')) {
        --end;
    }
    length = (size_t)(end - ct);
    for (i = 0; i < sizeof(accepted) / sizeof(accepted[0]); ++i) {
        size_t j;
        if (length != strlen(accepted[i])) {
            continue;
        }
        for (j = 0; j < length; ++j) {
            if (tolower((unsigned char)ct[j]) != accepted[i][j]) {
                break;
            }
        }
        if (j == length) {
            return 1;
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
static CURLcode ensure_curl_initialized(void)
{
    if (!s_curl_initialized) {
        CURLcode result = curl_global_init(CURL_GLOBAL_DEFAULT);
        if (result != CURLE_OK) { return result; }
        s_curl_initialized = 1;
    }
    return CURLE_OK;
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
    case HTTP_ERR_SCOPE:
        return "URL/redirect outside permitted scope";
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
    case HTTP_ERR_SCOPE:
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
static HttpStatus download_request(const char *url, long timeout_seconds,
                                  HttpResponse *response, char **location)
{
    /* STEP 1: Validate argument pointers and bounds. */
    if (response == NULL) {
        return HTTP_ERR_INVALID_ARGUMENT;
    }

    /* Initialize response to a zeroed state. */
    *response = (HttpResponse){0};
    if (location != NULL) { *location = NULL; }

    if (url == NULL || *url == '\0' || timeout_seconds <= 0 ||
        timeout_seconds > HTTP_MAX_TIMEOUT_SECONDS) {
        snprintf(response->detail, sizeof(response->detail),
                 "Invalid argument: %s",
                 url == NULL ? "url is NULL" :
                 (*url == '\0' ? "url is empty" : "timeout_seconds must be 1..3600"));
        return HTTP_ERR_INVALID_ARGUMENT;
    }

    /* STEP 2: Validate the URL using Part 2's url_parse(). */
    ParsedUrl parsed = {0};
    UrlStatus url_status = url_parse(url, &parsed);
    if (url_status != URL_OK) {
        snprintf(response->detail, sizeof(response->detail),
                 "URL rejected by url_parse (%s): %.150s",
                 url_status_message(url_status), url);
        return url_status == URL_ERROR_OUT_OF_MEMORY
            ? HTTP_ERR_OUT_OF_MEMORY : HTTP_ERR_BAD_URL;
    }
    url_free(&parsed);

    /* STEP 3: Initialize libcurl global and easy session. */
    CURLcode initialized = ensure_curl_initialized();
    if (initialized != CURLE_OK) {
        snprintf(response->detail, sizeof(response->detail),
                 "libcurl global initialization failed");
        return initialized == CURLE_OUT_OF_MEMORY ? HTTP_ERR_OUT_OF_MEMORY : HTTP_ERR_INIT;
    }

    CURL *curl = curl_easy_init();
    if (curl == NULL) {
        snprintf(response->detail, sizeof(response->detail),
                 "libcurl easy session creation failed");
        return HTTP_ERR_OUT_OF_MEMORY;
    }

    /* STEP 4: Configure download options. */
    char errbuf[CURL_ERROR_SIZE] = {0};
    BodyBuffer buf = {0};
    LocationHeader header = {0};

    /* Restrict BOTH the initial request and every redirect. Fail closed if
     * libcurl cannot apply the policy. Keep default TLS verification enabled.
     * String options arrived in 7.85.0; retain older MSYS2 curl compatibility. */
#if LIBCURL_VERSION_NUM >= 0x075500
    SET_OPTION(CURLOPT_PROTOCOLS_STR, "http,https");
    SET_OPTION(CURLOPT_REDIR_PROTOCOLS_STR, "http,https");
#else
    SET_OPTION(CURLOPT_PROTOCOLS, (long)(CURLPROTO_HTTP | CURLPROTO_HTTPS));
    SET_OPTION(CURLOPT_REDIR_PROTOCOLS, (long)(CURLPROTO_HTTP | CURLPROTO_HTTPS));
#endif
    SET_OPTION(CURLOPT_URL, url);
    SET_OPTION(CURLOPT_FOLLOWLOCATION, location == NULL ? 1L : 0L);
    SET_OPTION(CURLOPT_MAXREDIRS, 5L);
    SET_OPTION(CURLOPT_TIMEOUT, timeout_seconds);
    SET_OPTION(CURLOPT_CONNECTTIMEOUT, timeout_seconds);
    SET_OPTION(CURLOPT_SSL_VERIFYPEER, 1L);
    SET_OPTION(CURLOPT_SSL_VERIFYHOST, 2L);
    SET_OPTION(CURLOPT_WRITEFUNCTION, http_write_callback);
    SET_OPTION(CURLOPT_WRITEDATA, &buf);
    SET_OPTION(CURLOPT_ERRORBUFFER, errbuf);
    SET_OPTION(CURLOPT_USERAGENT, "WebCrawler/1.0 (+https://github.com/jatinvimal1506/Web-Crawler-and-Scraper-in-C)");
    SET_OPTION(CURLOPT_MAXFILESIZE_LARGE, (curl_off_t)HTTP_MAX_BODY_BYTES);
    SET_OPTION(CURLOPT_NOSIGNAL, 1L);
    if (location != NULL) {
        SET_OPTION(CURLOPT_HEADERFUNCTION, location_header);
        SET_OPTION(CURLOPT_HEADERDATA, &header);
    }

    /* STEP 5: Perform the HTTP request. */
    CURLcode res = curl_easy_perform(curl);
    if (header.invalid) {
        free(buf.data);
        curl_easy_cleanup(curl);
        snprintf(response->detail, sizeof(response->detail), "Invalid/ambiguous Location header");
        return HTTP_ERR_HTTP_STATUS;
    }

    /* STEP 6: Capture metadata (final URL, HTTP status code, Content-Type). */
    char *effective_url = NULL;
    curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &effective_url);
    if (effective_url != NULL) {
        response->final_url = copy_string(effective_url);
        if (response->final_url == NULL) {
            goto metadata_oom;
        }
    }

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response->status_code);

    char *ct = NULL;
    curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE, &ct);
    if (ct != NULL) {
        response->content_type = copy_string(ct);
        if (response->content_type == NULL) {
            goto metadata_oom;
        }
    }
    if (location != NULL && header.seen) {
        *location = copy_string(header.text);
        if (*location == NULL) { goto metadata_oom; }
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

metadata_oom:
    /* Nothing from a partially copied response is delivered as success.
     * buf owns the body until STEP 10; response owns any copied metadata. */
    free(buf.data);
    if (location != NULL) { free(*location); *location = NULL; }
    http_response_free(response);
    curl_easy_cleanup(curl);
    snprintf(response->detail, sizeof(response->detail),
             "Out of memory copying HTTP response metadata");
    return HTTP_ERR_OUT_OF_MEMORY;
}

HttpStatus http_download(const char *url, long timeout_seconds, HttpResponse *response)
{
    return download_request(url, timeout_seconds, response, NULL);
}

HttpStatus http_download_once(const char *url, long timeout_seconds,
                             HttpResponse *response, char **location)
{
    if (location == NULL) { return HTTP_ERR_INVALID_ARGUMENT; }
    return download_request(url, timeout_seconds, response, location);
}
