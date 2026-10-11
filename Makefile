CC ?= cc
AR ?= ar
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -Iinclude
BUILD := build
LIB := $(BUILD)/libcrawler.a
SOURCES := src/project.c src/url.c src/http.c src/html.c src/queue.c src/visited.c
OBJECTS := $(SOURCES:src/%.c=$(BUILD)/%.o)

.PHONY: all clean test
all: $(LIB) $(BUILD)/crawler-demo

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/%.o: src/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(LIB): $(OBJECTS)
	$(AR) rcs $@ $^

$(BUILD)/crawler-demo: examples/demo.c $(LIB)
	$(CC) $(CFLAGS) $< $(LIB) -o $@

$(BUILD)/test-crawler: tests/test_crawler.c $(LIB)
	$(CC) $(CFLAGS) $< $(LIB) -o $@

test: $(BUILD)/test-crawler
	./$(BUILD)/test-crawler

clean:
	rm -rf $(BUILD)

