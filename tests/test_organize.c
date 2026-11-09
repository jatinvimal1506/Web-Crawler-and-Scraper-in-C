#include "organize.h"
#include "sha256.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#define MAKE_DIR(path) _mkdir(path)
#define REMOVE_DIR(path) _rmdir(path)
#else
#include <unistd.h>
#define MAKE_DIR(path) mkdir((path), 0700)
#define REMOVE_DIR(path) rmdir(path)
#endif

#define TEST_ROOT "build/phase10-organize-unit"
#define INPUT_A "build/phase10-organize-input-a.bin"
#define INPUT_B "build/phase10-organize-input-b.bin"
#define INPUT_C "build/phase10-organize-input-c.bin"
#define INPUT_EMPTY "build/phase10-organize-empty.bin"

#ifdef _WIN32
#define PATH_SEPARATOR '\\'
#else
#define PATH_SEPARATOR '/'
#endif

static unsigned checks;
static unsigned failures;
static const unsigned char same_bytes[] = {0x00U, 0x01U, 0x80U, 0xffU, 'A', 0x00U};
static const unsigned char different_bytes[] = {0x00U, 0x01U, 0x81U, 0xffU, 'A', 0x00U};

#define CHECK(expression) do { \
    ++checks; \
    if (!(expression)) { \
        ++failures; \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expression); \
    } \
} while (0)

static int join(char *output, size_t capacity, const char *left,
                const char *right)
{
    int written = snprintf(output, capacity, "%s/%s", left, right);
    return written > 0 && (size_t)written < capacity;
}

