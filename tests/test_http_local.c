




#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif

#ifdef HTTP_ALLOC_TEST
#include <stdlib.h>




static size_t metadata_allocations, fail_metadata_at, http_live_allocations;
void *http_test_malloc(size_t size);
void *http_test_realloc(void *pointer, size_t size);
void *http_test_calloc(size_t count, size_t size);
void http_test_free(void *pointer);

void *http_test_malloc(size_t size)
{
    void *pointer;
    ++metadata_allocations;
    pointer = metadata_allocations == fail_metadata_at ? NULL : malloc(size);
    if (pointer != NULL) {
        ++http_live_allocations;
    }
    return pointer;
}

void *http_test_realloc(void *pointer, size_t size)
{
    int was_null = pointer == NULL;
    void *grown;
    if (size == 0) {
        abort();
    }
    grown = realloc(pointer, size);
    if (grown != NULL && was_null) {
        ++http_live_allocations;
    }
    return grown;
}

void *http_test_calloc(size_t count, size_t size)
{
    void *pointer = calloc(count, size);
    if (pointer != NULL) {
        ++http_live_allocations;
    }
    return pointer;
}

void http_test_free(void *pointer)
{
    if (pointer != NULL) {
        if (http_live_allocations == 0) {
            abort();
        }
        --http_live_allocations;
    }
    free(pointer);
}
#endif

#ifdef _WIN32
#include <stdio.h>
int main(void)
{
    puts("SKIP: local HTTP fixtures require POSIX fork/sockets; "
         "native Windows integration execution is not verified.");
    return 0;
}
#else
#include "html.h"
#include "http.h"

#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static pid_t fixture_pid = -1;
static unsigned fixture_port;
static unsigned checks;


#define CHECK(condition) do { \
    ++checks; \
    if (!(condition)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        exit(EXIT_FAILURE); \
    } \
} while (0)

static void stop_fixture(void)
{
    if (fixture_pid > 0) {
        int status;
        (void)kill(fixture_pid, SIGTERM);
        while (waitpid(fixture_pid, &status, 0) < 0 && errno == EINTR) {
        }
        fixture_pid = -1;
    }
    http_cleanup();
}


static int send_all(int fd, const void *data, size_t size)
{
    const char *p = data;
    while (size > 0) {
        ssize_t sent = send(fd, p, size, 0);
        if (sent < 0 && errno == EINTR) {
            continue;
        }
        if (sent <= 0) {
            return 0;
        }
        p += sent;
        size -= (size_t)sent;
    }
    return 1;
}

static void serve_response(int fd, const char *path)
{
    static const char html[] =
        "<html><a href=../docs/notes.pdf?download=one&amp;x=two>notes</a></html>";
    static const unsigned char binary[] = {'%', 'P', 'D', 'F', 0, 1, 255, 2};
    const char *type = "text/html";
    const void *body = html;
    size_t size = sizeof(html) - 1;
    const char *location = NULL;
    char header[512], size_header[80];
    int large = 0, streaming = 0, written;

    if (strcmp(path, "/old/start") == 0) {
        location = "/new/page.html";
    } else if (strcmp(path, "/to-file") == 0) {
        location = "file:///deepdownload-fixture-must-not-be-read";
    } else if (strcmp(path, "/to-ftp") == 0) {
        location = "ftp://127.0.0.1:1/must-not-be-requested";
    } else if (strcmp(path, "/loop") == 0) {
        location = "/loop";
    } else if (strcmp(path, "/html") == 0) {
        type = "TeXt/HtMl \t; charset=UTF-8";
    } else if (strcmp(path, "/html-no-type") == 0) {
        type = NULL;
    } else if (strcmp(path, "/xhtml") == 0) {
        type = "Application/XHTML+XML; charset=UTF-8";
    } else if (strcmp(path, "/fake-type") == 0) {
        type = "application/x-text/html-evil";
    } else if (strcmp(path, "/fake-xhtml") == 0) {
        type = "application/xhtml+xml-evil";
    } else if (strcmp(path, "/parameter-fake") == 0) {
        type = "application/octet-stream; note=text/html";
    } else if (strcmp(path, "/binary") == 0 ||
               strcmp(path, "/binary-no-type") == 0) {
        type = strcmp(path, "/binary") == 0 ? "application/pdf" : NULL;
        body = binary;
        size = sizeof(binary);
    } else if (strcmp(path, "/at-limit") == 0 ||
               strcmp(path, "/over-limit") == 0 ||
               strcmp(path, "/over-limit-stream") == 0) {
        large = 1;
        size = HTTP_MAX_BODY_BYTES + (strcmp(path, "/at-limit") != 0 ? 1 : 0);
        streaming = strcmp(path, "/over-limit-stream") == 0;
    } else if (strcmp(path, "/new/page.html") != 0) {
        const char not_found[] =
            "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
        (void)send_all(fd, not_found, sizeof(not_found) - 1);
        return;
    }
    if (location != NULL) {
        written = snprintf(header, sizeof(header),
            "HTTP/1.1 302 Found\r\nLocation: %s\r\n"
            "Content-Length: 0\r\nConnection: close\r\n\r\n", location);
        if (written > 0 && (size_t)written < sizeof(header)) {
            (void)send_all(fd, header, (size_t)written);
        }
        return;
    }
    size_header[0] = '\0';
    if (!streaming) {
        (void)snprintf(size_header, sizeof(size_header),
                       "Content-Length: %zu\r\n", size);
    }
    written = snprintf(header, sizeof(header),
        "HTTP/1.1 200 OK\r\n%s%s%s%sConnection: close\r\n\r\n",
        type != NULL ? "Content-Type: " : "",
        type != NULL ? type : "", type != NULL ? "\r\n" : "", size_header);
    if (written <= 0 || (size_t)written >= sizeof(header) ||
        !send_all(fd, header, (size_t)written)) {
        return;
    }
    if (!large) {
        (void)send_all(fd, body, size);
    } else {
        char block[8192];
        memset(block, 'x', sizeof(block));
        while (size > 0) {
            size_t chunk = size < sizeof(block) ? size : sizeof(block);
            if (!send_all(fd, block, chunk)) {
                break;
            }
            size -= chunk;
        }
    }
}

