/* URL parsing, canonicalization, and common HTTP reference resolution. */
#include "url.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define URL_MAX_LENGTH 2048

static char *copy_text(const char *start, size_t length)
{
    char *copy = malloc(length + 1);
    if (copy != NULL) {
        memcpy(copy, start, length);
        copy[length] = '\0';
    }
    return copy;
}

/* Host labels follow the ASCII DNS hostname limits used by this project. */
static int valid_host(const char *host, size_t length)
{
    size_t label_length = 0;
    size_t i;

    if (length == 0 || length > 253) {
        return 0;
    }
    for (i = 0; i < length; ++i) {
        unsigned char c = (unsigned char)host[i];
        int letter = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
        int digit = c >= '0' && c <= '9';

        if (c == '.') {
            if (label_length == 0 || host[i - 1] == '-') {
                return 0;
            }
            label_length = 0;
        } else {
            if (!letter && !digit && c != '-') {
                return 0;
            }
            if ((label_length == 0 && c == '-') || label_length == 63) {
                return 0;
            }
            ++label_length;
        }
    }
    return label_length != 0 && host[length - 1] != '-';
}

static int equal_ascii_case(const char *text, size_t length, const char *word)
{
    size_t i;
    if (strlen(word) != length) {
        return 0;
    }
    for (i = 0; i < length; ++i) {
        if (tolower((unsigned char)text[i]) != (unsigned char)word[i]) {
            return 0;
        }
    }
    return 1;
}

static int hex_digit(unsigned char c)
{
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
           (c >= 'A' && c <= 'F');
}

/* Accept URI ASCII characters and require every percent escape to be complete. */
static int valid_url_characters(const char *text, size_t length)
{
    size_t i;
    for (i = 0; i < length; ++i) {
        unsigned char c = (unsigned char)text[i];
        int allowed = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || strchr("-._~:/?#@!$&'()*+,;=%", c) != NULL;
        if (!allowed) {
            return 0;
        }
        if (c == '%') {
            if (i + 2 >= length || !hex_digit((unsigned char)text[i + 1]) ||
                !hex_digit((unsigned char)text[i + 2])) {
                return 0;
            }
            i += 2;
        }
    }
    return 1;
}

static int field_is_empty(const ParsedUrl *url)
{
    return url->scheme == NULL && url->host == NULL && url->port == NULL &&
           url->path == NULL && url->query == NULL && url->fragment == NULL;
}

void url_free(ParsedUrl *url)
{
    if (url == NULL) {
        return;
    }
    free(url->scheme);
    free(url->host);
    free(url->port);
    free(url->path);
    free(url->query);
    free(url->fragment);
    *url = (ParsedUrl){0};
}

/*
 * Parse one absolute HTTP(S) URL. Build into a temporary structure so that
 * partial allocation failures can be cleaned without exposing partial output.
 */
