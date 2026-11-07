/* Phase 4: a small, conservative anchor scanner, not an HTML5 parser. */
#include "html.h"
#include "url.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* HTML defines these five ASCII whitespace bytes, independent of locale. */
static int html_space(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f';
}

static char ascii_lower(char c)
{
    return c >= 'A' && c <= 'Z' ? (char)(c + ('a' - 'A')) : c;
}

/* Compare a bounded slice, without allocating a temporary C string. */
static int name_is(const char *start, size_t length, const char *name)
{
    size_t i;
    if (length != strlen(name)) {
        return 0;
    }
    for (i = 0; i < length; ++i) {
        if (ascii_lower(start[i]) != name[i]) {
            return 0;
        }
    }
    return 1;
}

/* Locate an actual tag end: a '>' inside a quoted attribute is not the end.
 * A new '<' outside quotes is a recovery point for a broken previous tag. */
static const char *tag_end(const char *p)
{
    char quote = '\0';
    for (; *p != '\0'; ++p) {
        if (quote != '\0') {
            if (*p == quote) {
                quote = '\0';
            }
        } else if (*p == '\'' || *p == '"') {
            quote = *p;
        } else if (*p == '>' || *p == '<') {
            return p;
        }
    }
    return p;
}

/* Decode the practical ASCII entity subset supported by our URL module.
 * Return 1 if decoded, 0 for an unknown name, -1 for an invalid numeric entity.
 * The caller preserves unknown names literally; decoding is never recursive. */
static int entity_byte(const char *p, size_t length, char *byte)
{
    static const struct { const char *name; char byte; } named[] = {
        {"amp", '&'}, {"AMP", '&'}, {"quot", '"'}, {"QUOT", '"'},
        {"apos", '\''}, {"lt", '<'}, {"LT", '<'}, {"gt", '>'},
        {"GT", '>'}, {"colon", ':'}, {"sol", '/'}, {"num", '#'},
        {"Tab", '\t'}, {"NewLine", '\n'}
    };
    size_t i;
    if (length > 0 && p[0] == '#') {
        unsigned value = 0, base = 10;
        i = 1;
        if (i < length && (p[i] == 'x' || p[i] == 'X')) {
            base = 16;
            ++i;
        }
        if (i == length) {
            return -1;
        }
        for (; i < length; ++i) {
            unsigned digit;
            char c = ascii_lower(p[i]);
            if (c >= '0' && c <= '9') {
                digit = (unsigned)(c - '0');
            } else if (c >= 'a' && c <= 'f') {
                digit = (unsigned)(c - 'a') + 10;
            } else {
                return -1;
            }
            /* Bound before multiplying, so enormous numbers cannot overflow. */
            if (digit >= base || value > (127U - digit) / base) {
                return -1;
            }
            value = value * base + digit;
        }
        if (value == 0) {
            return -1; /* Never insert a NUL into the middle of a URL. */
        }
        *byte = (char)value;
        return 1;
    }
    for (i = 0; i < sizeof(named) / sizeof(named[0]); ++i) {
        if (strlen(named[i].name) == length &&
            memcmp(p, named[i].name, length) == 0) {
            *byte = named[i].byte;
            return 1;
        }
    }
    return 0;
}

/* Allocate a decoded href. Each supported entity shrinks, so length+1 is
 * enough. Invalid numeric entities produce an empty href, which is skipped. */
static char *decode_href(const char *start, size_t length)
{
    char *decoded;
    size_t in = 0, out = 0;
    if (length == SIZE_MAX) {
        return NULL;
    }
    decoded = malloc(length + 1);
    if (decoded == NULL) {
        return NULL;
    }
    while (in < length) {
        if (start[in] == '&') {
            size_t end = in + 1;
            char byte;
            int found;
            /* Stop at another '&' to avoid swallowing a following entity. */
            while (end < length && start[end] != ';' && start[end] != '&' &&
                   !html_space(start[end])) {
                ++end;
            }
            if (end < length && start[end] == ';') {
                found = entity_byte(start + in + 1, end - in - 1, &byte);
                if (found < 0) {
                    decoded[0] = '\0';
                    return decoded;
                }
                if (found > 0) {
                    decoded[out++] = byte;
                    in = end + 1;
                    continue;
                }
            }
        }
        decoded[out++] = start[in++];
    }
    decoded[out] = '\0';
    return decoded;
}

