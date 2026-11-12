
#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
#include "download.h"
#include "http_internal.h"
#include "url.h"

#include <curl/curl.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#define DD_OPEN _open
#define DD_CLOSE _close
#define DD_FDOPEN _fdopen
#define DD_BINARY_FLAG _O_BINARY
#define DD_PATH_SEPARATOR '\\'
typedef struct _stat DownloadStat;
#define DOWNLOAD_STAT _stat
#define DOWNLOAD_IS_DIRECTORY(mode) (((mode) & _S_IFMT) == _S_IFDIR)
#else
#include <fcntl.h>
#include <unistd.h>
#define DD_OPEN open
#define DD_CLOSE close
#define DD_FDOPEN fdopen
#define DD_BINARY_FLAG 0
#define DD_PATH_SEPARATOR '/'
typedef struct stat DownloadStat;
#define DOWNLOAD_STAT stat
#define DOWNLOAD_IS_DIRECTORY(mode) S_ISDIR(mode)
#endif

#define DOWNLOAD_MAX_REDIRECTS 5U
#define DOWNLOAD_LOCATION_CAPACITY 2049U
#define DOWNLOAD_CONTENT_TYPE_CAPACITY 512U

typedef struct {
    FILE *file;
    DocumentType expected_type;
    size_t bytes_written;
    long response_code;
    char location[DOWNLOAD_LOCATION_CAPACITY];
    size_t location_count;
    int location_invalid;
    char content_type[DOWNLOAD_CONTENT_TYPE_CAPACITY];
    size_t content_type_count;
    int content_type_invalid;
    int size_overflow;
    int too_large;
    int write_failed;
} DownloadTransfer;

static void set_result(DownloadResult *result, DownloadStatus status,
                       const char *detail)
{
    result->status = status;
    if (detail == NULL) { detail = download_status_message(status); }
    (void)snprintf(result->detail, sizeof(result->detail), "%s", detail);
}

static int result_is_empty(const DownloadResult *result)
{
    return result->status == DOWNLOAD_OK &&
           result->type == DOCUMENT_TYPE_UNKNOWN &&
           result->http_status == 0 && result->bytes_written == 0 &&
           result->final_url == NULL && result->file_path == NULL &&
           result->detail[0] == '\0';
}

const char *download_status_message(DownloadStatus status)
{
    switch (status) {
    case DOWNLOAD_OK: return "download completed";
    case DOWNLOAD_ERROR_INVALID_ARGUMENT: return "invalid request or result";
    case DOWNLOAD_ERROR_UNSUPPORTED_TYPE: return "URL is not a supported document type";
    case DOWNLOAD_ERROR_OUT_OF_MEMORY: return "out of memory";
    case DOWNLOAD_ERROR_INIT: return "could not initialize libcurl";
    case DOWNLOAD_ERROR_TIMEOUT: return "request timed out";
    case DOWNLOAD_ERROR_NETWORK: return "network transfer failed";
    case DOWNLOAD_ERROR_REDIRECT: return "redirect was invalid, unsafe, repeated, or excessive";
    case DOWNLOAD_ERROR_SCOPE: return "URL is outside the requested host/port scope";
    case DOWNLOAD_ERROR_HTTP_STATUS: return "server returned an unsuccessful HTTP status";
    case DOWNLOAD_ERROR_CONTENT_TYPE: return "response Content-Type is missing, ambiguous, or invalid";
    case DOWNLOAD_ERROR_TOO_LARGE: return "document exceeds the 20 MiB limit";
    case DOWNLOAD_ERROR_EMPTY: return "document response is empty";
    case DOWNLOAD_ERROR_INCOMPLETE: return "document response was partial or interrupted";
    case DOWNLOAD_ERROR_EXISTS: return "destination file already exists";
    case DOWNLOAD_ERROR_WRITE: return "could not write the complete file";
    case DOWNLOAD_ERROR_FILE: return "could not open, close, or inspect the output file";
    case DOWNLOAD_ERROR_CLEANUP: return "failed to remove the incomplete output file";
    case DOWNLOAD_ERROR_OVERFLOW: return "size arithmetic overflow";
    default: return "unknown document-download error";
    }
}

