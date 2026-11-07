#include "html.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const base = "https://example.com/dir/page.html?old=1";

/* Compare the entire output, not just whether a link happened to be found. */
static void expect(const char *input, const char *const *urls, size_t count)
{
    HtmlLinks links = {0};
    size_t i;
    assert(html_extract_links(input, base, &links) == HTML_OK);
    assert(links.count == count);
    for (i = 0; i < count; ++i) {
        assert(strcmp(links.urls[i], urls[i]) == 0);
    }
    html_links_free(&links);
    assert(links.urls == NULL && links.count == 0);
    html_links_free(&links); /* Cleanup can safely be repeated. */
}

static void test_normal(void)
{
    const char *expected[] = {
        "https://example.com/dir/next.html",
        "https://example.com/root",
        "https://other.test/doc.pdf",
        "http://other.test/archive.zip",
        "https://cdn.test/doc.docx",
        "https://example.com/dir/page.html?new=2",
        "https://example.com/dir/page.html?old=1"
    };
    expect("<a href=\"next.html\">Next</a>"
           "<A class='x' HREF \t=\n '/root'>Root</A>"
           "<a href='HTTPS://Other.TEST:443/a/../doc.pdf#part'>PDF</a>"
           "<a href=http://other.test/archive.zip>ZIP</a>"
           "<a href='//cdn.test/doc.docx'>DOCX</a>"
           "<a href='?new=2'>Query</a><a href='#part'>Section</a>",
           expected, sizeof(expected) / sizeof(expected[0]));
    puts("PASS: normal links, case, quotes, whitespace and relative resolution");
}

static void test_duplicates(void)
{
    const char *expected[] = {
        "https://example.com/x",
        "https://example.com/x",
        "https://example.com/x"
    };
    expect("<a href='/x'>1</a><a href='../x'>2</a>"
           "<a href='https://example.com/x#other'>3</a>", expected, 3);
    /* Duplicate attributes use the first occurrence, never the second. */
    expect("<a href='/x' href='/ignored'>x</a>", expected, 1);
    expect("<a href='' href='/ignored'><a href href='/ignored'>", NULL, 0);
    puts("PASS: duplicate links preserved; first href attribute wins");
}

static void test_unquoted_queries(void)
{
    const char *query[] = {
        "https://example.com/search?q=one",
        "https://example.com/search?q=one",
        "https://example.com/search?q=one&next=two",
        "https://example.com/search?q=one"
    };
    expect("<a href=/search?q=one>"
           "<a href='/search?q=one'>"
           "<A HREF = /search?q=one&amp;next=two class=link>"
           "<a href=/search?q=one href=/ignored>", query, 4);
    expect("<a =href='/incorrect'><a `href='/incorrect'>"
           "<a title='x'href='/incorrect'><a /href='/incorrect'>"
           "<a href=/bad=value><a href=/bad`value>"
           "<a href=/search?q=one\"broken\">"
           "<a href=javascript:alert(1)?x=one>"
           "<a href=mailto:a@example.com?subject=one>"
           "<a href=data:text/plain,x?y=one>"
           "<a href=javascript&colon;alert(1)?x=one>", NULL, 0);
    expect("<a href='' href=/search?q=one>"
           "<a href=javascript:x?y=one href=/search?q=one>", NULL, 0);
    {
        const char *recovery[] = {"https://example.com/search?q=one"};
        expect("<a href=/unfinished <a href=/search?q=one>", recovery, 1);
    }
    puts("PASS: unquoted queries, attribute boundaries, schemes and duplicates");
}

static void test_entities(void)
{
    const char *expected[] = {
        "https://example.com/q?a=1&b=2",
        "https://example.com/A?a=1&b=2",
        "https://example.com/a'b",
        "https://example.com/q?x=&unknown;",
        "https://other.test/x"
    };
    expect("<a href='/q?a=1&amp;b=2'>"
           "<a href='&#47;&#x41;?a=1&#38;b=2'>"
           "<a href='/a&apos;b'>"
           "<a href='/q?x=&unknown;'>"
           "<a href='&#x20;https&colon;//other.test/x&#32;'>",
           expected, 5);
    expect("<a href='javascript&colon;alert(1)'>"
           "<a href='java&#115;cript:alert(1)'>"
           "<a href='java&#x09;script:alert(1)'>"
           "<a href='/nul&#0;hidden'>"
           "<a href='/bad&#999999999999999999999999;'>"
           "<a href='/bad&#xZZ;'><a href='/nonascii&#233;'>"
           "<a href='/quote&quot;x'><a href='/less&lt;x'>"
           "<a href='/greater&gt;x'>", NULL, 0);
    puts("PASS: entity decoding, encoded schemes and invalid numeric entities");
}

