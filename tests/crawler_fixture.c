/* Deliberately small loopback-only C HTTP fixture, not production server code. */
#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
#include "crawler_fixture.h"
#include "http.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
unsigned crawler_fixture_start(void) { return 0; }
void crawler_fixture_stop(void) {}
unsigned crawler_fixture_refused_port(void) { return 0; }
size_t crawler_fixture_requests(unsigned *r, size_t n) { (void)r; (void)n; return 0; }
size_t crawler_fixture_body_size(unsigned r) { (void)r; return 0; }
#else
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static pid_t server = -1;
static int request_reader = -1, request_writer = -1, refused_socket = -1;
static unsigned port, refused_port, other_port;
static const char root[] =
    "<a href='/a'>A</a><a href='/b'>B</a><a href='/a#dup'>dup</a>"
    "<a href='/site#root'>cycle</a><a href='javascript:x'>bad</a>"
    "<iframe><a href='/leak'></iframe><script><a href='/leak'></script>"
    "<a broken='x' href=''><a href='unfinished";

static void require(int condition)
{
    if (!condition) {
        perror("crawler C fixture");
        exit(EXIT_FAILURE);
    }
}

void crawler_fixture_stop(void)
{
    if (server > 0) {
        int status;
        (void)kill(server, SIGTERM);
        while (waitpid(server, &status, 0) < 0 && errno == EINTR) {}
        server = -1;
    }
    if (request_reader >= 0) { (void)close(request_reader); request_reader = -1; }
    if (refused_socket >= 0) { (void)close(refused_socket); refused_socket = -1; }
    http_cleanup();
}

static unsigned route_id(const char *path)
{
    static const struct { const char *path; unsigned id; } routes[] = {
        {"/site", FIX_SITE}, {"/a", FIX_A}, {"/b", FIX_B}, {"/g", FIX_G},
        {"/empty", FIX_EMPTY}, {"/ok", FIX_OK}, {"/errors", FIX_ERRORS},
        {"/missing", FIX_MISSING}, {"/boom", FIX_BOOM}, {"/pdf", FIX_PDF},
        {"/nul", FIX_NUL}, {"/hang", FIX_HANG}, {"/redirect", FIX_REDIRECT},
        {"/new/page", FIX_NEW_PAGE}, {"/new/child", FIX_NEW_CHILD},
        {"/alias-site", FIX_ALIAS}, {"/alias-reverse", FIX_ALIAS_REVERSE},
        {"/off-redirect", FIX_OFF_REDIRECT}, {"/off-landing", FIX_OFF_LANDING},
        {"/off-child", FIX_OFF_CHILD}, {"/scope-strict", FIX_SCOPE_STRICT},
        {"/scope-open", FIX_SCOPE_OPEN}, {"/candidate-limit", FIX_CANDIDATE_LIMIT},
        {"/candidate-exact", FIX_CANDIDATE_EXACT}, {"/queue-limit", FIX_QUEUE_LIMIT},
        {"/leak", FIX_LEAK}, {"/drop", FIX_DROP},
        {"/port-redirect", FIX_PORT_REDIRECT}, {"/port-landing", FIX_PORT_LANDING},
        {"/failed-alias", FIX_FAILED_ALIAS}, {"/failed-redirect", FIX_FAILED_REDIRECT},
        {"/nul-alias", FIX_NUL_ALIAS}, {"/nul-redirect", FIX_NUL_REDIRECT}
    };
    size_t i;
    unsigned number;
    int consumed = 0;
    for (i = 0; i < sizeof(routes) / sizeof(routes[0]); ++i) {
        if (strcmp(path, routes[i].path) == 0) { return routes[i].id; }
    }
    if (sscanf(path, "/p/%u%n", &number, &consumed) == 1 &&
        path[consumed] == '\0' && number <= 150) {
        return FIX_CHAIN_BASE + number;
    }
    return FIX_MISSING;
}