DownloadStatus download_result_init(DownloadResult *result)
{
    if (result == NULL || !result_is_empty(result)) {
        return DOWNLOAD_ERROR_INVALID_ARGUMENT;
    }
    *result = (DownloadResult){0};
    return DOWNLOAD_OK;
}

void download_result_free(DownloadResult *result)
{
    if (result == NULL) { return; }
    free(result->final_url);
    free(result->file_path);
    *result = (DownloadResult){0};
}

static int ascii_equal_nocase(const char *left, const char *right)
{
    while (*left != '\0' && *right != '\0') {
        unsigned char a = (unsigned char)*left++;
        unsigned char b = (unsigned char)*right++;
        if (a >= 'A' && a <= 'Z') { a = (unsigned char)(a + ('a' - 'A')); }
        if (b >= 'A' && b <= 'Z') { b = (unsigned char)(b + ('a' - 'A')); }
        if (a != b) { return 0; }
    }
    return *left == '\0' && *right == '\0';
}

static int ascii_prefix_nocase(const char *value, const char *prefix, size_t n)
{
    size_t i;
    for (i = 0; i < n; ++i) {
        unsigned char a = (unsigned char)value[i];
        unsigned char b = (unsigned char)prefix[i];
        if (a >= 'A' && a <= 'Z') { a = (unsigned char)(a + ('a' - 'A')); }
        if (b >= 'A' && b <= 'Z') { b = (unsigned char)(b + ('a' - 'A')); }
        if (a != b) { return 0; }
    }
    return 1;
}

static const char *expected_mime(DocumentType type)
{
    switch (type) {
    case DOCUMENT_TYPE_PDF: return "application/pdf";
    case DOCUMENT_TYPE_DOCX:
        return "application/vnd.openxmlformats-officedocument.wordprocessingml.document";
    case DOCUMENT_TYPE_ZIP: return "application/zip";
    default: return NULL;
    }
}

static int media_type_is(const char *content_type, const char *expected)
{
    size_t length, token_length;
    if (content_type == NULL || expected == NULL) { return 0; }
    length = strlen(content_type);
    while (length != 0 &&
           (content_type[length - 1] == ' ' || content_type[length - 1] == '\t' ||
            content_type[length - 1] == '\r' || content_type[length - 1] == '\n')) {
        --length;
    }
    token_length = 0;
    while (token_length < length && content_type[token_length] != ';') {
        ++token_length;
    }
    while (token_length != 0 &&
           (content_type[token_length - 1] == ' ' ||
            content_type[token_length - 1] == '\t')) {
        --token_length;
    }
    {
        size_t start = 0, expected_length = strlen(expected);
        while (start < token_length &&
               (content_type[start] == ' ' || content_type[start] == '\t')) {
            ++start;
        }
        if (token_length - start == expected_length &&
            ascii_prefix_nocase(content_type + start, expected, expected_length)) {
            return 1;
        }
        return 0;
    }
}

static int content_type_is_valid(const DownloadTransfer *transfer)
{
    const char *expected = expected_mime(transfer->expected_type);
    if (transfer->content_type_count != 1 ||
        transfer->content_type_invalid || expected == NULL) {
        return 0;
    }
    if (media_type_is(transfer->content_type, expected) ||
        media_type_is(transfer->content_type, "application/octet-stream")) {
        return 1;
    }
    return transfer->expected_type == DOCUMENT_TYPE_ZIP &&
        media_type_is(transfer->content_type, "application/x-zip-compressed");
}

static int redirect_status(long status)
{
    return status == 301 || status == 302 || status == 303 ||
           status == 307 || status == 308;
}

static int parse_status_line(const char *line, size_t length, long *status)
{
    size_t position = 0;
    long value = 0;
    int digits = 0;
    if (length < 5 || memcmp(line, "HTTP/", 5) != 0) { return 0; }
    while (position < length && line[position] != ' ' && line[position] != '\t') {
        ++position;
    }
    while (position < length && (line[position] == ' ' || line[position] == '\t')) {
        ++position;
    }
    while (position < length && line[position] >= '0' && line[position] <= '9') {
        if (value > 9999) { return 0; }
        value = value * 10 + (line[position] - '0');
        ++position;
        ++digits;
    }
    if (digits < 3 || value < 100 || value > 599) { return 0; }
    *status = value;
    return 1;
}

