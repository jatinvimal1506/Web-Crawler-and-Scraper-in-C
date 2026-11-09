CC = gcc
AR = ar
# Append project flags: Replit/Nix supplies dependency includes in CFLAGS.
CFLAGS += -std=c11 -Wall -Wextra -Wpedantic -Iinclude
LDLIBS ?= -lcurl
BUILD := build
LIB := $(BUILD)/libcrawler.a
SOURCES := src/project.c src/url.c src/http.c src/http_scoped.c src/html.c src/queue.c src/visited.c src/document.c
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
BFS_DEMO := $(BUILD)/crawler-bfs$(EXE)
CRAWLER_FIXTURE := $(BUILD)/crawler-fixture$(EXE)
DOCUMENT_DEMO := $(BUILD)/discover-documents$(EXE)
TEST := $(BUILD)/test-crawler$(EXE)
URL_TEST := $(BUILD)/test-url$(EXE)
HTTP_TEST := $(BUILD)/test-http$(EXE)
HTML_TEST := $(BUILD)/test-html$(EXE)
LOCAL_HTTP_TEST := $(BUILD)/test-http-local$(EXE)
HTTP_ALLOC_TEST := $(BUILD)/test-http-alloc$(EXE)
QUEUE_TEST := $(BUILD)/test-queue$(EXE)
VISITED_TEST := $(BUILD)/test-visited$(EXE)
PHASE5_ALLOC_TEST := $(BUILD)/test-phase5-alloc$(EXE)
SAFETY_TEST := $(BUILD)/test-phase7-safety$(EXE)
PHASE7_ALLOC_TEST := $(BUILD)/test-phase7-alloc$(EXE)
DOCUMENT_TEST := $(BUILD)/test-document$(EXE)
DOCUMENT_INTEGRATION_TEST := $(BUILD)/test-document-integration$(EXE)
DOCUMENT_ALLOC_TEST := $(BUILD)/test-document-alloc$(EXE)
CRAWLER_ALLOC_TEST := $(BUILD)/test-crawler-alloc$(EXE)
CRAWLER_ENGINE_TEST := $(BUILD)/test-crawler-engine$(EXE)
TEST_HEADERS := tests/phase5_check.h
HEADERS := $(wildcard include/*.h) src/http_internal.h

.PHONY: all clean test test-unit test-http test-html test-html-alloc test-http-local test-http-alloc test-phase5 test-phase5-alloc test-crawler-engine test-crawler-alloc test-phase7-safety test-phase7-alloc test-document test-document-integration test-document-alloc
all: $(LIB) $(DEMO) $(PIPELINE_DEMO) $(BFS_DEMO) $(DOCUMENT_DEMO)

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

$(BFS_DEMO): examples/bfs_demo.c $(LIB)
	$(CC) $(CFLAGS) $(LDFLAGS) $< $(LIB) $(LDLIBS) -o $@

$(DOCUMENT_DEMO): examples/discover_documents.c $(LIB)
	$(CC) $(CFLAGS) $(LDFLAGS) $< $(LIB) $(LDLIBS) -o $@

$(CRAWLER_FIXTURE): tests/crawler_fixture_main.c tests/crawler_fixture.c tests/crawler_fixture.h $(LIB) $(BFS_DEMO) $(DOCUMENT_DEMO)
	$(CC) $(CFLAGS) $(LDFLAGS) tests/crawler_fixture_main.c tests/crawler_fixture.c $(LIB) $(LDLIBS) -o $@

$(CRAWLER_ENGINE_TEST): tests/test_crawler_engine.c tests/crawler_fixture.c tests/crawler_fixture.h $(TEST_HEADERS) $(LIB)
	$(CC) $(CFLAGS) $(LDFLAGS) tests/test_crawler_engine.c tests/crawler_fixture.c $(LIB) $(LDLIBS) -o $@

test-crawler-engine: $(CRAWLER_ENGINE_TEST)
	$(RUN)test-crawler-engine$(EXE)

$(TEST): tests/test_crawler.c tests/crawler_stub.c tests/crawler_stub.h $(LIB)
	$(CC) $(CFLAGS) $(LDFLAGS) tests/test_crawler.c tests/crawler_stub.c $(LIB) -o $@

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

$(QUEUE_TEST): tests/test_queue.c $(TEST_HEADERS) $(LIB)
	$(CC) $(CFLAGS) $(LDFLAGS) $< $(LIB) -o $@

$(VISITED_TEST): tests/test_visited.c $(TEST_HEADERS) $(LIB)
	$(CC) $(CFLAGS) $(LDFLAGS) $< $(LIB) -o $@

# Separate allocator-instrumented C objects, NEVER added to libcrawler.a.
# These three modules currently allocate with malloc/free, not realloc/calloc.
$(BUILD)/phase5-alloc-%.o: src/%.c $(HEADERS) | $(BUILD)
	$(CC) $(CFLAGS) -Dmalloc=phase5_test_malloc -Dfree=phase5_test_free -c $< -o $@

PHASE5_ALLOC_OBJECTS := $(BUILD)/phase5-alloc-queue.o $(BUILD)/phase5-alloc-visited.o $(BUILD)/phase5-alloc-url.o
$(PHASE5_ALLOC_TEST): tests/test_phase5_alloc.c $(TEST_HEADERS) $(PHASE5_ALLOC_OBJECTS)
	$(CC) $(CFLAGS) $(LDFLAGS) $< $(PHASE5_ALLOC_OBJECTS) -o $@

test-phase5: $(QUEUE_TEST) $(VISITED_TEST)
	$(RUN)test-queue$(EXE)
	$(RUN)test-visited$(EXE)

test-phase5-alloc: $(PHASE5_ALLOC_TEST)
	$(RUN)test-phase5-alloc$(EXE)

CRAWLER_ALLOC_NAMES := -Dmalloc=crawler_test_malloc -Drealloc=crawler_test_realloc -Dcalloc=crawler_test_calloc -Dfree=crawler_test_free
CRAWLER_ENGINE_HOOKS := -Dqueue_enqueue=crawler_test_enqueue -Dvisited_insert=crawler_test_insert -Dqueue_dequeue=crawler_test_dequeue
CRAWLER_ALLOC_OBJECTS := $(BUILD)/crawl-alloc-project.o $(BUILD)/crawl-alloc-url.o $(BUILD)/crawl-alloc-html.o $(BUILD)/crawl-alloc-queue.o $(BUILD)/crawl-alloc-visited.o $(BUILD)/crawl-alloc-stub.o

$(BUILD)/crawl-alloc-project.o: src/project.c $(HEADERS) | $(BUILD)
	$(CC) $(CFLAGS) $(CRAWLER_ALLOC_NAMES) $(CRAWLER_ENGINE_HOOKS) -c $< -o $@

$(BUILD)/crawl-alloc-%.o: src/%.c $(HEADERS) | $(BUILD)
	$(CC) $(CFLAGS) $(CRAWLER_ALLOC_NAMES) -c $< -o $@

$(BUILD)/crawl-alloc-stub.o: tests/crawler_stub.c tests/crawler_stub.h $(HEADERS) | $(BUILD)
	$(CC) $(CFLAGS) $(CRAWLER_ALLOC_NAMES) -c $< -o $@

$(CRAWLER_ALLOC_TEST): tests/test_crawler_alloc.c tests/crawler_stub.h $(TEST_HEADERS) $(CRAWLER_ALLOC_OBJECTS)
	$(CC) $(CFLAGS) $(LDFLAGS) $< $(CRAWLER_ALLOC_OBJECTS) -o $@

test-crawler-alloc: $(CRAWLER_ALLOC_TEST)
	$(RUN)test-crawler-alloc$(EXE)

# Unit tests need no sockets. Local fixtures need loopback, not Internet access.
# The original HTTP suite still uses public network endpoints.
test: test-unit test-phase5-alloc test-crawler-engine test-crawler-alloc test-http-local test-http-alloc test-phase7-safety test-phase7-alloc test-document test-document-integration test-document-alloc test-http

test-unit: $(TEST) $(URL_TEST) $(HTML_TEST) test-phase5
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

$(BUILD)/http-option-test.o: src/http.c $(HEADERS) | $(BUILD)
	$(CC) $(CFLAGS) -DHTTP_OPTION_TEST -c $< -o $@

$(SAFETY_TEST): tests/test_phase7_safety.c tests/crawler_fixture.c tests/crawler_fixture.h $(BUILD)/http-option-test.o $(LIB)
	$(CC) $(CFLAGS) $(LDFLAGS) tests/test_phase7_safety.c tests/crawler_fixture.c $(BUILD)/http-option-test.o $(LIB) $(LDLIBS) -o $@

test-phase7-safety: $(SAFETY_TEST)
	$(RUN)test-phase7-safety$(EXE)

PHASE7_NAMES := -Dmalloc=phase7_malloc -Dcalloc=phase7_calloc -Drealloc=phase7_realloc -Dfree=phase7_free
PHASE7_OBJECTS := $(SOURCES:src/%.c=$(BUILD)/phase7-%.o)

$(BUILD)/phase7-%.o: src/%.c $(HEADERS) | $(BUILD)
	$(CC) $(CFLAGS) $(PHASE7_NAMES) -c $< -o $@

$(BUILD)/phase7-http.o: src/http.c $(HEADERS) | $(BUILD)
	$(CC) $(CFLAGS) $(PHASE7_NAMES) -Dhttp_download_once=phase7_real_once -c $< -o $@

$(PHASE7_ALLOC_TEST): tests/test_phase7_alloc.c tests/crawler_fixture.c tests/crawler_fixture.h $(PHASE7_OBJECTS)
	$(CC) $(CFLAGS) $(LDFLAGS) tests/test_phase7_alloc.c tests/crawler_fixture.c $(PHASE7_OBJECTS) $(LDLIBS) -o $@

test-phase7-alloc: $(PHASE7_ALLOC_TEST)
	$(RUN)test-phase7-alloc$(EXE)

$(DOCUMENT_TEST): tests/test_document.c $(LIB)
	$(CC) $(CFLAGS) $(LDFLAGS) $< $(LIB) $(LDLIBS) -o $@

$(DOCUMENT_INTEGRATION_TEST): tests/test_document_integration.c tests/crawler_fixture.c tests/crawler_fixture.h $(TEST_HEADERS) $(LIB)
	$(CC) $(CFLAGS) $(LDFLAGS) tests/test_document_integration.c tests/crawler_fixture.c $(LIB) $(LDLIBS) -o $@

DOCUMENT_ALLOC_NAMES := -Dmalloc=document_test_malloc -Drealloc=document_test_realloc -Dfree=document_test_free
DOCUMENT_ALLOC_OBJECTS := $(SOURCES:src/%.c=$(BUILD)/document-alloc-%.o)

$(BUILD)/document-alloc-%.o: src/%.c $(HEADERS) | $(BUILD)
	$(CC) $(CFLAGS) $(DOCUMENT_ALLOC_NAMES) -c $< -o $@

$(DOCUMENT_ALLOC_TEST): tests/test_document_alloc.c tests/crawler_fixture.c tests/crawler_fixture.h $(TEST_HEADERS) $(DOCUMENT_ALLOC_OBJECTS)
	$(CC) $(CFLAGS) $(LDFLAGS) $< tests/crawler_fixture.c $(DOCUMENT_ALLOC_OBJECTS) $(LDLIBS) -o $@

test-document: $(DOCUMENT_TEST)
	$(RUN)test-document$(EXE)

test-document-integration: $(DOCUMENT_INTEGRATION_TEST)
	$(RUN)test-document-integration$(EXE)

test-document-alloc: $(DOCUMENT_ALLOC_TEST)
	$(RUN)test-document-alloc$(EXE)

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
