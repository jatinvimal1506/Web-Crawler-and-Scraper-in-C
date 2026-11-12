#ifndef HTML_H
#define HTML_H

#include <stddef.h>

typedef enum {
    HTML_OK = 0,
    HTML_ERROR_INVALID_ARGUMENT,
    HTML_ERROR_INVALID_BASE_URL,
    HTML_ERROR_OUT_OF_MEMORY,
    HTML_ERROR_LIMIT
} HtmlStatus;


typedef struct {
    char **urls;
    size_t count;
} HtmlLinks;
HtmlStatus html_extract_links(const char *html, const char *base_url,HtmlLinks *result);
HtmlStatus html_extract_links_limited(const char *html, const char *base_url, size_t maximum, HtmlLinks *result);

void html_links_free(HtmlLinks *links);
const char *html_status_message(HtmlStatus status);

#endif
