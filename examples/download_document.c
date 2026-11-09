#include "download.h"
#include "http.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

static int parse_long(const char *text, long *value)
{
    char *end = NULL;
    long parsed;
    errno = 0;
    parsed = strtol(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0') { return 0; }
    *value = parsed;
    return 1;
}

static void print_usage(const char *program)
{
    fprintf(stderr,
        "Usage: %s DOCUMENT_URL SEED_URL OUTPUT_DIRECTORY SAFE_FILENAME "
        "SAME_DOMAIN:0|1 [TIMEOUT_SECONDS:1..3600]\n",
        program);
}

int main(int argc, char **argv)
{
    DownloadConfig config;
    DownloadResult result = {0};
    long same_domain = 1;
    long timeout = 3;
    DownloadStatus status;

    if (argc < 6 || argc > 7) {
        print_usage(argv[0]);
        return 2;
    }
    if (!parse_long(argv[5], &same_domain) ||
        (same_domain != 0 && same_domain != 1)) {
        print_usage(argv[0]);
        return 2;
    }
    if (argc == 7 && !parse_long(argv[6], &timeout)) {
        print_usage(argv[0]);
        return 2;
    }

    config.document_url = argv[1];
    config.seed_url = argv[2];
    config.output_directory = argv[3];
    config.filename = argv[4];
    config.same_domain = (int)same_domain;
    config.timeout_seconds = timeout;

    status = download_document(&config, &result);
    printf("Download status: %s\n", download_status_message(status));
    if (result.detail[0] != '\0') {
        printf("Detail: %s\n", result.detail);
    }
    printf("Document type: %s\n", document_type_name(result.type));
    printf("HTTP status: %ld\n", result.http_status);
    printf("Bytes written: %zu\n", result.bytes_written);
    if (result.final_url != NULL) {
        printf("Final URL: %s\n", result.final_url);
    }
    if (result.file_path != NULL) {
        printf("File path: %s\n", result.file_path);
    }
    download_result_free(&result);
    http_cleanup();
    return status == DOWNLOAD_OK ? 0 : 1;
}