static void reset_response_headers(DownloadTransfer *transfer, long status)
{
    transfer->response_code = status;
    transfer->location[0] = '\0';
    transfer->location_count = 0;
    transfer->location_invalid = 0;
    transfer->content_type[0] = '\0';
    transfer->content_type_count = 0;
    transfer->content_type_invalid = 0;
}

static int copy_header_value(char *output, size_t capacity,
                             const char *line, size_t length)
{
    size_t start, end;
    if (capacity == 0 || memchr(line, '\0', length) != NULL) { return 0; }
    start = 0;
    end = length;
    while (start < end && (line[start] == ' ' || line[start] == '\t')) { ++start; }
    while (end > start &&
           (line[end - 1] == '\r' || line[end - 1] == '\n' ||
            line[end - 1] == ' ' || line[end - 1] == '\t')) {
        --end;
    }
    if (end == start || end - start >= capacity) { return 0; }
    memcpy(output, line + start, end - start);
    output[end - start] = '\0';
    return 1;
}

static size_t receive_header(char *data, size_t size, size_t count, void *opaque)
{
    DownloadTransfer *transfer = opaque;
    size_t bytes, colon;
    long status;
    if (size != 0 && count > SIZE_MAX / size) { return 0; }
    bytes = size * count;
    if (parse_status_line(data, bytes, &status)) {
        reset_response_headers(transfer, status);
        return bytes;
    }
    colon = 0;
    while (colon < bytes && data[colon] != ':') { ++colon; }
    if (colon == bytes) { return bytes; }
    if (colon == strlen("Location") &&
        ascii_prefix_nocase(data, "Location", colon)) {
        ++transfer->location_count;
        if (transfer->location_count > 1 ||
            !copy_header_value(transfer->location, sizeof(transfer->location),
                               data + colon + 1, bytes - colon - 1)) {
            transfer->location_invalid = 1;
        }
    } else if (colon == strlen("Content-Type") &&
               ascii_prefix_nocase(data, "Content-Type", colon)) {
        ++transfer->content_type_count;
        if (transfer->content_type_count > 1 ||
            !copy_header_value(transfer->content_type,
                               sizeof(transfer->content_type),
                               data + colon + 1, bytes - colon - 1)) {
            transfer->content_type_invalid = 1;
        }
    }
    return bytes;
}

static size_t write_file_chunk(void *data, size_t size, size_t count, void *opaque)
{
    DownloadTransfer *transfer = opaque;
    size_t bytes, written;
    long status = transfer->response_code;
    if (size != 0 && count > SIZE_MAX / size) {
        transfer->size_overflow = 1;
        return 0;
    }
    bytes = size * count;
    if (status < 200 || status >= 300 || status == 206) {
        return bytes;
    }
    if (!content_type_is_valid(transfer)) {
        transfer->content_type_invalid = 1;
        return 0;
    }
    if (bytes > DOWNLOAD_MAX_BYTES - transfer->bytes_written) {
        transfer->too_large = 1;
        return 0;
    }
    if (bytes == 0) { return 0; }
    written = fwrite(data, 1, bytes, transfer->file);
    if (written > SIZE_MAX - transfer->bytes_written) {
        transfer->size_overflow = 1;
        return 0;
    }
    transfer->bytes_written += written;
    if (written != bytes || ferror(transfer->file)) {
        transfer->write_failed = 1;
        return 0;
    }
    return bytes;
}

static DownloadStatus curl_failure(CURLcode code,
                                   const DownloadTransfer *transfer)
{
    if (transfer->size_overflow) { return DOWNLOAD_ERROR_OVERFLOW; }
    if (transfer->too_large || code == CURLE_FILESIZE_EXCEEDED) {
        return DOWNLOAD_ERROR_TOO_LARGE;
    }
    if (transfer->write_failed) { return DOWNLOAD_ERROR_WRITE; }
    if (transfer->content_type_invalid) { return DOWNLOAD_ERROR_CONTENT_TYPE; }
    if (code == CURLE_OUT_OF_MEMORY) { return DOWNLOAD_ERROR_OUT_OF_MEMORY; }
    if (code == CURLE_OPERATION_TIMEDOUT) { return DOWNLOAD_ERROR_TIMEOUT; }
    if (code == CURLE_PARTIAL_FILE) { return DOWNLOAD_ERROR_INCOMPLETE; }
    return DOWNLOAD_ERROR_NETWORK;
}

