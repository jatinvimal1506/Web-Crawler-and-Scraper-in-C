/* C-only POSIX loopback server for deterministic binary-download tests. */
#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
#include "download_fixture.h"

#ifdef _WIN32
unsigned download_fixture_start(void) { return 0; }
unsigned download_fixture_other_port(void) { return 0; }
void download_fixture_stop(void) {}
void download_fixture_reset_counts(void) {}
size_t download_fixture_request_count(unsigned route) { (void)route; return 0; }
const unsigned char *download_fixture_small_data(size_t *size)
{
    static const unsigned char data[] = {0, 255, 128, 65, 10, 0, 239, 66};
    if (size != NULL) { *size = sizeof(data); }
    return data;
}
unsigned char download_fixture_expected_byte(size_t offset)
{
    return (unsigned char)((offset * 37U + (offset >> 8) * 13U + 0xA5U) & 0xffU);
}
#else
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define EXACT_SIZE ((size_t)20 * 1024 * 1024)

static pid_t server_pid = -1;
static int request_reader = -1;
static int request_writer = -1;
static unsigned primary_port;
static unsigned secondary_port;
static size_t route_counts[DFX_ROUTE_COUNT];

static void require(int condition)
{
    if (!condition) {
        perror("download fixture");
        exit(EXIT_FAILURE);
    }
}

static int open_listener(unsigned *port)
{
    struct sockaddr_in address = {0};
    socklen_t length = sizeof(address);
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    int reuse = 1;
    require(fd >= 0);
    require(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) == 0);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;
    require(bind(fd, (struct sockaddr *)&address, sizeof(address)) == 0);
    require(getsockname(fd, (struct sockaddr *)&address, &length) == 0);
    require(listen(fd, 16) == 0);
    *port = ntohs(address.sin_port);
    return fd;
}

static unsigned identify_route(char *path)
{
    static const struct { const char *path; unsigned route; } routes[] = {
        {"/files/small.pdf", DFX_PDF},
        {"/files/word.docx", DFX_DOCX},
        {"/files/archive.zip", DFX_ZIP},
        {"/files/archive-alt.zip", DFX_ZIP_ALT},
        {"/files/octet.pdf", DFX_OCTET},
        {"/files/html.pdf", DFX_HTML},
        {"/files/wrong.pdf", DFX_WRONG_MIME},
        {"/files/duplicate-type.pdf", DFX_DUPLICATE_MIME},
        {"/files/no-type.pdf", DFX_MISSING_MIME},
        {"/files/empty.pdf", DFX_EMPTY},
        {"/files/not-found.pdf", DFX_NOT_FOUND},
        {"/files/server-error.pdf", DFX_SERVER_ERROR},
        {"/files/partial-content.pdf", DFX_PARTIAL_CONTENT},
        {"/files/partial.pdf", DFX_PARTIAL},
        {"/files/slow.pdf", DFX_SLOW},
        {"/files/drop.pdf", DFX_DROP},
        {"/redir/start.pdf", DFX_RELATIVE_REDIRECT},
        {"/r301.pdf", DFX_REDIRECT_301},
        {"/r302.pdf", DFX_REDIRECT_302},
        {"/r303.pdf", DFX_REDIRECT_303},
        {"/r307.pdf", DFX_REDIRECT_307},
        {"/r308.pdf", DFX_REDIRECT_308},
        {"/off-host.pdf", DFX_OFF_HOST_REDIRECT},
        {"/off-port.pdf", DFX_OFF_PORT_REDIRECT},
        {"/localhost-redirect.pdf", DFX_LOCALHOST_REDIRECT},
        {"/host-target.pdf", DFX_HOST_TARGET},
        {"/port-target.pdf", DFX_PORT_TARGET},
        {"/loop-a.pdf", DFX_LOOP_A},
        {"/loop-b.pdf", DFX_LOOP_B},
        {"/ftp.pdf", DFX_FTP_REDIRECT},
        {"/file.pdf", DFX_FILE_REDIRECT},
        {"/missing-location.pdf", DFX_MISSING_LOCATION},
        {"/malformed-location.pdf", DFX_MALFORMED_LOCATION},
        {"/redirect-404.pdf", DFX_REDIRECT_404},
        {"/redirect-wrong.pdf", DFX_REDIRECT_WRONG_MIME},
        {"/files/exact.pdf", DFX_EXACT_LENGTH},
        {"/files/exact-chunked.pdf", DFX_EXACT_CHUNKED},
        {"/files/oversize.pdf", DFX_OVERSIZE_LENGTH},
        {"/files/oversize-chunked.pdf", DFX_OVERSIZE_CHUNKED}
    };
    size_t i;
    unsigned number;
    int consumed = 0;
    char *query = strchr(path, '?');
    if (query != NULL) { *query = '\0'; }
    for (i = 0; i < sizeof(routes) / sizeof(routes[0]); ++i) {
        if (strcmp(path, routes[i].path) == 0) { return routes[i].route; }
    }
    if (sscanf(path, "/hop/%u.pdf%n", &number, &consumed) == 1 &&
        path[consumed] == '\0' && number < 7) {
        return DFX_HOP_BASE + number;
    }
    return 0;
}

