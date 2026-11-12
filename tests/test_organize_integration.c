#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#include "organize.h"
#include "sha256.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define INPUT_LIMIT (ORGANIZE_MAX_FILE_BYTES + 1U)

static unsigned checks;
static unsigned failures;
static char test_root[256];

static const unsigned char bytes_a[] = {0x00U, 0x01U, 0x80U, 0xffU, 'A', 0x00U};
static const unsigned char bytes_b[] = {0x00U, 0x01U, 0x81U, 0xffU, 'A', 0x00U};
static const unsigned char bytes_docx[] = {'D', 0x00U, 0x91U, 0x02U, 'X', 0xffU};

#define CHECK(expression) do { \
    ++checks; \
    if (!(expression)) { \
        ++failures; \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expression); \
    } \
} while (0)

static int make_path(char *output, size_t capacity, const char *base,
                     const char *name)
{
    int written = snprintf(output, capacity, "%s/%s", base, name);
    return written > 0 && (size_t)written < capacity;
}

static int write_file(const char *path, const unsigned char *bytes,
                      size_t length)
{
    FILE *file = fopen(path, "wb");
    int ok;
    if (file == NULL) { return 0; }
    ok = length == 0 || fwrite(bytes, 1, length, file) == length;
    if (fclose(file) != 0) { ok = 0; }
    return ok;
}

static int write_repeated(const char *path, unsigned char value, size_t length)
{
    unsigned char buffer[16384];
    FILE *file = fopen(path, "wb");
    size_t remaining = length;
    int ok = 1;
    memset(buffer, value, sizeof(buffer));
    if (file == NULL) { return 0; }
    while (remaining != 0) {
        size_t amount = remaining < sizeof(buffer) ? remaining : sizeof(buffer);
        if (fwrite(buffer, 1, amount, file) != amount) {
            ok = 0;
            break;
        }
        remaining -= amount;
    }
    if (fclose(file) != 0) { ok = 0; }
    return ok;
}

static int contents_equal(const char *path, const unsigned char *expected,
                          size_t expected_size)
{
    unsigned char buffer[64];
    FILE *file = fopen(path, "rb");
    size_t count;
    int ok;
    if (file == NULL) { return 0; }
    count = fread(buffer, 1, sizeof(buffer), file);
    ok = count == expected_size &&
         memcmp(buffer, expected, expected_size) == 0 &&
         fgetc(file) == EOF;
    if (fclose(file) != 0) { ok = 0; }
    return ok;
}

static int is_directory(const char *path)
{
    struct stat info;
    return lstat(path, &info) == 0 && S_ISDIR(info.st_mode);
}

static void test_directory_collisions(void)
{
    char bad_root[512], pdf_path[768], target[512], link_root[512];
    char link_path[768];
    FILE *file;

    CHECK(make_path(bad_root, sizeof(bad_root), test_root, "file-collision"));
    CHECK(mkdir(bad_root, 0700) == 0);
    CHECK(make_path(pdf_path, sizeof(pdf_path), bad_root, "pdf"));
    file = fopen(pdf_path, "wb");
    CHECK(file != NULL);
    if (file != NULL) { CHECK(fclose(file) == 0); }
    CHECK(organizer_prepare_directories(bad_root) ==
          ORGANIZER_ERROR_FILESYSTEM);
    CHECK(access(pdf_path, F_OK) == 0);
    CHECK(remove(pdf_path) == 0);
    CHECK(rmdir(bad_root) == 0);

    CHECK(make_path(target, sizeof(target), test_root, "symlink-target"));
    CHECK(mkdir(target, 0700) == 0);
    CHECK(make_path(link_root, sizeof(link_root), test_root, "symlink-root"));
    CHECK(mkdir(link_root, 0700) == 0);
    CHECK(make_path(link_path, sizeof(link_path), link_root, "pdf"));
    if (symlink(target, link_path) == 0) {
        CHECK(organizer_prepare_directories(link_root) ==
              ORGANIZER_ERROR_FILESYSTEM);
        CHECK(is_directory(target));
        CHECK(unlink(link_path) == 0);
    } else {
        printf("SKIP: filesystem does not permit the symlink test\n");
    }
    CHECK(rmdir(link_root) == 0);
    CHECK(rmdir(target) == 0);
}