static DownloadStatus perform_hop(const char *url, long timeout_seconds,
                                  DownloadTransfer *transfer,
                                  DownloadResult *result)
{
    CURL *curl;
    CURLcode code;
    CURLcode info;
    char error_buffer[CURL_ERROR_SIZE] = {0};
    char *effective_url = NULL;
    long status_code = 0;

    curl = curl_easy_init();
    if (curl == NULL) {
        set_result(result, DOWNLOAD_ERROR_OUT_OF_MEMORY,
                   "libcurl could not allocate a request handle");
        return DOWNLOAD_ERROR_OUT_OF_MEMORY;
    }

#define SET_DOWNLOAD_OPTION(option, value) do { \
    code = curl_easy_setopt(curl, option, value); \
    if (code != CURLE_OK) { \
        DownloadStatus option_status = code == CURLE_OUT_OF_MEMORY \
            ? DOWNLOAD_ERROR_OUT_OF_MEMORY : DOWNLOAD_ERROR_INIT; \
        (void)snprintf(result->detail, sizeof(result->detail), \
                       "Cannot configure %s: %.150s", #option, \
                       curl_easy_strerror(code)); \
        result->status = option_status; \
        curl_easy_cleanup(curl); \
        return option_status; \
    } \
} while (0)

#if LIBCURL_VERSION_NUM >= 0x075500
    SET_DOWNLOAD_OPTION(CURLOPT_PROTOCOLS_STR, "http,https");
    SET_DOWNLOAD_OPTION(CURLOPT_REDIR_PROTOCOLS_STR, "http,https");
#else
    SET_DOWNLOAD_OPTION(CURLOPT_PROTOCOLS,
                        (long)(CURLPROTO_HTTP | CURLPROTO_HTTPS));
    SET_DOWNLOAD_OPTION(CURLOPT_REDIR_PROTOCOLS,
                        (long)(CURLPROTO_HTTP | CURLPROTO_HTTPS));