static void test_encoded_unquoted_queries(void)
{
    const char *expected[] = {"https://example.com/search?q=one"};
    const char *cases[] = {
        "<a href=/search?q=one>",
        "<a href=/search&#63;q=one>",
        "<a href=/search&#x3f;q=one>",
        "<a href=/search&quest;q=one>",
        "<a href='/search&#63;q=one'>",
        "<a href=\"/search&quest;q=one\">",
        "<A HREF=/search&#x003F;q&#61;one class=link>",
        "<a href=/search&quest;q=one href=/ignored>"
    };
    size_t i;
    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        expect(cases[i], expected, 1);
    }
    {
        const char *both[] = {"https://example.com/search?q=one&next=two"};
        expect("<a href=/search&#63;q=one&amp;next=two title=ok>", both, 1);
    }
    expect("<a href=javascript&colon;alert(1)&quest;x=one>"
           "<a href=mailto:a@example.com&#63;subject=one>"
           "<a href=data:text/plain,x&quest;y=one>"
           "<a href=/search&#0;q=one>"
           "<a href=/search&#63q=one>"
           "<a href=/search&Quest;q=one>"
           "<a href=/search&amp;quest;q=one>"
           "<a href=/search&quest;q=one'broken'>"
           "<a title='x'href=/search&quest;q=one>"
           "<a href='' href=/search&quest;q=one>", NULL, 0);
    puts("PASS: literal/encoded query delimiters, boundaries and unsafe schemes");
}

static void test_raw_text_blocks(void)
{
    const char *tags[] = {
        "script", "style", "title", "textarea", "iframe", "IFRAME",
        "xmp", "noembed", "noframes"
    };
    const char *expected[] = {
        "https://example.com/before", "https://example.com/after"
    };
    char input[512];
    size_t i;
    for (i = 0; i < sizeof(tags) / sizeof(tags[0]); ++i) {
        int length = snprintf(input, sizeof(input),
            "<a href='/before'><%s title='>'><a href='/fake'>"
            "</%s><a href='/after'>", tags[i], tags[i]);
        assert(length > 0 && (size_t)length < sizeof(input));
        expect(input, expected, 2);
    }
    expect("<iframe><a href='/fake'></iframe>", NULL, 0);
    expect("<a href='/before'><iframe><a href='/fake'>", expected, 1);
    expect("<a href='/before'><iframe></iframeX><a href='/fake'>", expected, 1);
    expect("<a href='/before'><iframe></iframe <a href='/fake'>"
           "</iframe><a href='/after'>", expected, 2);
    expect("<a href='/before'><plaintext><a href='/fake'>"
           "</plaintext><a href='/also-fake'>", expected, 1);
    expect("<iframe>&lt;/iframe&gt;<a href='/fake'></iframe>"
           "<a href='/after'>", expected + 1, 1);
    puts("PASS: raw-text exclusions, plaintext and malformed closing tags");
}

static void test_unsafe(void)
{
    expect("<a href='javascript:alert(1)'><a href='JaVaScRiPt:alert(1)'>"
           "<a href='data:text/plain,x'><a href='file:///etc/passwd'>"
           "<a href='ftp://example.com/x'><a href='mailto:a@example.com'>"
           "<a href='tel:123'><a href='https://'>"
           "<a href='https://-invalid.test/x'><a href='/bad%ZZ'>"
           "<a href='/has space'><a href='http://example.com:65536/'>",
           NULL, 0);
    puts("PASS: unsafe schemes and malformed URLs rejected");
}

static void test_false_positives(void)
{
    const char *expected[] = {"https://example.com/real"};
    expect("href='/text'<abbr href='/no'><area href='/no'>"
           "<a-extra href='/no'><a:custom href='/no'>"
           "<link href='/no'><img href='/no'>"
           "<a data-href='/no' xhref='/no' title=\"href='/no'\">"
           "<div title=\"<a href='/no'>\">"
           "<!-- <a href='/no'> -->"
           "<SCRIPT>const x = \"<a href='/no'>\";</sCrIpT>"
           "<style>/* <a href='/no'> */</style>"
           "<textarea><a href='/no'></textarea>"
           "<title><a href='/no'></title>"
           "</a href='/no'><a href='/real'>", expected, 1);
    expect("<!-- <a href='/no'>", NULL, 0);
    expect("<script> <a href='/no'>", NULL, 0);
    puts("PASS: ignores non-anchor text, comments and raw-text contents");
}

static void test_malformed(void)
{
    const char *expected[] = {"https://example.com/good"};
    expect("<a><a href><a href=><a href=''><a href=\"\">"
           "<a href=' \t\n\r\f '><a href=/bad`value>"
           "<a href=/bad=value><a href='/unfinished' "
           "<a href='/good'>", expected, 1);
    expect("<a href='/good'><a href='unterminated", expected, 1);
    expect("<a href='/unfinished'", NULL, 0);
    expect("<a href=\"/bad>text", NULL, 0);
    expect("<a title='>' href='/good'>", expected, 1);
    /* A URL containing a slash is not cut off as though it were tag syntax. */
    {
        const char *trailing[] = {"https://example.com/trailing/"};
        expect("<a href=/trailing/>", trailing, 1);
    }
    puts("PASS: malformed, missing and unfinished attributes handled safely");
}

