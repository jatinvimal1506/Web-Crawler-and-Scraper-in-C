/* URL parsing, relative-link resolution, and canonical normalization belong here.
 * Keep allocation and ownership rules explicit in the public API. */
#include "url.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>


// static:- the function  can only be called from within this file. It is not visible to other files in the project.
static char*copy_text(const char*start,size_t length){
    // this helper allocates a new string, copies the text we want. 
    char *result = malloc(length + 1);
    if (!result) {
        return NULL;
    }
    memcpy(result, start, length);
    result[length] = '\0';
    return result;
}
/*
 * Release every dynamically allocated component
 * of a ParsedUrl structure.
 */
void url_free(ParsedUrl *url)
{
    // Check that the structure pointer exists.
    if (url == NULL) {
        return;
    }
    // Release the six separately allocated strings.
    free(url->scheme);
    free(url->host);
    free(url->port);
    free(url->path);
    free(url->query);
    free(url->fragment);
    /*
     * Reset pointers after freeing them.
     *
     * This prevents the structure from retaining
     * dangling pointers to released memory.
     */
    url->scheme = NULL;
    url->host = NULL;
    url->port = NULL;
    url->path = NULL;
    url->query = NULL;
    url->fragment = NULL;
}

static int is_valid_host(const char*start,size_t length){
    //a host is valid if it is not empty and does not exceed 253 characters.
    if(length==0||length>253){
        return 0; // Invalid length
    }
    size_t label_length = 0;
    for(size_t i=0;i<length;i++){
        unsigned char c = (unsigned char)start[i];
        if(c == '.'){
            if(label_length == 0 || start[i-1]=='-'){
                return 0; // Invalid label length
            }
            label_length = 0; // Reset label length for the next label
            continue;
        }
        int is_letter =(c >= 'a' && c <= 'z') ||(c >= 'A' && c <= 'Z');
        int is_digit = (c >= '0' && c <= '9');
        if (!is_letter && !is_digit && c != '-') {
            return 0;
        }
        if(label_length ==0 && c == '-'){
            return 0;
        }
        if(label_length >= 63){
            return 0; // Invalid label length
        }
        label_length++;
    }

    return label_length > 0 && start[length - 1] != '-'; // Last label must not be empty or end with a hyphen
}
/*
 * Parse an absolute HTTP/HTTPS URL into six components.
 *
 * This version supports:
 *   - http and https (lowercase)
 *   - basic ASCII hostnames
 *   - optional numeric ports
 *   - paths
 *   - query strings
 *   - fragments
 *
 * It does not yet implement URL normalization,
 * relative resolution, IPv6, or percent decoding.
 */