const unsigned char *download_fixture_small_data(size_t *size)
{
    static const unsigned char data[] = {0, 255, 128, 65, 10, 0, 239, 66};
    if (size != NULL) { *size = sizeof(data); }
    return data;
}

unsigned char download_fixture_expected_byte(size_t offset)
{
    return (unsigned char)((offset * 37U + (offset >> 8) * 13U + 0xA5U) & 0xffU);
}

static int send_all(int fd, const void *data, size_t length)
{
    const unsigned char *bytes = data;
    while (length != 0) {
        ssize_t sent = send(fd, bytes, length, 0);
        if (sent < 0 && errno == EINTR) { continue; }
        if (sent <= 0) { return 0; }
        bytes += (size_t)sent;
        length -= (size_t)sent;
    }
    return 1;
}

static int send_pattern(int fd, size_t total, int chunked)
{
    unsigned char buffer[8192];
    size_t offset = 0;
    while (offset < total) {
        size_t amount = total - offset;
        size_t i;
        char chunk_header[32];
        int header_length;
        if (amount > sizeof(buffer)) { amount = sizeof(buffer); }
        for (i = 0; i < amount; ++i) {
            buffer[i] = download_fixture_expected_byte(offset + i);
        }
        if (chunked) {
            header_length = snprintf(chunk_header, sizeof(chunk_header),
                                     "%zx\r\n", amount);
            if (header_length <= 0 ||
                !send_all(fd, chunk_header, (size_t)header_length)) { return 0; }
        }
        if (!send_all(fd, buffer, amount)) { return 0; }
        if (chunked && !send_all(fd, "\r\n", 2)) { return 0; }
        offset += amount;
    }
    return !chunked || send_all(fd, "0\r\n\r\n", 5);
}