UrlStatus url_parse(const char *input, ParsedUrl *result)
{
    ParsedUrl parsed = {0};
    const char *scheme_end, *authority, *authority_end, *port_mark = NULL;
    const char *path_start, *path_end, *query_start = NULL, *query_end = NULL;
    const char *fragment_start = NULL, *cursor;
    size_t length, scheme_length, host_length;
    unsigned long port_number = 0;
    const char *p;
    UrlStatus status = URL_ERROR_INVALID_FORMAT;

    if (input == NULL || result == NULL) {
        return URL_ERROR_INVALID_ARGUMENT;
    }
    /* Do not silently discard caller-owned pointers from an earlier parse. */
    if (!field_is_empty(result)) {
        return URL_ERROR_INVALID_ARGUMENT;
    }
    length = strlen(input);
    if (length == 0) {
        return URL_ERROR_INVALID_FORMAT;
    }
    if (length > URL_MAX_LENGTH) {
        return URL_ERROR_TOO_LONG;
    }
    if (!valid_url_characters(input, length)) {
        return URL_ERROR_INVALID_FORMAT;
    }

    scheme_end = strstr(input, "://");
    if (scheme_end == NULL) {
        const char *colon = strchr(input, ':');
        return colon != NULL ? URL_ERROR_UNSUPPORTED_SCHEME
                             : URL_ERROR_INVALID_FORMAT;
    }
    scheme_length = (size_t)(scheme_end - input);
    if (!equal_ascii_case(input, scheme_length, "http") &&
        !equal_ascii_case(input, scheme_length, "https")) {
        return URL_ERROR_UNSUPPORTED_SCHEME;
    }

    authority = scheme_end + 3;
    authority_end = authority + strcspn(authority, "/?#");
    if (authority == authority_end) {
        return URL_ERROR_INVALID_HOST;
    }
    for (p = authority; p < authority_end; ++p) {
        if (*p == ':') {
            if (port_mark != NULL) {
                return URL_ERROR_INVALID_HOST;
            }
            port_mark = p;
        }
    }
    host_length = (size_t)((port_mark != NULL ? port_mark : authority_end) - authority);
    if (!valid_host(authority, host_length)) {
        return URL_ERROR_INVALID_HOST;
    }
    if (port_mark != NULL) {
        p = port_mark + 1;
        if (p == authority_end) {
            return URL_ERROR_INVALID_PORT;
        }
        for (; p < authority_end; ++p) {
            if (*p < '0' || *p > '9') {
                return URL_ERROR_INVALID_PORT;
            }
            port_number = port_number * 10 + (unsigned)(*p - '0');
            if (port_number > 65535) {
                return URL_ERROR_INVALID_PORT;
            }
        }
        if (port_number == 0) {
            return URL_ERROR_INVALID_PORT;
        }
    }

    path_start = authority_end;
    path_end = path_start;
    if (*path_start == '/') {
        path_end = path_start + strcspn(path_start, "?#");
    }
    cursor = path_end;
    if (*cursor == '?') {
        query_start = cursor + 1;
        query_end = query_start + strcspn(query_start, "#");
        cursor = query_end;
    }
    if (*cursor == '#') {
        fragment_start = cursor + 1;
    }

    parsed.scheme = copy_text(input, scheme_length);
    parsed.host = copy_text(authority, host_length);
    parsed.port = port_mark != NULL
        ? copy_text(port_mark + 1, (size_t)(authority_end - port_mark - 1))
        : NULL;
    parsed.path = *path_start == '/'
        ? copy_text(path_start, (size_t)(path_end - path_start))
        : copy_text("/", 1);
    parsed.query = query_start != NULL
        ? copy_text(query_start, (size_t)(query_end - query_start))
        : NULL;
    parsed.fragment = fragment_start != NULL
        ? copy_text(fragment_start, strlen(fragment_start))
        : NULL;

    if (parsed.scheme == NULL || parsed.host == NULL || parsed.path == NULL ||
        (port_mark != NULL && parsed.port == NULL) ||
        (query_start != NULL && parsed.query == NULL) ||
        (fragment_start != NULL && parsed.fragment == NULL)) {
        status = URL_ERROR_OUT_OF_MEMORY;
        goto fail;
    }
    for (p = parsed.scheme; *p != '\0'; ++p) {
        ((char *)p)[0] = (char)tolower((unsigned char)*p);
    }
    for (p = parsed.host; *p != '\0'; ++p) {
        ((char *)p)[0] = (char)tolower((unsigned char)*p);
    }
    *result = parsed;
    return URL_OK;

fail:
    url_free(&parsed);
    return status;
}

/* Pop the last output path segment for the RFC-style /../ transformation. */
static void remove_last_segment(char *output, size_t *length)
{
    while (*length > 0 && output[*length - 1] != '/') {
        --*length;
    }
    if (*length > 0) {
        --*length;
    }
    output[*length] = '\0';
}

static void remove_prefix(char *input, size_t count)
{
    size_t remaining = strlen(input + count);
    memmove(input, input + count, remaining + 1);
}

/*
 * Apply RFC-style dot-segment removal while preserving repeated separators
 * and trailing slash meaning. The input path is absolute and bounded.
 */
