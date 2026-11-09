#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
#include "crawler_fixture.h"
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
int main(void)
{
    puts("SKIP: controlled fixture launcher requires POSIX; native Windows NOT VERIFIED.");
    return 0;
}
#else
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
int main(int argc, char **argv)
{
    unsigned port = crawler_fixture_start();
    char url[128];
    int document_demo = argc == 2 && strcmp(argv[1], "--documents") == 0;
    int status;
    pid_t child;
    (void)snprintf(url, sizeof(url), document_demo
                   ? "http://127.0.0.1:%u/doc-home"
                   : "http://127.0.0.1:%u/site", port);
    printf("Controlled C fixture: %s\n", url);
    (void)fflush(stdout);
    if (argc == 2 && (strcmp(argv[1], "--demo") == 0 || document_demo)) {
        child = fork();
        if (child < 0) { crawler_fixture_stop(); return 1; }
        if (child == 0) {
            execl(document_demo ? "./build/discover-documents" : "./build/crawler-bfs",
                  document_demo ? "discover-documents" : "crawler-bfs",
                  url, "2", "1", (char *)NULL);
            _exit(127);
        }
        while (waitpid(child, &status, 0) < 0) {
            if (errno != EINTR) { crawler_fixture_stop(); return 1; }
        }
        crawler_fixture_stop();
        return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
    }
    if (argc != 1) { crawler_fixture_stop(); return 2; }
    puts("In another terminal run build/crawler-bfs with that URL. Press Enter here to stop.");
    (void)getchar();
    crawler_fixture_stop();
    return 0;
}
#endif