static void serve_request(int fd, unsigned route)
{
    static const unsigned char small[] = {0, 255, 128, 65, 10, 0, 239, 66};
    const char *mime = "application/pdf";
    const char *location = NULL;
    char header[1024];
    size_t body_size = sizeof(small), advertised_size = body_size;
    int status = 200, chunked = 0, written;
    int special = 0;
    if (route == DFX_DOCX) { mime = "application/vnd.openxmlformats-officedocument.wordprocessingml.document"; }
    if (route == DFX_ZIP) { mime = "application/zip"; }
    if (route == DFX_ZIP_ALT) { mime = "application/x-zip-compressed; version=1"; }
    if (route == DFX_OCTET) { mime = "application/octet-stream"; }
    if (route == DFX_HTML) { mime = "text/html"; }
    if (route == DFX_WRONG_MIME) { mime = "image/png"; }
    if (route == DFX_DUPLICATE_MIME) { mime = "application/pdf"; }
    if (route == DFX_MISSING_MIME) { mime = NULL; }
    if (route == DFX_EMPTY) { body_size = 0; advertised_size = 0; }
    if (route == DFX_NOT_FOUND) { status = 404; }
    if (route == DFX_SERVER_ERROR) { status = 500; }
    if (route == DFX_PARTIAL_CONTENT) { status = 206; }
    if (route == DFX_PARTIAL) { advertised_size = 64; body_size = 8; special = 1; }
    if (route == DFX_SLOW) { advertised_size = 8; body_size = 0; special = 2; }
    if (route == DFX_DROP) { return; }

    if (route == DFX_RELATIVE_REDIRECT) { location = "../files/small.pdf"; status = 302; }
    if (route >= DFX_REDIRECT_301 && route <= DFX_REDIRECT_308) {
        static const int codes[] = {301, 302, 303, 307, 308};
        status = codes[route - DFX_REDIRECT_301];
        location = "/files/small.pdf";
    }
    if (route == DFX_OFF_HOST_REDIRECT || route == DFX_LOCALHOST_REDIRECT) {
        static char target[128];
        (void)snprintf(target, sizeof(target), "http://%s:%u/host-target.pdf",
            route == DFX_OFF_HOST_REDIRECT ? "localhost" : "localhost", primary_port);
        location = target; status = 302;
    }
    if (route == DFX_OFF_PORT_REDIRECT) {
        static char target[128];
        (void)snprintf(target, sizeof(target), "http://127.0.0.1:%u/port-target.pdf",
                       secondary_port);
        location = target; status = 302;
    }
    if (route == DFX_LOOP_A) { location = "/loop-b.pdf"; status = 302; }
    if (route == DFX_LOOP_B) { location = "/loop-a.pdf"; status = 302; }
    if (route == DFX_FTP_REDIRECT) { location = "ftp://127.0.0.1/file.pdf"; status = 302; }
    if (route == DFX_FILE_REDIRECT) { location = "file:///tmp/blocked.pdf"; status = 302; }
    if (route == DFX_REDIRECT_404) { location = "/files/not-found.pdf"; status = 302; }
    if (route == DFX_REDIRECT_WRONG_MIME) { location = "/files/wrong.pdf"; status = 302; }
    if (route == DFX_MISSING_LOCATION) { status = 302; }
    if (route == DFX_MALFORMED_LOCATION) { location = "http://[invalid"; status = 302; }
    if (route >= DFX_HOP_BASE && route < DFX_HOP_BASE + 6) {
        static char target[64];
        (void)snprintf(target, sizeof(target), "/hop/%u.pdf",
                       route - DFX_HOP_BASE + 1);
        location = target; status = 302;
    }
    if (route == DFX_EXACT_LENGTH || route == DFX_EXACT_CHUNKED ||
        route == DFX_OVERSIZE_LENGTH || route == DFX_OVERSIZE_CHUNKED) {
        body_size = route == DFX_EXACT_LENGTH || route == DFX_EXACT_CHUNKED
            ? EXACT_SIZE : EXACT_SIZE + 1;
        advertised_size = body_size;
        chunked = route == DFX_EXACT_CHUNKED || route == DFX_OVERSIZE_CHUNKED;
    }
    if (route == DFX_HOP_BASE + 6) { body_size = sizeof(small); }
    if (location != NULL) { body_size = 0; advertised_size = 0; }

    written = snprintf(header, sizeof(header), "HTTP/1.1 %d Fixture\r\n", status);
    require(written > 0 && (size_t)written < sizeof(header));
    if (mime != NULL) {
        written = snprintf(header + strlen(header), sizeof(header) - strlen(header),
                           "Content-Type: %s\r\n", mime);
        require(written > 0 && (size_t)written < sizeof(header));
    }
    if (route == DFX_DUPLICATE_MIME) {
        written = snprintf(header + strlen(header), sizeof(header) - strlen(header),
                           "Content-Type: application/octet-stream\r\n");
        require(written > 0 && (size_t)written < sizeof(header));
    }
    if (location != NULL) {
        written = snprintf(header + strlen(header), sizeof(header) - strlen(header),
                           "Location: %s\r\n", location);
        require(written > 0 && (size_t)written < sizeof(header));
    }
    if (chunked) {
        written = snprintf(header + strlen(header), sizeof(header) - strlen(header),
                           "Transfer-Encoding: chunked\r\n");
        require(written > 0 && (size_t)written < sizeof(header));
    } else {
        written = snprintf(header + strlen(header), sizeof(header) - strlen(header),
                           "Content-Length: %zu\r\n", advertised_size);
        require(written > 0 && (size_t)written < sizeof(header));
    }
    written = snprintf(header + strlen(header), sizeof(header) - strlen(header),
                       "Connection: close\r\n\r\n");
    require(written > 0 && (size_t)written < sizeof(header));
    require(send_all(fd, header, strlen(header)));
    if (location != NULL) { return; }
    if (special == 1) {
        require(send_all(fd, small, body_size));
        return;
    }
    if (special == 2) {
        struct timespec delay = {2, 0};
        while (nanosleep(&delay, &delay) < 0 && errno == EINTR) {}
        (void)send_all(fd, small, sizeof(small));
        return;
    }
    if (body_size == 0) { return; }
    if (chunked) {
        (void)send_pattern(fd, body_size, 1);
    } else if (route == DFX_EXACT_LENGTH || route == DFX_OVERSIZE_LENGTH) {
        (void)send_pattern(fd, body_size, 0);
    } else {
        size_t offset = 0;
        while (offset < body_size) {
            size_t amount = body_size - offset;
            if (amount > 3) { amount = 3; }
            if (!send_all(fd, small + offset, amount)) { break; }
            offset += amount;
        }
    }
}

