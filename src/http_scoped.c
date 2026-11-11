/* Phase 7: bounded manual redirects using the SAME checked HTTP transport.
 * URL scope is checked before each transfer, never by a string prefix. */
#include "http_internal.h"
#include "http.h"
#include "url.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned scoped_port(const ParsedUrl *parts)
{
    return parts->port != NULL ? (unsigned)strtoul(parts->port, NULL, 10)
        : strcmp(parts->scheme, "https") == 0 ? 443U : 80U;
}

int http_scope_allows(const ParsedUrl *seed, const ParsedUrl *target,
                      int same_domain, int previously_https)
{
    return seed != NULL && target != NULL &&
        scoped_port(target) == scoped_port(seed) &&
        (!same_domain || strcmp(target->host, seed->host) == 0) &&
        (!previously_https || strcmp(target->scheme, "https") == 0);
}

static int redirect_status(long status)
{
    return status == 301 || status == 302 || status == 303 ||
           status == 307 || status == 308;
}
HttpStatus http_download_scoped(const char *url, long timeout_seconds,
                               const char *seed_url, int same_domain,
                               HttpResponse *response)
{
    ParsedUrl seed = {0}, target = {0};
    char *current = NULL, *next = NULL, *location = NULL;
    unsigned redirects = 0;
    int was_https = 0;
    UrlStatus parsed;
    HttpStatus result = HTTP_ERR_INVALID_ARGUMENT;
    if (response == NULL) { return result; }
    *response = (HttpResponse){0};
    if (url == NULL || seed_url == NULL || timeout_seconds < 1 ||
        timeout_seconds > HTTP_MAX_TIMEOUT_SECONDS ||
        (same_domain != 0 && same_domain != 1)) {
        snprintf(response->detail, sizeof(response->detail),
                 "Scoped download needs URLs, domain flag 0|1 and timeout 1..3600");
        return result;
    }
    parsed = url_parse(seed_url, &seed);
    if (parsed != URL_OK) {
        result = parsed == URL_ERROR_OUT_OF_MEMORY ? HTTP_ERR_OUT_OF_MEMORY : HTTP_ERR_BAD_URL;
        goto done;
    }
    parsed = url_normalize(url, &current);
    if (parsed != URL_OK) {
        result = parsed == URL_ERROR_OUT_OF_MEMORY ? HTTP_ERR_OUT_OF_MEMORY : HTTP_ERR_BAD_URL;
        goto done;
    }
    for (;;) {
        parsed = url_parse(current, &target);
        if (parsed != URL_OK) {
            result = parsed == URL_ERROR_OUT_OF_MEMORY ? HTTP_ERR_OUT_OF_MEMORY : HTTP_ERR_BAD_URL;
            goto done;
        }
        if (!http_scope_allows(&seed, &target, same_domain, was_https)) {
            result = HTTP_ERR_SCOPE;
            snprintf(response->detail, sizeof(response->detail),
                     "Target refused before request: host/port scope or HTTPS downgrade");
            goto done;
        }
        was_https = strcmp(target.scheme, "https") == 0;
        url_free(&target);
        http_response_free(response);
        result = http_download_once(current, timeout_seconds, response, &location);
        if (result != HTTP_ERR_HTTP_STATUS || !redirect_status(response->status_code)) {
            goto done;
        }
        if (redirects == 5) {
            result = HTTP_ERR_TOO_MANY_REDIRECTS;
            snprintf(response->detail, sizeof(response->detail), "Scoped redirect limit is five hops");
            goto done;
        }
        if (location == NULL) {
            snprintf(response->detail, sizeof(response->detail), "Redirect has no usable Location");
            goto done;
        }
        if (response->final_url == NULL) {
            result = HTTP_ERR_NETWORK;
            snprintf(response->detail, sizeof(response->detail), "Redirect has no effective response URL");
            goto done;
        }
        parsed = url_resolve(response->final_url, location, &next);
        if (parsed != URL_OK) {
            result = parsed == URL_ERROR_OUT_OF_MEMORY ? HTTP_ERR_OUT_OF_MEMORY : HTTP_ERR_HTTP_STATUS;
            snprintf(response->detail, sizeof(response->detail), "Redirect Location is not a valid HTTP(S) URL");
            goto done;
        }
        free(current);
        current = next;
        next = NULL;
        free(location);
        location = NULL;
        ++redirects;
        /* Keep last-hop metadata until the new target passes validation. */
    }
done:
    free(current);
    free(next);
    free(location);
    url_free(&target);
    url_free(&seed);
    if (result == HTTP_ERR_OUT_OF_MEMORY) {
        http_response_free(response);
        snprintf(response->detail, sizeof(response->detail), "Out of memory in scoped download");
    } else if (result != HTTP_OK && response->detail[0] == '\0') {
        snprintf(response->detail, sizeof(response->detail), "Scoped URL validation failed");
    }
    return result;
}