int url_parse(const char *input, ParsedUrl *result)
{
    /*
     * STEP 1: Validate the input pointers.
     *
     * Both input and result must point to
     * valid objects.
     */
    if (input == NULL || result == NULL) {
        return -1;
    }

    /*
     * Start with empty component pointers.
     *
     * Important: the caller must not reuse a
     * populated ParsedUrl without freeing it first.
     */
    *result = (ParsedUrl){0};
    /*
     * STEP 2: Check URL length.
     *
     * 2048 is our project limit, not a
     * universal maximum URL length.
     */
    size_t length = strlen(input);
    if (length == 0 || length > 2048) {
        return -1;
    }
    /*
     * Reject raw spaces and control characters.
     */
    for (size_t i = 0; i < length; i++) {
        unsigned char c = (unsigned char)input[i];

        if (c <= 32 || c == 127) {
            return -1;
        }
    }
    /*
     * STEP 3: Find "://", separating scheme
     * from the authority.
     */
    const char *separator = strstr(input, "://");
    if (separator == NULL) {
        return -1;
    }
    size_t scheme_length =
        (size_t)(separator - input);
    // Support lowercase HTTP and HTTPS for now.
    int is_http =
        scheme_length == 4 &&
        tolower((unsigned char)input[0]) == 'h' &&
        tolower((unsigned char)input[1]) == 't' &&
        tolower((unsigned char)input[2]) == 't' &&
        tolower((unsigned char)input[3]) == 'p';
    int is_https =
        scheme_length == 5 &&
        is_http && 0; /* overwritten below for clarity */
    is_https = scheme_length == 5 &&
        tolower((unsigned char)input[0]) == 'h' &&
        tolower((unsigned char)input[1]) == 't' &&
        tolower((unsigned char)input[2]) == 't' &&
        tolower((unsigned char)input[3]) == 'p' &&
        tolower((unsigned char)input[4]) == 's';
    if (!is_http && !is_https) {
        return -1;
    }
    /*
     * STEP 4: Locate the authority.
     *
     * Example:
     * https://example.com:8080/products
     *         ^----------------^
     *              authority
     *
     * Authority ends at '/', '?' or '#'.
     */
    const char *authority_start = separator + 3;
    const char *authority_end =
        authority_start +
        strcspn(authority_start, "/?#");
    if (authority_end == authority_start) {
        return -1;
    }
    /*
     * STEP 5: Separate hostname and port.
     *
     * Search for a colon within the authority.
     */
    const char *port_separator = NULL;
    for (const char *p = authority_start;
         p < authority_end; p++) {
        if (*p == ':') {
            // Multiple colons are unsupported.
            if (port_separator != NULL) {
                return -1;
            }
            port_separator = p;
        }
    }
    const char *host_end =
        port_separator != NULL
        ? port_separator
        : authority_end;
    size_t host_length =
        (size_t)(host_end - authority_start);
    /*
     * Reuse our Part 2.2 hostname validator.
     */
    if (!is_valid_host(authority_start,
                       host_length)) {
        return -1;
    }
    /*
     * STEP 6: Validate an optional port.
     *
     * Ports must contain only digits and
     * have a numeric value from 1 to 65535.
     */
    if (port_separator != NULL) {

        const char *port_start = port_separator + 1;

        if (port_start == authority_end) {
            return -1;  // Empty port
        }
        unsigned long port_number = 0;

        for (const char *p = port_start;
             p < authority_end; p++) {

            // Every character must be a digit.
            if (*p < '0' || *p > '9') {
                return -1;
            }

            /*
             * Convert decimal digits into a number.
             *
             * Example:
             * '8' -> 8
             * '0' -> 80
             * '8' -> 808
             * '0' -> 8080
             */
            port_number =
                port_number * 10 + (unsigned)(*p - '0');

            /*
             * Reject immediately when too large.
             * Because we stop above 65535, this
             * calculation cannot overflow here.
             */
            if (port_number > 65535) {
                return -1;
            }
        }
        if (port_number == 0) {
            return -1;
        }
    }
    /*
     * STEP 7: Identify the path.
     *
     * Path ends at '?' or '#'.
     *
     * If no path exists, use "/" as the
     * default path.
     */
    const char *path_start = authority_end;
    const char *path_end = path_start;

    if (*path_start == '/') {
        path_end =
            path_start + strcspn(path_start, "?#");
    }

    /*
     * STEP 8: Identify the optional query.
     *
     * The query begins after '?'
     * and ends before '#', if present.
     */
    const char *query_start = NULL;
    const char *query_end = NULL;

    const char *cursor = path_end;

    if (*cursor == '?') {
        query_start = cursor + 1;
        query_end =
            query_start + strcspn(query_start, "#");

        cursor = query_end;
    }
    /*
     * STEP 9: Identify the optional fragment.
     *
     * Everything after '#' belongs to
     * the fragment.
     */
    const char *fragment_start = NULL;
    if (*cursor == '#') {
        fragment_start = cursor + 1;
    }
    /*
     * STEP 10: Allocate separate strings.
     *
     * copy_text() was created in Part 2.1.
     */
    result->scheme = copy_text(input, scheme_length);
    if (result->scheme != NULL) {
        for (size_t i = 0; i < scheme_length; ++i)
            result->scheme[i] = (char)tolower((unsigned char)result->scheme[i]);
    }

    result->host = copy_text(authority_start, host_length);
    if (result->host != NULL) {
        for (size_t i = 0; i < host_length; ++i)
            result->host[i] = (char)tolower((unsigned char)result->host[i]);
    }

    // Store the optional port without its colon.
    if (port_separator != NULL) {
        result->port = copy_text(
            port_separator + 1,
            (size_t)(authority_end - port_separator - 1)
        );
    }
    // A missing path becomes "/".
    if (*path_start == '/') {
        result->path = copy_text(
            path_start,
            (size_t)(path_end - path_start)
        );
    } else {
        result->path = copy_text("/", 1);
    }
    // Store query without '?'.
    if (query_start != NULL) {
        result->query = copy_text(
            query_start,
            (size_t)(query_end - query_start)
        );
    }
    // Store fragment without '#'.
    if (fragment_start != NULL) {
        result->fragment =
            copy_text(fragment_start, strlen(fragment_start));
    }
    /*
     * STEP 11: Check allocation results.
     *
     * Required fields cannot be NULL.
     * Optional fields need allocation only
     * when present in the original URL.
     */
    if (result->scheme == NULL ||
        result->host == NULL ||
        result->path == NULL ||
        (port_separator != NULL && result->port == NULL) ||
        (query_start != NULL && result->query == NULL) ||
        (fragment_start != NULL &&
         result->fragment == NULL)) {
        // Release allocations made before failure.
        url_free(result);
        return -1;
    }
    // All components were processed successfully.
    return 0;
}