static char *normalize_path(const char *path)
{
    size_t length = strlen(path);
    char *input = copy_text(path, length);
    char *output = malloc(length + 2);
    size_t out_length = 0;

    if (input == NULL || output == NULL) {
        free(input);
        free(output);
        return NULL;
    }
    output[0] = '\0';
    while (*input != '\0') {
        size_t segment_length;
        char *next_slash;

        if (strncmp(input, "../", 3) == 0) {
            remove_prefix(input, 3);
        } else if (strncmp(input, "./", 2) == 0) {
            remove_prefix(input, 2);
        } else if (strncmp(input, "/./", 3) == 0) {
            remove_prefix(input, 2);
        } else if (strcmp(input, "/.") == 0) {
            input[1] = '\0';
        } else if (strncmp(input, "/../", 4) == 0) {
            remove_prefix(input, 3);
            remove_last_segment(output, &out_length);
        } else if (strcmp(input, "/..") == 0) {
            input[1] = '\0';
            remove_last_segment(output, &out_length);
        } else if (strcmp(input, ".") == 0 || strcmp(input, "..") == 0) {
            input[0] = '\0';
        } else {
            next_slash = input[0] == '/'
                ? strchr(input + 1, '/')
                : strchr(input, '/');
            segment_length = next_slash != NULL
                ? (size_t)(next_slash - input)
                : strlen(input);
            memcpy(output + out_length, input, segment_length);
            out_length += segment_length;
            output[out_length] = '\0';
            remove_prefix(input, segment_length);
        }
    }
    free(input);
    if (out_length == 0) {
        output[0] = '/';
        output[1] = '\0';
    }
    return output;
}

static int is_default_port(const ParsedUrl *url)
{
    char *end = NULL;
    unsigned long value;
    if (url->port == NULL) {
        return 0;
    }
    value = strtoul(url->port, &end, 10);
    return end != url->port && *end == '\0' &&
        ((strcmp(url->scheme, "http") == 0 && value == 80) ||
         (strcmp(url->scheme, "https") == 0 && value == 443));
}

/* Build scheme://authority plus a path/query/optional fragment tail. */
static UrlStatus build_url(const ParsedUrl *url, const char *tail, char **output)
{
    size_t need = strlen(url->scheme) + 3 + strlen(url->host) + strlen(tail) + 1;
    char *text;
    if (url->port != NULL && !is_default_port(url)) {
        need += strlen(url->port) + 1;
    }
    text = malloc(need);
    if (text == NULL) {
        return URL_ERROR_OUT_OF_MEMORY;
    }
    snprintf(text, need, "%s://%s%s%s%s", url->scheme, url->host,
             (url->port != NULL && !is_default_port(url)) ? ":" : "",
             (url->port != NULL && !is_default_port(url)) ? url->port : "", tail);
    *output = text;
    return URL_OK;
}

UrlStatus url_normalize(const char *input, char **output)
{
    ParsedUrl parsed = {0};
    char *path = NULL, *tail = NULL;
    size_t tail_size;
    UrlStatus status;

    if (output == NULL) {
        return URL_ERROR_INVALID_ARGUMENT;
    }
    *output = NULL;
    status = url_parse(input, &parsed);
    if (status != URL_OK) {
        return status;
    }
    path = normalize_path(parsed.path);
    if (path == NULL) {
        status = URL_ERROR_OUT_OF_MEMORY;
        goto done;
    }
    tail_size = strlen(path) + (parsed.query != NULL ? strlen(parsed.query) + 1 : 0) + 1;
    tail = malloc(tail_size);
    if (tail == NULL) {
        status = URL_ERROR_OUT_OF_MEMORY;
        goto done;
    }
    snprintf(tail, tail_size, "%s%s%s", path,
             parsed.query != NULL ? "?" : "",
             parsed.query != NULL ? parsed.query : "");
    status = build_url(&parsed, tail, output);
done:
    free(path);
    free(tail);
    url_free(&parsed);
    return status;
}

static int has_scheme(const char *reference)
{
    const char *colon = strchr(reference, ':');
    const char *delimiter = strpbrk(reference, "/?#");
    return colon != NULL && (delimiter == NULL || colon < delimiter);
}

