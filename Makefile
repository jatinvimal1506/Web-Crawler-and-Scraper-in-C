CC = gcc
AR = ar
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -Iinclude
LDLIBS ?= -lcurl
BUILD := build
LIB := $(BUILD)/libcrawler.a
SOURCES := src/project.c src/url.c src/http.c src/html.c src/queue.c src/visited.c
OBJECTS := $(SOURCES:src/%.c=$(BUILD)/%.o)
DEMO := $(BUILD)/crawler-demo.exe
TEST := $(BUILD)/test-crawler.exe
URL_TEST := $(BUILD)/test-url.exe
HTTP_TEST := $(BUILD)/test-http.exe

# Use native Windows commands so PowerShell/Windows builds do not require WSL.
SHELL = cmd.exe
.SHELLFLAGS = /C

.PHONY: all clean test
all: $(LIB) $(DEMO)

$(BUILD):
	if not exist "$(BUILD)" mkdir "$(BUILD)"

$(BUILD)/%.o: src/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(LIB): $(OBJECTS)
	$(AR) rcs $@ $^

$(DEMO): examples/demo.c $(LIB)
	$(CC) $(CFLAGS) $< $(LIB) $(LDLIBS) -o $@

$(TEST): tests/test_crawler.c $(LIB)
	$(CC) $(CFLAGS) $< $(LIB) $(LDLIBS) -o $@

$(URL_TEST): tests/test_url.c $(LIB)
	$(CC) $(CFLAGS) $< $(LIB) $(LDLIBS) -o $@

$(HTTP_TEST): tests/test_http.c $(LIB)
	$(CC) $(CFLAGS) $< $(LIB) $(LDLIBS) -o $@

test: $(TEST) $(URL_TEST) $(HTTP_TEST)
	.\build\test-crawler.exe
	.\build\test-url.exe
	.\build\test-http.exe

clean:
	if exist "$(BUILD)" rmdir /s /q "$(BUILD)"

