CC = gcc
AR = ar
# Append project flags: Replit/Nix supplies dependency includes in CFLAGS.
CFLAGS += -std=c11 -Wall -Wextra -Wpedantic -Iinclude
LDLIBS ?= -lcurl
BUILD := build
LIB := $(BUILD)/libcrawler.a
SOURCES := src/project.c src/url.c src/http.c src/html.c src/queue.c src/visited.c
OBJECTS := $(SOURCES:src/%.c=$(BUILD)/%.o)
# Retain native cmd.exe behavior on Windows; use POSIX commands on Linux.
ifeq ($(OS),Windows_NT)
SHELL = cmd.exe
.SHELLFLAGS = /C
EXE := .exe
RUN := .\$(BUILD)\$(empty)
else
EXE :=
RUN := ./$(BUILD)/
endif

DEMO := $(BUILD)/crawler-demo$(EXE)
PIPELINE_DEMO := $(BUILD)/demo_pipeline$(EXE)
TEST := $(BUILD)/test-crawler$(EXE)
URL_TEST := $(BUILD)/test-url$(EXE)
HTTP_TEST := $(BUILD)/test-http$(EXE)
HTML_TEST := $(BUILD)/test-html$(EXE)
LOCAL_HTTP_TEST := $(BUILD)/test-http-local$(EXE)
HTTP_ALLOC_TEST := $(BUILD)/test-http-alloc$(EXE)
HEADERS := $(wildcard include/*.h)

.PHONY: all clean test test-unit test-http test-html test-html-alloc test-http-local test-http-alloc
all: $(LIB) $(DEMO) $(PIPELINE_DEMO)

$(BUILD):
ifeq ($(OS),Windows_NT)
	if not exist "$(BUILD)" mkdir "$(BUILD)"
else
	mkdir -p "$(BUILD)"
endif

$(BUILD)/%.o: src/%.c $(HEADERS) | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(LIB): $(OBJECTS)
	$(AR) rcs $@ $^

$(DEMO): examples/demo.c $(LIB)
	$(CC) $(CFLAGS) $(LDFLAGS) $< $(LIB) $(LDLIBS) -o $@

$(PIPELINE_DEMO): examples/demo_pipeline.c $(LIB)
	$(CC) $(CFLAGS) $(LDFLAGS) $< $(LIB) $(LDLIBS) -o $@

$(TEST): tests/test_crawler.c $(LIB)
	$(CC) $(CFLAGS) $(LDFLAGS) $< $(LIB) $(LDLIBS) -o $@

$(URL_TEST): tests/test_url.c $(LIB)
	$(CC) $(CFLAGS) $(LDFLAGS) $< $(LIB) $(LDLIBS) -o $@

$(HTTP_TEST): tests/test_http.c $(LIB)
	$(CC) $(CFLAGS) $(LDFLAGS) $< $(LIB) $(LDLIBS) -o $@

$(HTML_TEST): tests/test_html.c $(LIB)
	$(CC) $(CFLAGS) $(LDFLAGS) $< $(LIB) $(LDLIBS) -o $@

$(LOCAL_HTTP_TEST): tests/test_http_local.c $(LIB)
	$(CC) $(CFLAGS) $(LDFLAGS) $< $(LIB) $(LDLIBS) -o $@

# Test-only allocator replacements. The production HTTP object is unchanged.
$(BUILD)/http-alloc-test.o: src/http.c $(HEADERS) | $(BUILD)
	$(CC) $(CFLAGS) -Dmalloc=http_test_malloc -Drealloc=http_test_realloc -Dcalloc=http_test_calloc -Dfree=http_test_free -c $< -o $@

$(HTTP_ALLOC_TEST): tests/test_http_local.c $(BUILD)/http-alloc-test.o $(LIB)
	$(CC) $(CFLAGS) $(LDFLAGS) -DHTTP_ALLOC_TEST $< $(BUILD)/http-alloc-test.o $(LIB) $(LDLIBS) -o $@

# Unit tests need no sockets. Local fixtures need loopback, not Internet access.
# The original HTTP suite still uses public network endpoints.
test: test-unit test-http-local test-http-alloc test-http

test-unit: $(TEST) $(URL_TEST) $(HTML_TEST)
	$(RUN)test-crawler$(EXE)
	$(RUN)test-url$(EXE)
	$(RUN)test-html$(EXE)

test-http: $(HTTP_TEST)
	$(RUN)test-http$(EXE)

test-http-local: $(LOCAL_HTTP_TEST)
	$(RUN)test-http-local$(EXE)

test-http-alloc: $(HTTP_ALLOC_TEST)
	$(RUN)test-http-alloc$(EXE)

test-html: $(HTML_TEST)
	$(RUN)test-html$(EXE)

# Optional: requires a GNU-compatible linker (Linux or MSYS2).
$(BUILD)/test-html-alloc$(EXE): tests/test_html_alloc.c src/html.c src/url.c $(HEADERS) | $(BUILD)
	$(CC) $(CFLAGS) $(LDFLAGS) tests/test_html_alloc.c src/html.c src/url.c -Wl,--wrap=malloc -Wl,--wrap=realloc -Wl,--wrap=free -o $@

test-html-alloc: $(BUILD)/test-html-alloc$(EXE)
	$(RUN)test-html-alloc$(EXE)

clean:
ifeq ($(OS),Windows_NT)
	if exist "$(BUILD)" rmdir /s /q "$(BUILD)"
else
	rm -rf "$(BUILD)"
endif