static void server_loop(int primary_listener, int secondary_listener)
{
    (void)signal(SIGPIPE, SIG_IGN);
    for (;;) {
        fd_set readers;
        int maximum = primary_listener > secondary_listener
            ? primary_listener : secondary_listener;
        FD_ZERO(&readers);
        FD_SET(primary_listener, &readers);
        FD_SET(secondary_listener, &readers);
        if (select(maximum + 1, &readers, NULL, NULL, NULL) < 0) {
            if (errno == EINTR) { continue; }
            _exit(2);
        }
        {
            int listener = FD_ISSET(primary_listener, &readers)
                ? primary_listener : secondary_listener;
            int client = accept(listener, NULL, NULL);
            char request[2048] = {0}, path[256] = {0};
            size_t used = 0;
            unsigned route;
            if (client < 0) { if (errno == EINTR) { continue; } _exit(3); }
            while (used < sizeof(request) - 1 &&
                   strstr(request, "\r\n\r\n") == NULL) {
                ssize_t got = recv(client, request + used,
                                   sizeof(request) - used - 1, 0);
                if (got < 0 && errno == EINTR) { continue; }
                if (got <= 0) { break; }
                used += (size_t)got;
                request[used] = '\0';
            }
            if (sscanf(request, "GET %255s", path) == 1) {
                route = identify_route(path);
                if (route != 0) {
                    (void)write(request_writer, &route, sizeof(route));
                    serve_request(client, route);
                }
            }
            (void)close(client);
        }
    }
}

unsigned download_fixture_start(void)
{
    int primary = open_listener(&primary_port);
    int secondary = open_listener(&secondary_port);
    int descriptors[2];
    require(pipe(descriptors) == 0);
    request_reader = descriptors[0];
    request_writer = descriptors[1];
    require(fcntl(request_reader, F_SETFL,
                  fcntl(request_reader, F_GETFL) | O_NONBLOCK) == 0);
    server_pid = fork();
    require(server_pid >= 0);
    if (server_pid == 0) {
        (void)close(request_reader);
        server_loop(primary, secondary);
        _exit(0);
    }
    (void)close(primary);
    (void)close(secondary);
    (void)setenv("no_proxy", "127.0.0.1,localhost", 1);
    (void)setenv("NO_PROXY", "127.0.0.1,localhost", 1);
    return primary_port;
}

unsigned download_fixture_other_port(void) { return secondary_port; }

void download_fixture_stop(void)
{
    if (server_pid > 0) {
        int status;
        (void)kill(server_pid, SIGTERM);
        while (waitpid(server_pid, &status, 0) < 0 && errno == EINTR) {}
        server_pid = -1;
    }
    if (request_reader >= 0) { (void)close(request_reader); request_reader = -1; }
    if (request_writer >= 0) { (void)close(request_writer); request_writer = -1; }
}

static void drain_counts(void)
{
    for (;;) {
        unsigned route;
        ssize_t got = read(request_reader, &route, sizeof(route));
        if (got < 0 && errno == EINTR) { continue; }
        if (got < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) { break; }
        if (got == 0) { break; }
        require(got == (ssize_t)sizeof(route));
        if (route < DFX_ROUTE_COUNT) { ++route_counts[route]; }
    }
}

void download_fixture_reset_counts(void)
{
    drain_counts();
    memset(route_counts, 0, sizeof(route_counts));
}

size_t download_fixture_request_count(unsigned route)
{
    drain_counts();
    return route < DFX_ROUTE_COUNT ? route_counts[route] : 0;
}
#endif
