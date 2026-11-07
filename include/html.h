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

/* Owned array of owned, NUL-terminated absolute HTTP(S) URL strings. */
typedef struct {
    char **urls;
    size_t count;
} HtmlLinks;

/*
 * Extract <a href=...> links from a NUL-terminated HTML string without fetching.
 * base_url must be an absolute HTTP(S) page URL (use HttpResponse.final_url).
 * Initialize result with {0}, or free it before reuse. Nonempty results are
 * rejected without modification. Neither input string is modified or owned.
 *
 * Supports case-insensitive tag/attribute names, single/double-quoted and
 * unquoted values, HTML whitespace, common named and ASCII numeric entities.
 * Unquoted query strings may contain '=' after literal or entity-encoded '?'.
 * Whitespace in the source ends the attribute; references require a semicolon.
 * Invalid/empty hrefs and non-HTTP(S) schemes are skipped. Relative references
 * are resolved by url_resolve(), including its URL limits and normalization
 * rules (notably fragment removal). Duplicates are PRESERVED in source order.
 * Comments and script/style/title/textarea/iframe/xmp/noembed/noframes contents
 * are ignored; plaintext consumes the rest of the document.
 *
 * Empty HTML is a successful empty result; NULL HTML is an invalid argument.
 * Malformed tags are skipped; an unterminated quote/comment/raw-text block
 * consumes the remaining input. This is not a browser/HTML5 tree parser.
 * <base>, JavaScript-generated links and non-anchor links are not supported.
 *
 * On allocation failure all partial results are freed and result stays empty.
 * Release successful results with html_links_free(), not just free(result).
 */
HtmlStatus html_extract_links(const char *html, const char *base_url,
                              HtmlLinks *result);

/*
 * Same parser with bounded retained output. If one more valid link would
 * exceed maximum, return HTML_ERROR_LIMIT and free ALL partial output.
 * The unlimited original API above is unchanged. Used by the BFS engine so
 * a 5 MiB page cannot create an unbounded temporary link array.
 */
HtmlStatus html_extract_links_limited(const char *html, const char *base_url,
                                      size_t maximum, HtmlLinks *result);

/* Frees strings, then their array, then resets fields. NULL/repeated calls OK. */
void html_links_free(HtmlLinks *links);

/* Static description: do not modify or free the returned pointer. */
const char *html_status_message(HtmlStatus status);

#endif
