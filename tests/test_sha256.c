#include "sha256.h"

#include <stdio.h>
#include <string.h>

#define TEST_PATH "build/phase10-sha256-test.bin"

static unsigned checks;
static unsigned failures;

#define CHECK(expression) do { \
    ++checks; \
    if (!(expression)) { \
        ++failures; \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expression); \
    } \
} while (0)

static int write_bytes(const unsigned char *bytes, size_t length)
{
    FILE *file = fopen(TEST_PATH, "wb");
    int ok;
    if (file == NULL) { return 0; }
    ok = length == 0 || fwrite(bytes, 1, length, file) == length;
    if (fclose(file) != 0) { ok = 0; }
    return ok;
}

static int write_repeated(unsigned char value, size_t length)
{
    unsigned char buffer[16384];
    FILE *file = fopen(TEST_PATH, "wb");
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

static void expect_hash(const char *expected, size_t expected_bytes)
{
    char hex[65];
    size_t bytes = 0;
    Sha256Status status = sha256_file(TEST_PATH, hex, &bytes);
    CHECK(status == SHA256_OK);
    CHECK(strcmp(hex, expected) == 0);
    CHECK(bytes == expected_bytes);
}

int main(void)
{
    static const char empty_hash[] =
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";
    static const char abc_hash[] =
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
    static const char million_a_hash[] =
        "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0";
    static const char binary_hash[] =
        "1c768c53c8a60bfd3ea06be77b311ac36e9c0d53c07f1bff7c4cabc69a4541bd";
    static const char maximum_zeros_hash[] =
        "cd52d81e25f372e6fa4db2c0dfceb59862c1969cab17096da352b34950c973cc";
    static const unsigned char binary[] = {0x00U, 0x01U, 0x80U, 0xffU, 'A', 0x00U};
    char hex[65];
    size_t bytes;

    CHECK(write_bytes(NULL, 0));
    expect_hash(empty_hash, 0);

    CHECK(write_bytes((const unsigned char *)"abc", 3));
    expect_hash(abc_hash, 3);

    CHECK(write_repeated((unsigned char)'a', 1000000U));
    expect_hash(million_a_hash, 1000000U);

    CHECK(write_bytes(binary, sizeof(binary)));
    expect_hash(binary_hash, sizeof(binary));
    CHECK(sha256_file(TEST_PATH, hex, NULL) == SHA256_OK);
    CHECK(strcmp(hex, binary_hash) == 0);

    CHECK(write_repeated(0, SHA256_MAX_FILE_BYTES));
    expect_hash(maximum_zeros_hash, SHA256_MAX_FILE_BYTES);

    CHECK(write_repeated(0, SHA256_MAX_FILE_BYTES + 1U));
    memset(hex, 'X', sizeof(hex));
    bytes = 123U;
    CHECK(sha256_file(TEST_PATH, hex, &bytes) == SHA256_ERROR_SIZE_LIMIT);
    CHECK(hex[0] == '\0' && bytes == 0);

    memset(hex, 'X', sizeof(hex));
    bytes = 123U;
    CHECK(sha256_file(NULL, hex, &bytes) == SHA256_ERROR_INVALID_ARGUMENT);
    CHECK(hex[0] == '\0' && bytes == 0);
    CHECK(sha256_file(TEST_PATH, NULL, &bytes) == SHA256_ERROR_INVALID_ARGUMENT);
    CHECK(bytes == 0);
    CHECK(sha256_file("", hex, NULL) == SHA256_ERROR_INVALID_ARGUMENT);
    CHECK(sha256_file("build/phase10-file-that-does-not-exist", hex, &bytes) ==
          SHA256_ERROR_OPEN);
    CHECK(hex[0] == '\0' && bytes == 0);
    CHECK(sha256_status_message(SHA256_OK) != NULL);
    CHECK(remove(TEST_PATH) == 0);

    printf("sha256: %u checks, %u failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
