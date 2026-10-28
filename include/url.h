#ifndef URL_H
#define URL_H
#include <stddef.h>

/* URL functions return a specific status so callers can report useful errors. */
typedef enum {
    URL_OK = 0,
    URL_ERROR_INVALID_ARGUMENT,
    URL_ERROR_INVALID_FORMAT,
    URL_ERROR_UNSUPPORTED_SCHEME,
    URL_ERROR_INVALID_HOST,
    URL_ERROR_INVALID_PORT,
    URL_ERROR_TOO_LONG,
    URL_ERROR_OUT_OF_MEMORY
} UrlStatus;

typedef struct {
    char *scheme;
    char *host;
    char *port;
    char *path;
    char *query;
    char *fragment;
} ParsedUrl;

/*
 * Parse an absolute HTTP(S) URL. Initialize result as {0} before its first
 * use. On success it owns allocated components; free them with url_free().
 * A non-empty result is rejected so existing pointers are never overwritten.
 */
UrlStatus url_parse(const char *input, ParsedUrl *result);

/* Safe for NULL; frees all parsed component strings and clears the structure. */
void url_free(ParsedUrl *url);

/*
 * Normalize a supported absolute URL or resolve a URL reference. On success
 * *output is heap allocated and belongs to the caller (free it). On failure,
 * *output is NULL whenever output itself is non-NULL.
 */
UrlStatus url_normalize(const char *input, char **output);
UrlStatus url_resolve(const char *base, const char *relative, char **output);

/* Return a static status description; do not modify or free it. */
const char *url_status_message(UrlStatus status);

#endif