/* Read attributes only inside this opening tag. The first href wins,
 * including a boolean/empty href: later duplicates must not override it. */
static void find_href(const char *p, const char *end,
                      const char **value, size_t *length)
{
    *value = NULL;
    *length = 0;
    while (p < end) {
        const char *name, *start;
        size_t name_length;
        int is_href;
        while (p < end && html_space(*p)) {
            ++p;
        }
        if (p == end || (*p == '/' && p + 1 == end)) {
            return; /* End of attributes, including a self-closing marker. */
        }
        name = p;
        while (p < end && !html_space(*p) && *p != '=' && *p != '/' &&
               *p != '\'' && *p != '"' && *p != '`') {
            ++p;
        }
        name_length = (size_t)(p - name);
        if (name_length == 0) {
            return; /* Do not invent an href after a stray '=' or quote. */
        }
        is_href = name_is(name, name_length, "href");
        while (p < end && html_space(*p)) {
            ++p;
        }
        if (p == end || *p != '=') {
            if (is_href) {
                return;
            }
            continue;
        }
        ++p;
        while (p < end && html_space(*p)) {
            ++p;
        }
        if (p < end && (*p == '"' || *p == '\'')) {
            char quote = *p++;
            start = p;
            while (p < end && *p != quote) {
                ++p;
            }
            if (is_href) {
                *value = start;
                *length = (size_t)(p - start);
                return;
            }
            if (p < end) {
                ++p;
                if (p < end && !html_space(*p) &&
                    !(*p == '/' && p + 1 == end)) {
                    return; /* Attributes need a separator after a quote. */
                }
            }
        } else {
            int in_query = 0;
            start = p;
            while (p < end && !html_space(*p)) {
                /* Browsers tolerate '=' in unquoted query strings. Never
                 * treat those bytes as new attributes; whitespace ends value.
                 * Keep other malformed unquoted values conservative. */
                if (*p == '"' || *p == '\'' || *p == '`' ||
                    (*p == '=' && !in_query)) {
                    return;
                }
                if (*p == '?') {
                    in_query = 1;
                } else if (*p == '#') {
                    in_query = 0;
                }
                ++p;
            }
            if (is_href) {
                *value = start;
                *length = (size_t)(p - start);
                return;
            }
        }
    }
}

/* Resolve and append one candidate. Only allocation failure aborts a page;
 * invalid URLs and unsafe schemes are ordinary skipped links. */
static HtmlStatus append_href(HtmlLinks *links, size_t *capacity,
                               const char *base, const char *value,
                               size_t length)
{
    char *decoded = decode_href(value, length);
    char *start, *resolved = NULL;
    size_t end;
    UrlStatus status;
    if (decoded == NULL) {
        return HTML_ERROR_OUT_OF_MEMORY;
    }
    start = decoded;
    while (html_space(*start)) {
        ++start;
    }
    end = strlen(start);
    while (end > 0 && html_space(start[end - 1])) {
        --end;
    }
    start[end] = '\0';
    if (end == 0) {
        free(decoded);
        return HTML_OK;
    }
    status = url_resolve(base, start, &resolved);
    free(decoded);
    if (status != URL_OK) {
        return status == URL_ERROR_OUT_OF_MEMORY
            ? HTML_ERROR_OUT_OF_MEMORY : HTML_OK;
    }
    if (links->count == *capacity) {
        size_t next = *capacity == 0 ? 8 : *capacity * 2;
        char **grown;
        if (next < *capacity || next > SIZE_MAX / sizeof(*grown)) {
            free(resolved);
            return HTML_ERROR_OUT_OF_MEMORY;
        }
        /* Use a temporary: failed realloc leaves the original array intact. */
        grown = realloc(links->urls, next * sizeof(*grown));
        if (grown == NULL) {
            free(resolved);
            return HTML_ERROR_OUT_OF_MEMORY;
        }
        links->urls = grown;
        *capacity = next;
    }
    links->urls[links->count++] = resolved;
    return HTML_OK;
}

