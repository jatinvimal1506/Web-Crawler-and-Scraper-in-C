#ifndef DOWNLOAD_FIXTURE_H
#define DOWNLOAD_FIXTURE_H

#include <stddef.h>

enum {
    DFX_PDF = 1,
    DFX_DOCX,
    DFX_ZIP,
    DFX_ZIP_ALT,
    DFX_OCTET,
    DFX_HTML,
    DFX_WRONG_MIME,
    DFX_DUPLICATE_MIME,
    DFX_MISSING_MIME,
    DFX_EMPTY,
    DFX_NOT_FOUND,
    DFX_SERVER_ERROR,
    DFX_PARTIAL_CONTENT,
    DFX_PARTIAL,
    DFX_SLOW,
    DFX_DROP,
    DFX_RELATIVE_REDIRECT,
    DFX_REDIRECT_301,
    DFX_REDIRECT_302,
    DFX_REDIRECT_303,
    DFX_REDIRECT_307,
    DFX_REDIRECT_308,
    DFX_OFF_HOST_REDIRECT,
    DFX_OFF_PORT_REDIRECT,
    DFX_LOCALHOST_REDIRECT,
    DFX_HOST_TARGET,
    DFX_PORT_TARGET,
    DFX_LOOP_A,
    DFX_LOOP_B,
    DFX_FTP_REDIRECT,
    DFX_FILE_REDIRECT,
    DFX_MISSING_LOCATION,
    DFX_MALFORMED_LOCATION,
    DFX_REDIRECT_404,
    DFX_REDIRECT_WRONG_MIME,
    DFX_EXACT_LENGTH,
    DFX_EXACT_CHUNKED,
    DFX_OVERSIZE_LENGTH,
    DFX_OVERSIZE_CHUNKED,
    DFX_SITE = 128,
    DFX_COURSES,
    DFX_RESEARCH,
    DFX_RULES_PDF,
    DFX_LECTURE_DOCX,
    DFX_RESEARCH_PDF,
    DFX_MATERIAL_ZIP,
    DFX_EMPTY_SITE,
    DFX_FAILURE_SITE,
    DFX_REDIRECTED_SITE,
    DFX_REDIRECTED_PAGE,
    DFX_REDIRECTED_PDF,
    DFX_SCOPE_SITE,
    DFX_LIMIT_SITE,
    DFX_TIMEOUT_SITE,
    DFX_STAGING_SITE,
    DFX_BINARY_REDIRECT_SITE,
    DFX_ITEM_BASE = 160,
    DFX_BFS_ROOT = 176,
    DFX_BFS_A,
    DFX_BFS_B,
    DFX_BFS_GRANDCHILD,
    DFX_PARTIAL_CRAWL_SITE = 180,
    DFX_FAILED_HTML_CHILD,
    DFX_HOP_BASE = 200,
    DFX_ROUTE_COUNT = 256
};

unsigned download_fixture_start(void);
unsigned download_fixture_other_port(void);
void download_fixture_stop(void);
void download_fixture_reset_counts(void);
size_t download_fixture_request_count(unsigned route);
size_t download_fixture_request_total(void);
unsigned download_fixture_request_order(size_t index);
const unsigned char *download_fixture_small_data(size_t *size);
unsigned char download_fixture_expected_byte(size_t offset);

#endif