UrlStatus url_resolve(const char *base, const char *relative, char **output)
{
    ParsedUrl parsed_base = {0};
    char *tail = NULL, *candidate = NULL, *directory = NULL;
    const char *reference_path = relative;
    size_t relative_length, base_path_length, directory_length, tail_size;
    UrlStatus status;

    if (output == NULL) {
        return URL_ERROR_INVALID_ARGUMENT;
    }
    *output = NULL;
    if (base == NULL || relative == NULL) {
        return URL_ERROR_INVALID_ARGUMENT;
    }
    relative_length = strlen(relative);
    if (relative_length > URL_MAX_LENGTH) {
        return URL_ERROR_TOO_LONG;
    }
    status = url_parse(base, &parsed_base);
    if (status != URL_OK) {
        return status;
    }

    if (relative_length == 0) {
        status = url_normalize(base, output);
        goto done;
    }
    if (has_scheme(relative)) {
        status = url_normalize(relative, output);
        goto done;
    }
    if (relative[0] == '/' && relative[1] == '/') {
        size_t size = strlen(parsed_base.scheme) + 1 + relative_length + 1;
        candidate = malloc(size);
        if (candidate == NULL) {
            status = URL_ERROR_OUT_OF_MEMORY;
            goto done;
        }
        snprintf(candidate, size, "%s:%s", parsed_base.scheme, relative);
        status = url_normalize(candidate, output);
        goto done;
    }

    base_path_length = strlen(parsed_base.path);
    if (relative[0] == '/') {
        reference_path = relative;
        directory_length = 0;
        directory = copy_text("", 0);
    } else if (relative[0] == '?' || relative[0] == '#') {
        directory_length = base_path_length;
        directory = copy_text(parsed_base.path, directory_length);
    } else {
        const char *last_slash = strrchr(parsed_base.path, '/');
        directory_length = last_slash != NULL
            ? (size_t)(last_slash - parsed_base.path + 1)
            : 1;
        directory = copy_text(parsed_base.path, directory_length);
    }
    if (directory == NULL) {
        status = URL_ERROR_OUT_OF_MEMORY;
        goto done;
    }

    tail_size = directory_length + relative_length +
        ((relative[0] == '#' && parsed_base.query != NULL)
             ? strlen(parsed_base.query) + 1 : 0) + 2;
    tail = malloc(tail_size);
    if (tail == NULL) {
        status = URL_ERROR_OUT_OF_MEMORY;
        goto done;
    }
    if (relative[0] == '#') {
        snprintf(tail, tail_size, "%s%s%s%s", directory,
                 parsed_base.query != NULL ? "?" : "",
                 parsed_base.query != NULL ? parsed_base.query : "", relative);
    } else if (relative[0] == '?' || relative[0] == '/') {
        snprintf(tail, tail_size, "%s", reference_path);
        if (relative[0] == '?') {
            size_t path_and_query = directory_length + relative_length + 1;
            char *combined = malloc(path_and_query);
            if (combined == NULL) {
                status = URL_ERROR_OUT_OF_MEMORY;
                goto done;
            }
            snprintf(combined, path_and_query, "%s%s", directory, relative);
            free(tail);
            tail = combined;
            tail_size = path_and_query;
        }
    } else {
        snprintf(tail, tail_size, "%s%s", directory, relative);
    }
    status = build_url(&parsed_base, tail, &candidate);
    if (status == URL_OK) {
        status = url_normalize(candidate, output);
    }
done:
    free(tail);
    free(candidate);
    free(directory);
    url_free(&parsed_base);
    return status;
}

const char *url_status_message(UrlStatus status)
{
    switch (status) {
    case URL_OK: return "success";
    case URL_ERROR_INVALID_ARGUMENT: return "invalid argument";
    case URL_ERROR_INVALID_FORMAT: return "invalid URL format";
    case URL_ERROR_UNSUPPORTED_SCHEME: return "unsupported URL scheme";
    case URL_ERROR_INVALID_HOST: return "invalid hostname";
    case URL_ERROR_INVALID_PORT: return "invalid port";
    case URL_ERROR_TOO_LONG: return "URL exceeds the 2048-byte limit";
    case URL_ERROR_OUT_OF_MEMORY: return "out of memory";
    default: return "unknown URL error";
    }
}