static void fixture_loop(int listener)
{
    (void)signal(SIGPIPE, SIG_IGN);
    for (;;) {
        char request[4096], path[128];
        size_t used = 0;
        struct timeval timeout = {5, 0};
        int fd = accept(listener, NULL, NULL);
        if (fd < 0) {
            if (errno == EINTR) {
                continue;
            }
            _exit(2);
        }
        (void)setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        (void)setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
        request[0] = '\0';
        while (used < sizeof(request) - 1 &&
               strstr(request, "\r\n\r\n") == NULL) {
            ssize_t got = recv(fd, request + used, sizeof(request) - used - 1, 0);
            if (got < 0 && errno == EINTR) {
                continue;
            }
            if (got <= 0) {
                break;
            }
            used += (size_t)got;
            request[used] = '\0';
        }
        if (strstr(request, "\r\n\r\n") != NULL &&
            sscanf(request, "GET %127s", path) == 1) {
            serve_response(fd, path);
        }
        (void)close(fd);
    }
}

static void start_fixture(void)
{
    struct sockaddr_in address = {0};
    socklen_t length = sizeof(address);
    int listener = socket(AF_INET, SOCK_STREAM, 0);
    CHECK(listener >= 0);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;
    CHECK(bind(listener, (struct sockaddr *)&address, sizeof(address)) == 0);
    CHECK(getsockname(listener, (struct sockaddr *)&address, &length) == 0);
    fixture_port = ntohs(address.sin_port);
    CHECK(listen(listener, 8) == 0);
    fixture_pid = fork();
    CHECK(fixture_pid >= 0);
    if (fixture_pid == 0) {
        fixture_loop(listener);
        _exit(0);
    }
    (void)close(listener);
    CHECK(atexit(stop_fixture) == 0);

    CHECK(setenv("no_proxy", "127.0.0.1", 1) == 0);
}

static HttpStatus download_path(const char *path, HttpResponse *response)
{
    char url[256];
    int written = snprintf(url, sizeof(url), "http://127.0.0.1:%u%s",
                           fixture_port, path);
    CHECK(written > 0 && (size_t)written < sizeof(url));
    return http_download(url, 5, response);
}

static void expect_failure(const char *path, HttpStatus expected)
{
    HttpResponse response = {0};
    HttpStatus actual = download_path(path, &response);
    if (actual != expected) {
        fprintf(stderr, "%s: expected %s, got %s (%s)\n", path,
                http_status_string(expected), http_status_string(actual),
                response.detail);
    }
    CHECK(actual == expected);
    CHECK(response.body == NULL && response.size == 0);
    CHECK(response.detail[0] != '\0');
    if (expected == HTTP_ERR_NETWORK) {


        CHECK(strstr(response.detail, "Protocol") != NULL ||
              strstr(response.detail, "protocol") != NULL);
        CHECK(strstr(response.detail, "disabled") != NULL ||
              strstr(response.detail, "not supported") != NULL ||
              strstr(response.detail, "Unsupported") != NULL);
    }
    http_response_free(&response);
    http_response_free(&response);
    CHECK(response.final_url == NULL && response.content_type == NULL);
}