int main(void)
{
    char template_path[] = "build/phase10-organize-XXXXXX";
    char input_a[512], input_b[512], input_c[512], input_d[512], empty_path[512];
    char oversized_path[512], digest_a[65], digest_c[65], digest_d[65];
    char canonical_pdf[768], canonical_docx[768], canonical_zip[768];
    char path[768], input_link[768];
    OrganizeResult first = {0}, duplicate = {0}, different = {0};
    OrganizeResult docx = {0}, error = {0};
    size_t input_size = 0;

    if (mkdtemp(template_path) == NULL) {
        fprintf(stderr, "could not create local test directory\n");
        return 1;
    }
    (void)snprintf(test_root, sizeof(test_root), "%s", template_path);
    CHECK(make_path(input_a, sizeof(input_a), test_root, "source-a.bin"));
    CHECK(make_path(input_b, sizeof(input_b), test_root, "source-b.bin"));
    CHECK(make_path(input_c, sizeof(input_c), test_root, "source-c.bin"));
    CHECK(make_path(input_d, sizeof(input_d), test_root, "source-d.bin"));
    CHECK(make_path(empty_path, sizeof(empty_path), test_root, "empty.bin"));
    CHECK(make_path(oversized_path, sizeof(oversized_path), test_root,
                    "oversized.bin"));
    CHECK(write_file(input_a, bytes_a, sizeof(bytes_a)));
    CHECK(write_file(input_b, bytes_a, sizeof(bytes_a)));
    CHECK(write_file(input_c, bytes_b, sizeof(bytes_b)));
    CHECK(write_file(input_d, bytes_docx, sizeof(bytes_docx)));
    CHECK(write_file(empty_path, NULL, 0));
    CHECK(sha256_file(input_a, digest_a, &input_size) == SHA256_OK);
    CHECK(input_size == sizeof(bytes_a));
    CHECK(sha256_file(input_c, digest_c, NULL) == SHA256_OK);
    CHECK(sha256_file(input_d, digest_d, NULL) == SHA256_OK);
    CHECK(write_repeated(oversized_path, 0x5aU, INPUT_LIMIT));

    CHECK(organizer_prepare_directories(test_root) == ORGANIZER_OK);
    CHECK(organizer_prepare_directories(test_root) == ORGANIZER_OK);
    {
        char folder[512];
        CHECK(make_path(folder, sizeof(folder), test_root, "pdf"));
        CHECK(is_directory(folder));
        CHECK(make_path(folder, sizeof(folder), test_root, "docx"));
        CHECK(is_directory(folder));
        CHECK(make_path(folder, sizeof(folder), test_root, "zip"));
        CHECK(is_directory(folder));
    }


    CHECK(make_path(canonical_pdf, sizeof(canonical_pdf), test_root, "pdf"));
    {
        int written = snprintf(path, sizeof(path), "%s/%s.pdf",
                               canonical_pdf, digest_a);
        CHECK(written > 0 && (size_t)written < sizeof(path));
    }
    CHECK(write_file(path, bytes_b, sizeof(bytes_b)));
    CHECK(organize_file(input_a, DOCUMENT_TYPE_PDF, test_root, &error) ==
          ORGANIZER_ERROR_CONFLICT);
    CHECK(error.status == ORGANIZER_ERROR_CONFLICT);
    CHECK(error.stored_path == NULL);
    CHECK(contents_equal(path, bytes_b, sizeof(bytes_b)));
    CHECK(contents_equal(input_a, bytes_a, sizeof(bytes_a)));
    CHECK(remove(path) == 0);
    organize_result_free(&error);

    CHECK(organize_file(input_a, DOCUMENT_TYPE_PDF, test_root, &first) ==
          ORGANIZER_NEW_FILE);
    CHECK(first.status == ORGANIZER_NEW_FILE && first.is_duplicate == 0);
    CHECK(first.stored_type == DOCUMENT_TYPE_PDF);
    CHECK(first.bytes == sizeof(bytes_a));
    CHECK(strcmp(first.sha256_hex, digest_a) == 0);
    CHECK(first.stored_path != NULL);
    CHECK(snprintf(canonical_pdf, sizeof(canonical_pdf), "%s/pdf/%s.pdf",
                   test_root, digest_a) > 0);
    CHECK(strcmp(first.stored_path, canonical_pdf) == 0);
    CHECK(contents_equal(first.stored_path, bytes_a, sizeof(bytes_a)));
    CHECK(contents_equal(input_a, bytes_a, sizeof(bytes_a)));


    CHECK(organize_file(input_b, DOCUMENT_TYPE_DOCX, test_root, &duplicate) ==
          ORGANIZER_DUPLICATE);
    CHECK(duplicate.is_duplicate == 1);
    CHECK(duplicate.stored_type == DOCUMENT_TYPE_PDF);
    CHECK(strcmp(duplicate.stored_path, first.stored_path) == 0);
    CHECK(contents_equal(input_b, bytes_a, sizeof(bytes_a)));


    CHECK(organize_file(input_c, DOCUMENT_TYPE_ZIP, test_root, &different) ==
          ORGANIZER_NEW_FILE);
    CHECK(different.is_duplicate == 0);
    CHECK(different.stored_type == DOCUMENT_TYPE_ZIP);
    CHECK(strcmp(different.sha256_hex, digest_c) == 0);
    CHECK(snprintf(canonical_zip, sizeof(canonical_zip), "%s/zip/%s.zip",
                   test_root, digest_c) > 0);
    CHECK(strcmp(different.stored_path, canonical_zip) == 0);
    CHECK(contents_equal(different.stored_path, bytes_b, sizeof(bytes_b)));
    CHECK(contents_equal(input_c, bytes_b, sizeof(bytes_b)));

    CHECK(organize_file(input_d, DOCUMENT_TYPE_DOCX, test_root, &docx) ==
          ORGANIZER_NEW_FILE);
    CHECK(docx.stored_type == DOCUMENT_TYPE_DOCX && !docx.is_duplicate);
    CHECK(strcmp(docx.sha256_hex, digest_d) == 0);
    CHECK(snprintf(canonical_docx, sizeof(canonical_docx), "%s/docx/%s.docx",
                   test_root, digest_d) > 0);
    CHECK(strcmp(docx.stored_path, canonical_docx) == 0);
    CHECK(contents_equal(docx.stored_path, bytes_docx, sizeof(bytes_docx)));
    CHECK(contents_equal(input_d, bytes_docx, sizeof(bytes_docx)));

    CHECK(organize_file(empty_path, DOCUMENT_TYPE_PDF, test_root, &error) ==
          ORGANIZER_ERROR_EMPTY);
    CHECK(error.stored_path == NULL);
    CHECK(access(empty_path, F_OK) == 0);
    organize_result_free(&error);
    CHECK(organize_file(oversized_path, DOCUMENT_TYPE_PDF, test_root, &error) ==
          ORGANIZER_ERROR_TOO_LARGE);
    CHECK(error.stored_path == NULL);
    {
        struct stat info;
        CHECK(stat(oversized_path, &info) == 0 &&
              (uintmax_t)info.st_size == (uintmax_t)INPUT_LIMIT);
    }
    organize_result_free(&error);
    CHECK(organize_file(input_a, DOCUMENT_TYPE_DOCX, "build/no-organize-root",
                        &error) == ORGANIZER_ERROR_FILESYSTEM);
    CHECK(contents_equal(input_a, bytes_a, sizeof(bytes_a)));
    organize_result_free(&error);

    CHECK(make_path(input_link, sizeof(input_link), test_root, "source-link"));
    if (symlink(input_a, input_link) == 0) {
        CHECK(organize_file(input_link, DOCUMENT_TYPE_PDF, test_root, &error) ==
              ORGANIZER_ERROR_FILESYSTEM);
        CHECK(error.stored_path == NULL);
        organize_result_free(&error);
        CHECK(unlink(input_link) == 0);
    } else {
        printf("SKIP: filesystem does not permit the input symlink test\n");
    }

    test_directory_collisions();

    organize_result_free(&first);
    organize_result_free(&duplicate);
    organize_result_free(&different);
    organize_result_free(&docx);
    organize_result_free(&first);
    CHECK(remove(canonical_pdf) == 0);
    CHECK(remove(canonical_zip) == 0);
    CHECK(remove(canonical_docx) == 0);
    CHECK(remove(input_a) == 0);
    CHECK(remove(input_b) == 0);
    CHECK(remove(input_c) == 0);
    CHECK(remove(input_d) == 0);
    CHECK(remove(empty_path) == 0);
    CHECK(remove(oversized_path) == 0);
    CHECK(make_path(path, sizeof(path), test_root, "pdf"));
    CHECK(rmdir(path) == 0);
    CHECK(make_path(path, sizeof(path), test_root, "docx"));
    CHECK(rmdir(path) == 0);
    CHECK(make_path(path, sizeof(path), test_root, "zip"));
    CHECK(rmdir(path) == 0);
    CHECK(rmdir(test_root) == 0);

    printf("organize integration: %u checks, %u failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
#else
#include <stdio.h>
int main(void)
{
    puts("SKIP: Phase 10 filesystem integration requires POSIX mkdtemp/symlink");
    return 0;
}
#endif