#endif
    SET_DOWNLOAD_OPTION(CURLOPT_URL, url);
    SET_DOWNLOAD_OPTION(CURLOPT_FOLLOWLOCATION, 0L);
    SET_DOWNLOAD_OPTION(CURLOPT_MAXREDIRS, 0L);
    SET_DOWNLOAD_OPTION(CURLOPT_TIMEOUT, timeout_seconds);
    SET_DOWNLOAD_OPTION(CURLOPT_CONNECTTIMEOUT, timeout_seconds);
    SET_DOWNLOAD_OPTION(CURLOPT_SSL_VERIFYPEER, 1L);
    SET_DOWNLOAD_OPTION(CURLOPT_SSL_VERIFYHOST, 2L);
    SET_DOWNLOAD_OPTION(CURLOPT_NOSIGNAL, 1L);
    SET_DOWNLOAD_OPTION(CURLOPT_MAXFILESIZE_LARGE, (curl_off_t)DOWNLOAD_MAX_BYTES);
    SET_DOWNLOAD_OPTION(CURLOPT_USERAGENT,
                        "WebCrawler/1.0 (+https://github.com/jatinvimal1506/Web-Crawler-and-Scraper-in-C)");
    SET_DOWNLOAD_OPTION(CURLOPT_ERRORBUFFER, error_buffer);
    SET_DOWNLOAD_OPTION(CURLOPT_HEADERFUNCTION, receive_header);
    SET_DOWNLOAD_OPTION(CURLOPT_HEADERDATA, transfer);
    SET_DOWNLOAD_OPTION(CURLOPT_WRITEFUNCTION, write_file_chunk);
    SET_DOWNLOAD_OPTION(CURLOPT_WRITEDATA, transfer);

    code = curl_easy_perform(curl);
    info = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status_code);
    if (info != CURLE_OK) {
        curl_easy_cleanup(curl);
        set_result(result, DOWNLOAD_ERROR_INIT,
                   "libcurl could not read the HTTP response status");
        return DOWNLOAD_ERROR_INIT;
    }
    result->http_status = status_code;
    result->bytes_written = transfer->bytes_written;
    info = curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &effective_url);
    if (info != CURLE_OK || effective_url == NULL) {
        curl_easy_cleanup(curl);
        set_result(result, DOWNLOAD_ERROR_NETWORK,
                   "libcurl did not provide the effective response URL");
        return DOWNLOAD_ERROR_NETWORK;
    }
    {
        size_t length = strlen(effective_url);
        char *copy;
        if (length == SIZE_MAX) {
            curl_easy_cleanup(curl);
            set_result(result, DOWNLOAD_ERROR_OVERFLOW,
                       "effective URL length overflow");
            return DOWNLOAD_ERROR_OVERFLOW;
        }
        copy = malloc(length + 1);
        if (copy == NULL) {
            curl_easy_cleanup(curl);
            set_result(result, DOWNLOAD_ERROR_OUT_OF_MEMORY,
                       "out of memory copying effective URL");
            return DOWNLOAD_ERROR_OUT_OF_MEMORY;
        }
        memcpy(copy, effective_url, length + 1);
        free(result->final_url);
        result->final_url = copy;
    }
    curl_easy_cleanup(curl);
    if (code != CURLE_OK) {
        DownloadStatus failure = curl_failure(code, transfer);
        const char *diagnostic = error_buffer[0] != '\0'
            ? error_buffer : curl_easy_strerror(code);
        (void)snprintf(result->detail, sizeof(result->detail),
                       "%.200s: %.45s", download_status_message(failure),
                       diagnostic);
        result->status = failure;
        return failure;
    }
    return DOWNLOAD_OK;
#undef SET_DOWNLOAD_OPTION
}

static int basename_is_safe(const char *name, DocumentType type)
{
    const char *extension = expected_mime(type);
    const char *dot, *p;
    size_t length, stem_length, extension_length;
    char stem[DOWNLOAD_MAX_FILENAME_BYTES + 1];
    if (name == NULL || extension == NULL) { return 0; }
    length = strlen(name);
    if (length == 0 || length > DOWNLOAD_MAX_FILENAME_BYTES ||
        name[length - 1] == '.' || name[length - 1] == ' ' ||
        strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
        return 0;
    }
    for (p = name; *p != '\0'; ++p) {
        unsigned char ch = (unsigned char)*p;
        if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
              (ch >= '0' && ch <= '9') || ch == '.' || ch == '-' || ch == '_')) {
            return 0;
        }
    }
    dot = strrchr(name, '.');
    if (dot == NULL || dot == name) { return 0; }
    extension_length = type == DOCUMENT_TYPE_PDF ? 4U
        : type == DOCUMENT_TYPE_DOCX ? 5U : 4U;
    if (strlen(dot) != extension_length ||
        !ascii_equal_nocase(dot, type == DOCUMENT_TYPE_PDF ? ".pdf"
                           : type == DOCUMENT_TYPE_DOCX ? ".docx" : ".zip")) {
        return 0;
    }

    stem_length = (size_t)(dot - name);
    if (stem_length > sizeof(stem) - 1) { return 0; }
    memcpy(stem, name, stem_length);
    stem[stem_length] = '\0';
    if (ascii_equal_nocase(stem, "CON") || ascii_equal_nocase(stem, "PRN") ||
        ascii_equal_nocase(stem, "AUX") || ascii_equal_nocase(stem, "NUL")) {
        return 0;
    }
    if (stem_length == 4 &&
        ((ascii_prefix_nocase(stem, "COM", 3) ||
          ascii_prefix_nocase(stem, "LPT", 3)) &&
         stem[3] >= '1' && stem[3] <= '9')) {
        return 0;
    }
    return 1;
}

static int directory_is_existing(const char *path)
{
    DownloadStat info;
    return DOWNLOAD_STAT(path, &info) == 0 && DOWNLOAD_IS_DIRECTORY(info.st_mode);
}