static void test_empty_and_arguments(void)
{
    HtmlLinks links = {0};
    expect("", NULL, 0);
    expect(" \t\r\n", NULL, 0);
    expect("<html><body>No anchors</body></html>", NULL, 0);
    expect("<", NULL, 0);
    expect("<a", NULL, 0);
    assert(html_extract_links(NULL, base, &links) == HTML_ERROR_INVALID_ARGUMENT);
    assert(html_extract_links("", NULL, &links) == HTML_ERROR_INVALID_ARGUMENT);
    assert(html_extract_links("", base, NULL) == HTML_ERROR_INVALID_ARGUMENT);
    assert(html_extract_links("", "", &links) == HTML_ERROR_INVALID_BASE_URL);
    assert(html_extract_links("", "file:///x", &links) ==
           HTML_ERROR_INVALID_BASE_URL);
    assert(links.urls == NULL && links.count == 0);
    html_links_free(NULL);
    puts("PASS: empty HTML, NULL arguments and invalid base URLs");
}

static void test_ownership(void)
{
    HtmlLinks links = {0};
    char input[] = "<a href='/owned'>";
    char source[] = "https://example.com/";
    char **saved;
    assert(html_extract_links(input, source, &links) == HTML_OK);
    saved = links.urls;
    /* Reusing an occupied result cannot leak or overwrite the earlier list. */
    assert(html_extract_links("", base, &links) == HTML_ERROR_INVALID_ARGUMENT);
    assert(links.urls == saved && links.count == 1);
    input[0] = '\0';
    source[0] = '\0';
    assert(strcmp(links.urls[0], "https://example.com/owned") == 0);
    html_links_free(&links);
    assert(html_extract_links("", base, &links) == HTML_OK);
    html_links_free(&links);
    puts("PASS: independent owned output, protected reuse and repeated cleanup");
}

static void test_boundaries(void)
{
    char url[2050], input[2100];
    size_t length, i;
    const char *prefix = "https://example.com/";
    for (length = 2047; length <= 2049; ++length) {
        HtmlLinks links = {0};
        memcpy(url, prefix, strlen(prefix));
        memset(url + strlen(prefix), 'x', length - strlen(prefix));
        url[length] = '\0';
        snprintf(input, sizeof(input), "<a href='%s'>", url);
        assert(html_extract_links(input, base, &links) == HTML_OK);
        assert(links.count == (length <= 2048 ? 1U : 0U));
        if (links.count != 0) {
            assert(strcmp(links.urls[0], url) == 0);
        }
        html_links_free(&links);
    }
    /* Exercise several realloc growth steps and keep document order. */
    {
        HtmlLinks links = {0};
        char many[8192];
        size_t used = 0;
        for (i = 0; i < 200; ++i) {
            int written = snprintf(many + used, sizeof(many) - used,
                                   "<a href='/item%zu'>", i);
            assert(written > 0 && (size_t)written < sizeof(many) - used);
            used += (size_t)written;
        }
        assert(html_extract_links(many, base, &links) == HTML_OK);
        assert(links.count == 200);
        for (i = 0; i < links.count; ++i) {
            snprintf(url, sizeof(url), "https://example.com/item%zu", i);
            assert(strcmp(links.urls[i], url) == 0);
        }
        html_links_free(&links);
    }
    puts("PASS: URL length boundaries and 200-link array growth");
}

static void test_truncations(void)
{
    const char *sample =
        "<!-- ignored --><script>'<a href=\"/fake\">'</script>"
        "<iframe><a href='/fake'></iframe>"
        "<a title='>' href='../x?a=1&amp;b=2'>x</a>"
        "<a href=/search&#63;q=one><a href=/end>";
    size_t i, length = strlen(sample);
    /* Every possible prefix is its own properly NUL-terminated input buffer.
     * Sanitizers can therefore detect any read past a truncated document. */
    for (i = 0; i <= length; ++i) {
        HtmlLinks links = {0};
        char *prefix = malloc(i + 1);
        assert(prefix != NULL);
        memcpy(prefix, sample, i);
        prefix[i] = '\0';
        assert(html_extract_links(prefix, base, &links) == HTML_OK);
        html_links_free(&links);
        free(prefix);
    }
    puts("PASS: every truncation of a mixed HTML document");
}

static void test_messages(void)
{
    HtmlStatus statuses[] = {
        HTML_OK, HTML_ERROR_INVALID_ARGUMENT,
        HTML_ERROR_INVALID_BASE_URL, HTML_ERROR_OUT_OF_MEMORY
    };
    size_t i;
    for (i = 0; i < sizeof(statuses) / sizeof(statuses[0]); ++i) {
        assert(html_status_message(statuses[i])[0] != '\0');
        assert(strcmp(html_status_message(statuses[i]), "unknown HTML error") != 0);
    }
    assert(strcmp(html_status_message((HtmlStatus)999),
                  "unknown HTML error") == 0);
    puts("PASS: all public status messages");
}

int main(void)
{
    test_normal();
    test_duplicates();
    test_unquoted_queries();
    test_entities();
    test_encoded_unquoted_queries();
    test_raw_text_blocks();
    test_unsafe();
    test_false_positives();
    test_malformed();
    test_empty_and_arguments();
    test_ownership();
    test_boundaries();
    test_truncations();
    test_messages();
    puts("All HTML tests passed (14 groups).");
    return 0;
}
