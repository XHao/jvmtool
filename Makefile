# Makefile for jattach

BINARY_NAME = jvmtool 
BUILD_DIR = build

.PHONY: all build test clean package

all: build

build:
	go build -o $(BUILD_DIR)/$(BINARY_NAME) ./cmd

test:
	go test ./...

package: build
	tar -czvf $(BUILD_DIR)/$(BINARY_NAME).tar.gz -C $(BUILD_DIR) $(BINARY_NAME)

clean:
	rm -rf $(BUILD_DIR)
	go clean