static DownloadStatus make_output_path(const char *directory,
                                       const char *filename, char **output)
{
    size_t directory_length = strlen(directory);
    size_t filename_length = strlen(filename);
    int has_separator = directory_length != 0 &&
        (directory[directory_length - 1] == DD_PATH_SEPARATOR
#ifdef _WIN32
         || directory[directory_length - 1] == '/'
#endif
        );
    size_t separator_length = has_separator ? 0U : 1U;
    size_t total;
    char *path;
    if (directory_length > SIZE_MAX - filename_length - separator_length - 1) {
        return DOWNLOAD_ERROR_OVERFLOW;
    }
    total = directory_length + separator_length + filename_length + 1;
    path = malloc(total);
    if (path == NULL) { return DOWNLOAD_ERROR_OUT_OF_MEMORY; }
    memcpy(path, directory, directory_length);
    if (separator_length != 0) { path[directory_length] = DD_PATH_SEPARATOR; }
    memcpy(path + directory_length + separator_length, filename,
           filename_length + 1);
    *output = path;
    return DOWNLOAD_OK;
}

static DownloadStatus create_exclusive_file(const char *path, FILE **output,
                                            int *created)
{
    int descriptor;
    *output = NULL;
    *created = 0;
#ifdef _WIN32
    descriptor = DD_OPEN(path, _O_WRONLY | _O_CREAT | _O_EXCL | DD_BINARY_FLAG,
                         _S_IREAD | _S_IWRITE);
#else
    descriptor = DD_OPEN(path, O_WRONLY | O_CREAT | O_EXCL | DD_BINARY_FLAG, 0600);
#endif
    if (descriptor < 0) {
        return errno == EEXIST ? DOWNLOAD_ERROR_EXISTS : DOWNLOAD_ERROR_FILE;
    }
    *created = 1;
    {
        FILE *file = DD_FDOPEN(descriptor, "wb");
        if (file == NULL) {
            int saved_errno = errno;
            if (DD_CLOSE(descriptor) != 0) {
                saved_errno = errno;
            }
            errno = saved_errno;
            return DOWNLOAD_ERROR_FILE;
        }
        *output = file;
        return DOWNLOAD_OK;
    }
}

static DownloadStatus finish_file(FILE **file, int created, char **path,
                                  DownloadResult *result,
                                  DownloadStatus status)
{
    int close_failed = 0;
    if (*file != NULL) {
        if (fclose(*file) != 0) { close_failed = 1; }
        *file = NULL;
    }
    if (close_failed) {
        status = DOWNLOAD_ERROR_FILE;
        set_result(result, status,
                   "fclose reported an output-file error; transfer cannot be accepted");
    }
    if (status == DOWNLOAD_OK) {
        result->file_path = *path;
        *path = NULL;
        set_result(result, DOWNLOAD_OK, NULL);
        return DOWNLOAD_OK;
    }
    if (created && *path != NULL) {
        if (remove(*path) != 0) {
            result->file_path = *path;
            *path = NULL;
            status = DOWNLOAD_ERROR_CLEANUP;
            set_result(result, status, "failed to remove the incomplete destination file");
        }
    }
    if (result->status != status) {
        set_result(result, status, NULL);
    }
    return status;
}