static size_t build_body(unsigned id, char *body, size_t capacity)
{
    const char *text = "";
    int written;
    switch (id) {
    case FIX_SITE: text = root; break;
    case FIX_A: text = "<a href='g'>grand</a><a href='/b'>duplicate</a>"; break;
    case FIX_B: text = "<a href='/g#second'>grand</a>"; break;
    case FIX_G: text = "<a href='/site'>cycle</a>"; break;
    case FIX_OK: text = "ok"; break;
    case FIX_ERRORS:
        text = "<a href='/missing'>404</a><a href='/boom'>500</a>"
               "<a href='/pdf'>PDF</a><a href='/nul'>NUL</a>"
               "<a href='/hang'>timeout</a><a href='/ok'>ok</a>"
               "<a href='/missing#again'>no retry</a>";
        break;
    case FIX_PDF: text = "%PDF-x"; break;
    case FIX_NUL:
        memcpy(body, "<a\0>", 4);
        return 4;
    case FIX_NEW_PAGE: text = "<a href='child'>relative</a>"; break;
    case FIX_NEW_CHILD: case FIX_OFF_CHILD: text = "child"; break;
    case FIX_ALIAS:
        text = "<a href='/redirect'>alias</a><a href='/new/page'>direct</a>"
               "<a href='/alias-site#root'>cycle</a>";
        break;
    case FIX_ALIAS_REVERSE:
        text = "<a href='/new/page'>direct</a><a href='/redirect'>unknown alias</a>";
        break;
    case FIX_OFF_LANDING: text = "<a href='/off-child'>must not leak in strict mode</a>"; break;
    case FIX_PORT_LANDING: text = "<a href='/leak'>other service must not be parsed</a>"; break;
    case FIX_FAILED_ALIAS:
        text = "<a href='/failed-redirect'>alias</a><a href='/missing'>direct failed target</a>";
        break;
    case FIX_NUL_ALIAS:
        text = "<a href='/nul-redirect'>alias</a><a href='/nul'>direct malformed target</a>";
        break;
    case FIX_SCOPE_STRICT:
        written = snprintf(body, capacity,
            "<a href='/ok'>ok</a><a href='http://localhost:%u/off-landing'>off</a>"
            "<a href='http://sub.localhost:%u/leak'>subdomain</a>"
            "<a href='http://127.0.0.1.evil.invalid:%u/leak'>lookalike</a>"
            "<a href='http://127.0.0.1:%u/leak'>other port</a>"
            "<a href='http://127.0.0.1/leak'>default port</a>",
            port, port, port, other_port);
        require(written >= 0 && (size_t)written < capacity);
        return (size_t)written;
    case FIX_SCOPE_OPEN:
        written = snprintf(body, capacity,
            "<a href='/ok'>ok</a><a href='http://localhost:%u/off-landing'>off</a>"
            "<a href='http://127.0.0.1:%u/port-landing'>other port</a>", port, other_port);
        require(written >= 0 && (size_t)written < capacity);
        return (size_t)written;
    case FIX_CANDIDATE_LIMIT: case FIX_CANDIDATE_EXACT: case FIX_QUEUE_LIMIT: {
        size_t count = id == FIX_CANDIDATE_LIMIT ? 1001 : 1000;
        size_t i, used = 0;
        for (i = 0; i < count; ++i) {
            written = id == FIX_QUEUE_LIMIT
                ? snprintf(body + used, capacity - used, "<a href='/p/%zu'>x</a>", i)
                : snprintf(body + used, capacity - used, "<a href='/ok'>x</a>");
            require(written >= 0 && (size_t)written < capacity - used);
            used += (size_t)written;
        }
        return used;
    }
    case FIX_LEAK: text = "unexpected leak"; break;
    default:
        if (id >= FIX_CHAIN_BASE && id < FIX_CHAIN_BASE + 150) {
            written = snprintf(body, capacity, "<a href='/p/%u'>next</a>",
                               id - FIX_CHAIN_BASE + 1);
            require(written >= 0 && (size_t)written < capacity);
            return (size_t)written;
        }
        break;
    }
    require(strlen(text) < capacity);
    memcpy(body, text, strlen(text) + 1);
    return strlen(text);
}

size_t crawler_fixture_body_size(unsigned route)
{
    char body[65536];
    return build_body(route, body, sizeof(body));
}

static int send_all(int fd, const char *bytes, size_t size)
{
    while (size != 0) {
        ssize_t sent = send(fd, bytes, size, 0);
        if (sent < 0 && errno == EINTR) { continue; }
        if (sent <= 0) { return 0; }
        bytes += sent;
        size -= (size_t)sent;
    }
    return 1;
}

static void serve(int fd, unsigned id)
{
    char body[65536], header[512], location[256];
    size_t size = build_body(id, body, sizeof(body));
    int status = id == FIX_MISSING ? 404 : id == FIX_BOOM ? 500 : 200;
    const char *type = id == FIX_PDF ? "application/pdf" : "text/html";
    int written;
    if (id == FIX_DROP) { return; } /* Empty transport response, no HTTP header. */
    location[0] = '\0';
    if (id == FIX_HANG) {
        struct timespec delay = {1, 500000000L};
        while (nanosleep(&delay, &delay) < 0 && errno == EINTR) {}
    }
    if (id == FIX_REDIRECT || id == FIX_OFF_REDIRECT || id == FIX_PORT_REDIRECT ||
        id == FIX_FAILED_REDIRECT || id == FIX_NUL_REDIRECT) {
        if (id == FIX_REDIRECT) {
            (void)snprintf(location, sizeof(location), "Location: /new/page\r\n");
        } else if (id == FIX_OFF_REDIRECT) {
            (void)snprintf(location, sizeof(location),
                           "Location: http://localhost:%u/off-landing\r\n", port);
        } else if (id == FIX_PORT_REDIRECT) {
            (void)snprintf(location, sizeof(location),
                           "Location: http://127.0.0.1:%u/port-landing\r\n", other_port);
        } else {
            (void)snprintf(location, sizeof(location), "Location: %s\r\n",
                           id == FIX_FAILED_REDIRECT ? "/missing" : "/nul");
        }
        status = 302;
        size = 0;
    }
    written = snprintf(header, sizeof(header),
        "HTTP/1.1 %d Fixture\r\nContent-Type: %s\r\n%s"
        "Content-Length: %zu\r\nConnection: close\r\n\r\n", status, type, location, size);
    require(written > 0 && (size_t)written < sizeof(header));
    if (send_all(fd, header, (size_t)written)) { (void)send_all(fd, body, size); }
}