static int expected_path_matches(const char *actual, const char *folder,
                                 const char *digest, const char *extension)
{
    char expected[512];
    int written = snprintf(expected, sizeof(expected), "%s%c%s%c%s%s",
                           TEST_ROOT, PATH_SEPARATOR, folder, PATH_SEPARATOR,
                           digest, extension);
    return written > 0 && (size_t)written < sizeof(expected) &&
           actual != NULL && strcmp(actual, expected) == 0;
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

static int files_equal(const char *left_path, const char *right_path)
{
    unsigned char left[32], right[32];
    FILE *left_file = fopen(left_path, "rb");
    FILE *right_file = fopen(right_path, "rb");
    int equal = 0;
    if (left_file == NULL || right_file == NULL) {
        if (left_file != NULL) { (void)fclose(left_file); }
        if (right_file != NULL) { (void)fclose(right_file); }
        return 0;
    }
    {
        size_t left_count = fread(left, 1, sizeof(left), left_file);
        size_t right_count = fread(right, 1, sizeof(right), right_file);
        equal = left_count == right_count &&
                memcmp(left, right, left_count) == 0 &&
                fgetc(left_file) == EOF && fgetc(right_file) == EOF;
    }
    if (fclose(left_file) != 0 || fclose(right_file) != 0) { equal = 0; }
    return equal;
}

static void remove_digest_files(const char *digest, const char *extension)
{
    static const char *const folders[] = {"pdf", "docx", "zip"};
    char name[80], directory[256], path[512];
    size_t index;
    int written = snprintf(name, sizeof(name), "%s%s", digest, extension);
    if (written < 0 || (size_t)written >= sizeof(name)) { return; }
    for (index = 0; index < 3U; ++index) {
        if (join(directory, sizeof(directory), TEST_ROOT, folders[index]) &&
            join(path, sizeof(path), directory, name)) {
            (void)remove(path);
        }
    }
}

static void clean_test_files(const char *digest_a, const char *digest_c)
{
    if (digest_a != NULL) {
        remove_digest_files(digest_a, ".pdf");
        remove_digest_files(digest_a, ".docx");
        remove_digest_files(digest_a, ".zip");
    }
    if (digest_c != NULL) {
        remove_digest_files(digest_c, ".pdf");
        remove_digest_files(digest_c, ".docx");
        remove_digest_files(digest_c, ".zip");
    }
    (void)remove(INPUT_A);
    (void)remove(INPUT_B);
    (void)remove(INPUT_C);
    (void)remove(INPUT_EMPTY);
    {
        char path[256];
        if (join(path, sizeof(path), TEST_ROOT, "pdf")) { (void)REMOVE_DIR(path); }
        if (join(path, sizeof(path), TEST_ROOT, "docx")) { (void)REMOVE_DIR(path); }
        if (join(path, sizeof(path), TEST_ROOT, "zip")) { (void)REMOVE_DIR(path); }
    }
    (void)REMOVE_DIR(TEST_ROOT);
}

int main(void)
{
    OrganizeResult pdf = {0}, duplicate = {0}, zip = {0}, invalid_type = {0};
    OrganizeResult error = {0};
    Sha256Status hash_status;
    char digest_a[65] = {0}, digest_c[65] = {0};
    size_t bytes = 0;

    (void)MAKE_DIR(TEST_ROOT);
    CHECK(organizer_prepare_directories(TEST_ROOT) == ORGANIZER_OK);
    CHECK(organizer_prepare_directories(TEST_ROOT) == ORGANIZER_OK);
    CHECK(organizer_prepare_directories(NULL) ==
          ORGANIZER_ERROR_INVALID_ARGUMENT);
    CHECK(organizer_prepare_directories("") ==
          ORGANIZER_ERROR_INVALID_ARGUMENT);
    CHECK(organizer_prepare_directories("build/no-phase10-root") ==
          ORGANIZER_ERROR_FILESYSTEM);
    CHECK(organize_result_init(&error) == ORGANIZER_OK);
    CHECK(organize_file(NULL, DOCUMENT_TYPE_PDF, TEST_ROOT, &error) ==
          ORGANIZER_ERROR_INVALID_ARGUMENT);
    CHECK(error.status == ORGANIZER_ERROR_INVALID_ARGUMENT);
    organize_result_free(&error);
    CHECK(organize_file(INPUT_A, DOCUMENT_TYPE_PDF, NULL, &error) ==
          ORGANIZER_ERROR_INVALID_ARGUMENT);
    organize_result_free(&error);
    CHECK(organize_file("", DOCUMENT_TYPE_PDF, TEST_ROOT, &error) ==
          ORGANIZER_ERROR_INVALID_ARGUMENT);
    organize_result_free(&error);
    CHECK(organize_file(INPUT_A, DOCUMENT_TYPE_PDF, "", &error) ==
          ORGANIZER_ERROR_INVALID_ARGUMENT);
    organize_result_free(&error);
    CHECK(organize_file(INPUT_A, DOCUMENT_TYPE_PDF, TEST_ROOT, NULL) ==
          ORGANIZER_ERROR_INVALID_ARGUMENT);

    CHECK(write_file(INPUT_A, same_bytes, sizeof(same_bytes)));
    CHECK(write_file(INPUT_B, same_bytes, sizeof(same_bytes)));
    CHECK(write_file(INPUT_C, different_bytes, sizeof(different_bytes)));
    CHECK(write_file(INPUT_EMPTY, NULL, 0));
    hash_status = sha256_file(INPUT_A, digest_a, &bytes);
    CHECK(hash_status == SHA256_OK && bytes == sizeof(same_bytes));
    CHECK(sha256_file(INPUT_C, digest_c, &bytes) == SHA256_OK);
    clean_test_files(digest_a, digest_c);
    (void)MAKE_DIR(TEST_ROOT);
    CHECK(organizer_prepare_directories(TEST_ROOT) == ORGANIZER_OK);
    CHECK(write_file(INPUT_A, same_bytes, sizeof(same_bytes)));
    CHECK(write_file(INPUT_B, same_bytes, sizeof(same_bytes)));
    CHECK(write_file(INPUT_C, different_bytes, sizeof(different_bytes)));
    CHECK(write_file(INPUT_EMPTY, NULL, 0));

    CHECK(organize_result_init(&pdf) == ORGANIZER_OK);
    CHECK(organize_file(INPUT_A, DOCUMENT_TYPE_PDF, TEST_ROOT, &pdf) ==
          ORGANIZER_NEW_FILE);
    CHECK(pdf.status == ORGANIZER_NEW_FILE);
    CHECK(pdf.is_duplicate == 0);
    CHECK(pdf.stored_type == DOCUMENT_TYPE_PDF);
    CHECK(pdf.bytes == sizeof(same_bytes));
    CHECK(strcmp(pdf.sha256_hex, digest_a) == 0);
    CHECK(pdf.stored_path != NULL);
    if (pdf.stored_path != NULL) {
        CHECK(expected_path_matches(pdf.stored_path, "pdf", digest_a, ".pdf"));
        CHECK(files_equal(INPUT_A, pdf.stored_path));
    }
    CHECK(files_equal(INPUT_A, INPUT_B));

    CHECK(organize_file(INPUT_B, DOCUMENT_TYPE_DOCX, TEST_ROOT, &duplicate) ==
          ORGANIZER_DUPLICATE);
    CHECK(duplicate.is_duplicate == 1);
    CHECK(duplicate.stored_type == DOCUMENT_TYPE_PDF);
    CHECK(duplicate.bytes == sizeof(same_bytes));
    CHECK(strcmp(duplicate.sha256_hex, digest_a) == 0);
    CHECK(pdf.stored_path != NULL && duplicate.stored_path != NULL &&
          strcmp(pdf.stored_path, duplicate.stored_path) == 0);

    CHECK(organize_file(INPUT_C, DOCUMENT_TYPE_ZIP, TEST_ROOT, &zip) ==
          ORGANIZER_NEW_FILE);
    CHECK(zip.is_duplicate == 0 && zip.stored_type == DOCUMENT_TYPE_ZIP);
    CHECK(zip.bytes == sizeof(different_bytes));
    CHECK(strcmp(zip.sha256_hex, digest_c) == 0);
    CHECK(expected_path_matches(zip.stored_path, "zip", digest_c, ".zip"));
    CHECK(files_equal(INPUT_C, zip.stored_path));

    CHECK(organize_file(INPUT_A, DOCUMENT_TYPE_UNKNOWN, TEST_ROOT, &invalid_type) ==
          ORGANIZER_ERROR_INVALID_TYPE);
    /* A live result cannot be reused or overwritten accidentally. */
    CHECK(organize_file(INPUT_A, DOCUMENT_TYPE_PDF, TEST_ROOT, &pdf) ==
          ORGANIZER_ERROR_INVALID_ARGUMENT);
    CHECK(pdf.stored_path != NULL && pdf.status == ORGANIZER_NEW_FILE);

    organize_result_free(&pdf);
    organize_result_free(&pdf);
    organize_result_free(&duplicate);
    organize_result_free(&zip);
    organize_result_free(&invalid_type);
    CHECK(organize_file(INPUT_EMPTY, DOCUMENT_TYPE_PDF, TEST_ROOT, &pdf) ==
          ORGANIZER_ERROR_EMPTY);
    CHECK(organize_file("build/missing-phase10-input", DOCUMENT_TYPE_PDF,
                        TEST_ROOT, &zip) == ORGANIZER_ERROR_FILESYSTEM);
    CHECK(organizer_status_message(ORGANIZER_NEW_FILE) != NULL);
    organize_result_free(&pdf);
    organize_result_free(&zip);

    clean_test_files(digest_a, digest_c);
    printf("organize unit: %u checks, %u failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