#ifdef HTTP_ALLOC_TEST
static void expect_metadata_oom(const char *path, size_t fail_at)
{
    HttpResponse response = {0};
    CHECK(http_live_allocations == 0);
    metadata_allocations = 0;
    fail_metadata_at = fail_at;
    CHECK(download_path(path, &response) == HTTP_ERR_OUT_OF_MEMORY);
    CHECK(metadata_allocations == fail_at);
    CHECK(response.body == NULL && response.size == 0);
    CHECK(response.final_url == NULL && response.content_type == NULL);
    CHECK(response.status_code == 0 && response.detail[0] != '\0');
    CHECK(http_live_allocations == 0);
    http_response_free(&response);
    http_response_free(&response);
    CHECK(http_live_allocations == 0);


    fail_metadata_at = 0;
    metadata_allocations = 0;
    CHECK(download_path(path, &response) == HTTP_OK);
    CHECK(response.body != NULL && response.final_url != NULL);
    CHECK((response.content_type == NULL) == (strcmp(path, "/html-no-type") == 0));
    http_response_free(&response);
    CHECK(http_live_allocations == 0);
}

static void test_metadata_allocations(void)
{
    HttpResponse response = {0};
    expect_metadata_oom("/html", 1);
    expect_metadata_oom("/html", 2);
    expect_metadata_oom("/html-no-type", 1);


    metadata_allocations = 0;
    fail_metadata_at = 2;
    CHECK(download_path("/html-no-type", &response) == HTTP_OK);
    CHECK(metadata_allocations == 1);
    CHECK(response.final_url != NULL && response.content_type == NULL);
    http_response_free(&response);
    CHECK(http_live_allocations == 0);
    fail_metadata_at = 0;
    puts("PASS: 3 metadata-copy OOM cases, absent optional header and cleanup/reuse");
}
#endif

int main(void)
{
    HttpResponse response = {0};
    HtmlLinks links = {0};
    char expected[256];
    const char *valid_types[] = {"/html", "/xhtml"};
    size_t i;
    start_fixture();

    CHECK(download_path("/old/start", &response) == HTTP_OK);
    (void)snprintf(expected, sizeof(expected),
                   "http://127.0.0.1:%u/new/page.html", fixture_port);
    CHECK(response.final_url != NULL);
    CHECK(strcmp(response.final_url, expected) == 0);
    CHECK(html_extract_links(response.body, response.final_url, &links) == HTML_OK);
    http_response_free(&response);
    (void)snprintf(expected, sizeof(expected),
                   "http://127.0.0.1:%u/docs/notes.pdf?download=one&x=two",
                   fixture_port);
    CHECK(links.count == 1 && strcmp(links.urls[0], expected) == 0);
    html_links_free(&links);
    html_links_free(&links);
    CHECK(links.urls == NULL && links.count == 0);
    puts("PASS: HTTP redirect -> final base URL -> relative unquoted href");

    for (i = 0; i < sizeof(valid_types) / sizeof(valid_types[0]); ++i) {
        CHECK(download_path(valid_types[i], &response) == HTTP_OK);
        CHECK(response.body != NULL && response.size > 0);
        http_response_free(&response);
    }
    CHECK(download_path("/html-no-type", &response) == HTTP_OK);
    CHECK(response.final_url != NULL && response.content_type == NULL);
    http_response_free(&response);
    expect_failure("/fake-type", HTTP_ERR_NOT_HTML);
    expect_failure("/fake-xhtml", HTTP_ERR_NOT_HTML);
    expect_failure("/parameter-fake", HTTP_ERR_NOT_HTML);
    puts("PASS: exact HTML/XHTML MIME tokens, case/parameters and lookalikes");

    expect_failure("/binary", HTTP_ERR_NOT_HTML);
    expect_failure("/binary-no-type", HTTP_ERR_NOT_HTML);
    puts("PASS: binary PDF and missing-type binary bodies rejected");

    expect_failure("/to-file", HTTP_ERR_NETWORK);
    expect_failure("/to-ftp", HTTP_ERR_NETWORK);
    expect_failure("/loop", HTTP_ERR_TOO_MANY_REDIRECTS);
    puts("PASS: file/FTP redirects blocked and redirect-hop limit retained");

    CHECK(download_path("/at-limit", &response) == HTTP_OK);
    CHECK(response.size == HTTP_MAX_BODY_BYTES);
    CHECK(response.body != NULL && response.body[response.size] == '\0');
    http_response_free(&response);
    expect_failure("/over-limit", HTTP_ERR_TOO_LARGE);
    expect_failure("/over-limit-stream", HTTP_ERR_TOO_LARGE);
    puts("PASS: exact 5 MiB accepted; +1 byte rejected with/without Content-Length");
#ifdef HTTP_ALLOC_TEST
    test_metadata_allocations();
#endif
    printf("All local HTTP integration tests passed (%u checks).\n", checks);
    return 0;
}
#endif