DownloadStatus download_document(const DownloadConfig *config,
                                 DownloadResult *result)
{
    ParsedUrl seed = {0}, target = {0};
    char *urls[DOWNLOAD_MAX_REDIRECTS + 1] = {0};
    size_t url_count = 0, i;
    char *path = NULL;
    FILE *file = NULL;
    int created = 0, previously_https = 0;
    unsigned redirects = 0;
    DocumentType type = DOCUMENT_TYPE_UNKNOWN;
    DocumentStatus document_status;
    UrlStatus url_status;
    DownloadStatus status = DOWNLOAD_ERROR_INVALID_ARGUMENT;
    HttpStatus curl_status;

    if (result == NULL) { return DOWNLOAD_ERROR_INVALID_ARGUMENT; }
    if (!result_is_empty(result)) {
        return DOWNLOAD_ERROR_INVALID_ARGUMENT;
    }
    if (config == NULL || config->document_url == NULL ||
        config->document_url[0] == '\0' || config->seed_url == NULL ||
        config->seed_url[0] == '\0' || config->output_directory == NULL ||
        config->output_directory[0] == '\0' || config->filename == NULL ||
        config->timeout_seconds < 1 ||
        config->timeout_seconds > DOWNLOAD_MAX_TIMEOUT_SECONDS ||
        (config->same_domain != 0 && config->same_domain != 1)) {
        set_result(result, DOWNLOAD_ERROR_INVALID_ARGUMENT, NULL);
        return DOWNLOAD_ERROR_INVALID_ARGUMENT;
    }
    document_status = document_type_from_url(config->document_url, &type);
    if (document_status == DOCUMENT_ERROR_OUT_OF_MEMORY) {
        set_result(result, DOWNLOAD_ERROR_OUT_OF_MEMORY, NULL);
        return DOWNLOAD_ERROR_OUT_OF_MEMORY;
    }
    if (document_status != DOCUMENT_OK) {
        set_result(result, DOWNLOAD_ERROR_INVALID_ARGUMENT,
                   "document URL must be a valid absolute HTTP(S) URL");
        return DOWNLOAD_ERROR_INVALID_ARGUMENT;
    }
    if (type == DOCUMENT_TYPE_UNKNOWN) {
        set_result(result, DOWNLOAD_ERROR_UNSUPPORTED_TYPE, NULL);
        return DOWNLOAD_ERROR_UNSUPPORTED_TYPE;
    }
    result->type = type;
    if (!basename_is_safe(config->filename, type)) {
        set_result(result, DOWNLOAD_ERROR_INVALID_ARGUMENT,
                   "filename must be a safe basename with the matching extension");
        return DOWNLOAD_ERROR_INVALID_ARGUMENT;
    }
    if (!directory_is_existing(config->output_directory)) {
        set_result(result, DOWNLOAD_ERROR_INVALID_ARGUMENT,
                   "output directory must already exist");
        return DOWNLOAD_ERROR_INVALID_ARGUMENT;
    }
    url_status = url_parse(config->seed_url, &seed);
    if (url_status != URL_OK) {
        status = url_status == URL_ERROR_OUT_OF_MEMORY
            ? DOWNLOAD_ERROR_OUT_OF_MEMORY : DOWNLOAD_ERROR_INVALID_ARGUMENT;
        set_result(result, status, "seed URL is not a valid HTTP(S) URL");
        goto done;
    }
    url_status = url_normalize(config->document_url, &urls[0]);
    if (url_status != URL_OK) {
        status = url_status == URL_ERROR_OUT_OF_MEMORY
            ? DOWNLOAD_ERROR_OUT_OF_MEMORY : DOWNLOAD_ERROR_INVALID_ARGUMENT;
        set_result(result, status, "document URL is not a valid HTTP(S) URL");
        goto done;
    }
    url_count = 1;
    url_status = url_parse(urls[0], &target);
    if (url_status != URL_OK) {
        status = url_status == URL_ERROR_OUT_OF_MEMORY
            ? DOWNLOAD_ERROR_OUT_OF_MEMORY : DOWNLOAD_ERROR_INVALID_ARGUMENT;
        set_result(result, status, "document URL could not be parsed");
        goto done;
    }
    if (!http_scope_allows(&seed, &target, config->same_domain, 0)) {
        set_result(result, DOWNLOAD_ERROR_SCOPE,
                   "document URL is outside the seed host/port scope");
        status = DOWNLOAD_ERROR_SCOPE;
        goto done;
    }
    previously_https = strcmp(target.scheme, "https") == 0;
    url_free(&target);

    status = make_output_path(config->output_directory, config->filename, &path);
    if (status != DOWNLOAD_OK) {
        set_result(result, status, NULL);
        goto done;
    }
    curl_status = http_prepare_curl();
    if (curl_status != HTTP_OK) {
        status = curl_status == HTTP_ERR_OUT_OF_MEMORY
            ? DOWNLOAD_ERROR_OUT_OF_MEMORY : DOWNLOAD_ERROR_INIT;
        set_result(result, status, NULL);
        goto done;
    }
    errno = 0;
    status = create_exclusive_file(path, &file, &created);
    if (status != DOWNLOAD_OK) {
        set_result(result, status,
                   status == DOWNLOAD_ERROR_EXISTS ? NULL
                   : "could not exclusively create destination file");
        goto done;
    }

    for (;;) {
        DownloadTransfer transfer = {0};
        long response_code;
        transfer.file = file;
        transfer.expected_type = type;
        status = perform_hop(urls[url_count - 1], config->timeout_seconds,
                             &transfer, result);
        result->bytes_written = transfer.bytes_written;
        if (status != DOWNLOAD_OK) { goto done; }
        response_code = result->http_status;
        if (!redirect_status(response_code)) {
            if (response_code == 206) {
                status = DOWNLOAD_ERROR_HTTP_STATUS;
                set_result(result, status, "partial-content responses are not accepted");
                goto done;
            }
            if (response_code < 200 || response_code >= 300) {
                status = DOWNLOAD_ERROR_HTTP_STATUS;
                (void)snprintf(result->detail, sizeof(result->detail),
                               "server returned HTTP status %ld", response_code);
                result->status = status;
                goto done;
            }
            if (transfer.content_type_count != 1 ||
                transfer.content_type_invalid ||
                !content_type_is_valid(&transfer)) {
                status = DOWNLOAD_ERROR_CONTENT_TYPE;
                set_result(result, status, NULL);
                goto done;
            }
            if (transfer.bytes_written == 0) {
                status = DOWNLOAD_ERROR_EMPTY;
                set_result(result, status, NULL);
                goto done;
            }
            status = DOWNLOAD_OK;
            goto done;
        }
        if (transfer.location_invalid || transfer.location_count != 1 ||
            transfer.location[0] == '\0') {
            status = DOWNLOAD_ERROR_REDIRECT;
            set_result(result, status, "redirect has a missing or ambiguous Location");
            goto done;
        }
        if (redirects >= DOWNLOAD_MAX_REDIRECTS ||
            url_count >= DOWNLOAD_MAX_REDIRECTS + 1) {
            status = DOWNLOAD_ERROR_REDIRECT;
            set_result(result, status, "redirect limit of five hops exceeded");
            goto done;
        }
        if (result->final_url == NULL) {
            status = DOWNLOAD_ERROR_NETWORK;
            set_result(result, status, "redirect response has no effective URL");
            goto done;
        }
        url_status = url_resolve(result->final_url, transfer.location,
                                 &urls[url_count]);
        if (url_status != URL_OK) {
            status = url_status == URL_ERROR_OUT_OF_MEMORY
                ? DOWNLOAD_ERROR_OUT_OF_MEMORY : DOWNLOAD_ERROR_REDIRECT;
            set_result(result, status, "redirect Location is not a valid HTTP(S) URL");
            goto done;
        }
        for (i = 0; i < url_count; ++i) {
            if (strcmp(urls[i], urls[url_count]) == 0) {
                status = DOWNLOAD_ERROR_REDIRECT;
                set_result(result, status, "redirect loop detected");
                goto done;
            }
        }
        url_status = url_parse(urls[url_count], &target);
        if (url_status != URL_OK) {
            status = url_status == URL_ERROR_OUT_OF_MEMORY
                ? DOWNLOAD_ERROR_OUT_OF_MEMORY : DOWNLOAD_ERROR_REDIRECT;
            set_result(result, status, "redirect target could not be parsed");
            goto done;
        }
        if (!http_scope_allows(&seed, &target, config->same_domain,
                               previously_https)) {
            status = DOWNLOAD_ERROR_SCOPE;
            set_result(result, status,
                       "redirect target refused before request by host/port/TLS policy");
            goto done;
        }
        previously_https = strcmp(target.scheme, "https") == 0;
        url_free(&target);
        ++url_count;
        ++redirects;
    }

done:
    url_free(&target);
    url_free(&seed);
    for (i = 0; i < url_count; ++i) { free(urls[i]); }
    status = finish_file(&file, created, &path, result, status);
    free(path);
    return status;
}