/* Join components while excluding fragments from crawler identity. */
int url_normalize(const char *input, char **output)
{
    ParsedUrl u = {0};
    char *path = NULL;
    size_t *segments = NULL, count = 0, cap, n, i = 0, outn = 0;
    char *joined = NULL;
    int result = -1;
    if (output == NULL) return -1;
    *output = NULL;
    if (url_parse(input, &u) != 0) return -1;
    if (u.port != NULL && ((strcmp(u.scheme,"http")==0 && strcmp(u.port,"80")==0) ||
                           (strcmp(u.scheme,"https")==0 && strcmp(u.port,"443")==0))) {
        free(u.port); u.port = NULL;
    }
    n = strlen(u.path); cap = n + 1;
    path = malloc(n + 2); segments = malloc((n + 1) * sizeof(*segments));
    if (!path || !segments) goto done;
    path[outn++] = '/';
    while (i < n) {
        size_t start, len;
        while (i < n && u.path[i] == '/') ++i;
        start = i;
        while (i < n && u.path[i] != '/') ++i;
        len = i - start;
        if (len == 0 || (len == 1 && u.path[start] == '.')) continue;
        if (len == 2 && u.path[start] == '.' && u.path[start+1] == '.') {
            if (count) outn = segments[--count];
            continue;
        }
        segments[count++] = outn;
        if (outn > 1) path[outn++] = '/';
        memcpy(path + outn, u.path + start, len); outn += len;
    }
    if (outn == 0) path[outn++] = '/';
    path[outn] = '\0';
    (void)cap;
    {
        size_t need = strlen(u.scheme)+3+strlen(u.host)+strlen(path)+1;
        if (u.port) need += strlen(u.port)+1;
        if (u.query) need += strlen(u.query)+1;
        joined = malloc(need);
        if (!joined) goto done;
        snprintf(joined, need, "%s://%s%s%s%s%s%s", u.scheme, u.host,
            u.port ? ":" : "", u.port ? u.port : "", path,
            u.query ? "?" : "", u.query ? u.query : "");
    }
    *output = joined; joined = NULL; result = 0;
done:
    free(joined); free(path); free(segments); url_free(&u); return result;
}

/* Resolve the common HTTP reference forms used by links in HTML pages. */
int url_resolve(const char *base, const char *relative, char **output)
{
    ParsedUrl b = {0};
    char *candidate = NULL, *authority = NULL, *dir = NULL;
    size_t need;
    int rc = -1;
    if (!output) return -1;
    *output = NULL;
    if (!base || !relative || url_parse(base, &b) != 0) return -1;
    if (strstr(relative, "://") != NULL) return url_normalize(relative, output);
    need = strlen(b.scheme)+strlen(b.host)+strlen(b.path)+strlen(relative)+strlen(b.port ? b.port : "")+16;
    candidate = malloc(need); authority = malloc(strlen(b.host)+strlen(b.port ? b.port : "")+2);
    if (!candidate || !authority) goto done;
    snprintf(authority, strlen(b.host)+strlen(b.port ? b.port : "")+2, "%s%s%s", b.host, b.port?":":"", b.port?b.port:"");
    if (relative[0] == '/' && relative[1] == '/') snprintf(candidate, need, "%s:%s", b.scheme, relative);
    else if (relative[0] == '/') snprintf(candidate, need, "%s://%s%s", b.scheme, authority, relative);
    else if (relative[0] == '?' || relative[0] == '#') {
        char *base_path = malloc(strlen(b.path)+1);
        if (!base_path) goto done;
        strcpy(base_path, b.path);
        snprintf(candidate, need, "%s://%s%s%s%s", b.scheme, authority, base_path, relative[0]=='?' ? "" : "", relative);
        free(base_path);
    } else {
        size_t plen = strlen(b.path), cut = plen;
        while (cut && b.path[cut-1] != '/') --cut;
        dir = malloc(cut+1); if (!dir) goto done;
        memcpy(dir, b.path, cut); dir[cut]='\0';
        snprintf(candidate, need, "%s://%s%s%s", b.scheme, authority, dir, relative);
    }
    rc = url_normalize(candidate, output);
done:
    free(dir); free(candidate); free(authority); url_free(&b); return rc;
}