static void server_loop(int listener, int other_listener)
{
    (void)signal(SIGPIPE, SIG_IGN);
    for (;;) {
        char request[4096] = {0}, path[128];
        size_t used = 0;
        struct timeval timeout = {5, 0};
        fd_set ready;
        int fd, selected;
        FD_ZERO(&ready);
        FD_SET(listener, &ready);
        FD_SET(other_listener, &ready);
        selected = select((listener > other_listener ? listener : other_listener) + 1,
                          &ready, NULL, NULL, NULL);
        if (selected < 0) { if (errno == EINTR) { continue; } _exit(2); }
        fd = accept(FD_ISSET(listener, &ready) ? listener : other_listener, NULL, NULL);
        if (fd < 0) { if (errno == EINTR) { continue; } _exit(2); }
        (void)setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        while (used < sizeof(request) - 1 && strstr(request, "\r\n\r\n") == NULL) {
            ssize_t got = recv(fd, request + used, sizeof(request) - used - 1, 0);
            if (got < 0 && errno == EINTR) { continue; }
            if (got <= 0) { break; }
            used += (size_t)got;
            request[used] = '\0';
        }
        if (sscanf(request, "GET %127s", path) == 1) {
            unsigned id = route_id(path);
            unsigned record = id | (strstr(request, "Host: localhost:") != NULL
                                   ? FIX_LOCALHOST_BIT : 0);
            /* 4-byte atomic records: even the 100-page case cannot fill pipe. */
            require(write(request_writer, &record, sizeof(record)) == (ssize_t)sizeof(record));
            serve(fd, id);
        }
        (void)close(fd);
    }
}

unsigned crawler_fixture_start(void)
{
    struct sockaddr_in address = {0};
    socklen_t size = sizeof(address);
    int listener = socket(AF_INET, SOCK_STREAM, 0), other_listener, pipes[2];
    require(listener >= 0);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    require(bind(listener, (struct sockaddr *)&address, sizeof(address)) == 0);
    require(getsockname(listener, (struct sockaddr *)&address, &size) == 0);
    port = ntohs(address.sin_port);
    require(listen(listener, 16) == 0);
    refused_socket = socket(AF_INET, SOCK_STREAM, 0);
    address.sin_port = 0;
    require(refused_socket >= 0);
    require(bind(refused_socket, (struct sockaddr *)&address, sizeof(address)) == 0);
    require(getsockname(refused_socket, (struct sockaddr *)&address, &size) == 0);
    refused_port = ntohs(address.sin_port); /* Reserved but never listening. */
    other_listener = socket(AF_INET, SOCK_STREAM, 0);
    address.sin_port = 0;
    require(other_listener >= 0);
    require(bind(other_listener, (struct sockaddr *)&address, sizeof(address)) == 0);
    require(getsockname(other_listener, (struct sockaddr *)&address, &size) == 0);
    other_port = ntohs(address.sin_port);
    require(listen(other_listener, 16) == 0);
    require(pipe(pipes) == 0);
    request_reader = pipes[0];
    request_writer = pipes[1];
    server = fork();
    require(server >= 0);
    if (server == 0) {
        (void)close(request_reader);
        server_loop(listener, other_listener);
        _exit(0);
    }
    (void)close(listener);
    (void)close(other_listener);
    (void)close(request_writer);
    require(fcntl(request_reader, F_SETFL, O_NONBLOCK) == 0);
    require(atexit(crawler_fixture_stop) == 0);
    require(setenv("no_proxy", "127.0.0.1,localhost", 1) == 0);
    return port;
}

unsigned crawler_fixture_refused_port(void) { return refused_port; }

size_t crawler_fixture_requests(unsigned *records, size_t capacity)
{
    size_t count = 0;
    for (;;) {
        unsigned record;
        ssize_t got = read(request_reader, &record, sizeof(record));
        if (got < 0 && errno == EINTR) { continue; }
        if (got < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) { break; }
        if (got == 0) { break; }
        require(got == (ssize_t)sizeof(record) && count < capacity);
        records[count++] = record;
    }
    return count;
}
#endif