void html_links_free(HtmlLinks *links)
{
    size_t i;
    if (links == NULL) {
        return;
    }
    for (i = 0; i < links->count; ++i) {
        free(links->urls[i]);
    }
    free(links->urls);
    *links = (HtmlLinks){0};
}

HtmlStatus html_extract_links(const char *html, const char *base_url,
                              HtmlLinks *result)
{
    HtmlLinks links = {0};
    ParsedUrl base = {0};
    UrlStatus base_status;
    size_t capacity = 0;
    const char *p, *raw_name = NULL;

    if (html == NULL || base_url == NULL || result == NULL ||
        result->urls != NULL || result->count != 0) {
        return HTML_ERROR_INVALID_ARGUMENT;
    }
    base_status = url_parse(base_url, &base);
    url_free(&base);
    if (base_status != URL_OK) {
        return base_status == URL_ERROR_OUT_OF_MEMORY
            ? HTML_ERROR_OUT_OF_MEMORY : HTML_ERROR_INVALID_BASE_URL;
    }
    p = html;
    while ((p = strchr(p, '<')) != NULL) {
        const char *name, *after_name, *end;
        size_t length;
        int closing;
        ++p;
        if (raw_name == NULL && strncmp(p, "!--", 3) == 0) {
            const char *comment_end = strstr(p + 3, "-->");
            if (comment_end == NULL) {
                break;
            }
            p = comment_end + 3;
            continue;
        }
        closing = *p == '/';
        name = p + closing;
        after_name = name;
        while (*after_name != '\0' && !html_space(*after_name) &&
               *after_name != '/' && *after_name != '>' && *after_name != '<') {
            ++after_name;
        }
        length = (size_t)(after_name - name);
        if (raw_name != NULL) {
            if (!closing || !name_is(name, length, raw_name)) {
                continue;
            }
            raw_name = NULL;
        }
        end = tag_end(after_name);
        if (*end == '\0') {
            break;
        }
        if (*end == '<') {
            p = end;
            continue;
        }
        if (!closing && name_is(name, length, "a")) {
            const char *value;
            size_t value_length;
            HtmlStatus status;
            find_href(after_name, end, &value, &value_length);
            if (value != NULL) {
                status = append_href(&links, &capacity, base_url,
                                     value, value_length);
                if (status != HTML_OK) {
                    html_links_free(&links);
                    return status;
                }
            }
        } else if (!closing) {
            static const char *const raw_tags[] = {
                "script", "style", "title", "textarea"
            };
            size_t i;
            for (i = 0; i < sizeof(raw_tags) / sizeof(raw_tags[0]); ++i) {
                if (name_is(name, length, raw_tags[i])) {
                    raw_name = raw_tags[i];
                    break;
                }
            }
        }
        p = end + 1;
    }
    *result = links; /* Transfer ownership only after the entire scan succeeds. */
    return HTML_OK;
}

const char *html_status_message(HtmlStatus status)
{
    switch (status) {
    case HTML_OK: return "success";
    case HTML_ERROR_INVALID_ARGUMENT: return "invalid argument or nonempty result";
    case HTML_ERROR_INVALID_BASE_URL: return "invalid HTTP(S) base URL";
    case HTML_ERROR_OUT_OF_MEMORY: return "out of memory";
    default: return "unknown HTML error";
    }
}
