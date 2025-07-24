# Makefile for jattach

BINARY_NAME = jvmtool 
BUILD_DIR = build
DIST_DIR = dist
NATIVE_BUILD_DIR = native/build

# Installation prefix - can be overridden by user
PREFIX ?= /usr/local
BINDIR = $(PREFIX)/bin
LIBDIR = $(PREFIX)/lib

# Detect OS for library extension
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
    LIB_EXT = so
endif
ifeq ($(UNAME_S),Darwin)
    LIB_EXT = dylib
endif
ifeq ($(OS),Windows_NT)
    LIB_EXT = dll
    BINARY_NAME = jvmtool.exe
endif

AGENT_LIB = jvmtool-agent.$(LIB_EXT)

.PHONY: all build build-go build-native test clean package install uninstall install-info help dirs format format-check lint

all: build

# Create necessary directories
dirs:
	@mkdir -p $(BUILD_DIR)
	@mkdir -p $(DIST_DIR)/bin
	@mkdir -p $(DIST_DIR)/lib
	@mkdir -p $(NATIVE_BUILD_DIR)

# Build both Go binary and native agent
build: dirs generate-build-info build-go build-native restore-placeholders

# Generate build-time information for both Go and native code
generate-build-info:
	@echo "Generating build information..."
	@./scripts/generate_build_info.sh update-go

# Build Go binary (depends on build info generation)
build-go: dirs generate-build-info
	@echo "Building Go binary..."
	go build -o $(BUILD_DIR)/$(BINARY_NAME) ./cmd
	cp $(BUILD_DIR)/$(BINARY_NAME) $(DIST_DIR)/bin/

# Build native agent library
build-native: dirs
	@echo "Building native agent library..."
	@cd $(NATIVE_BUILD_DIR) && cmake .. && make jvmtool-agent
	@if [ -f "$(NATIVE_BUILD_DIR)/$(AGENT_LIB)" ]; then \
		cp $(NATIVE_BUILD_DIR)/$(AGENT_LIB) $(DIST_DIR)/lib/; \
		echo "Native agent library built: $(DIST_DIR)/lib/$(AGENT_LIB)"; \
	elif [ -f "$(NATIVE_BUILD_DIR)/lib$(AGENT_LIB)" ]; then \
		cp $(NATIVE_BUILD_DIR)/lib$(AGENT_LIB) $(DIST_DIR)/lib/$(AGENT_LIB); \
		echo "Native agent library built: $(DIST_DIR)/lib/$(AGENT_LIB)"; \
	else \
		echo "Warning: Native agent library not found after build"; \
	fi

# Restore Go constants to placeholder form after build
restore-placeholders:
	@echo "Restoring Go constants to placeholder form..."
	@./scripts/generate_build_info.sh restore

test:
	go test ./...

# Code formatting and linting targets
format: build-native
	@echo "Formatting native code..."
	cd native && cmake --build build --target format

format-check: build-native
	@echo "Checking native code format..."
	cd native && cmake --build build --target format-check

lint: build-native
	@echo "Running lint checks on native code..."
	cd native && cmake --build build --target lint

# Create distribution package with proper directory structure
package: build
	@echo "Creating package..."
	@PACKAGE_NAME="$(BINARY_NAME)-$$(uname -s)-$$(uname -m).tar.gz"; \
	echo "Package name: $$PACKAGE_NAME"; \
	cd $(DIST_DIR) && tar -czf "../$(BUILD_DIR)/$$PACKAGE_NAME" bin/ lib/; \
	echo "Package created: $(BUILD_DIR)/$$PACKAGE_NAME"

# Install to system directories
install: build
	@echo "Installing jvmtool to $(PREFIX)..."
	@echo "Creating directories..."
	install -d $(BINDIR)
	install -d $(LIBDIR)
	@echo "Installing binary..."
	install -m 0755 $(DIST_DIR)/bin/$(BINARY_NAME) $(BINDIR)/
	@if [ -f "$(DIST_DIR)/lib/$(AGENT_LIB)" ]; then \
		echo "Installing native agent library..."; \
		install -m 0755 $(DIST_DIR)/lib/$(AGENT_LIB) $(LIBDIR)/; \
		echo "✓ Installed $(AGENT_LIB) to $(LIBDIR)/"; \
	else \
		echo "⚠ Native agent library not found, skipping..."; \
	fi
	@echo ""
	@echo "✓ Installation complete!"
	@echo "  Binary: $(BINDIR)/$(BINARY_NAME)"
	@echo "  Library: $(LIBDIR)/$(AGENT_LIB)"
	@echo ""
	@echo "Usage: $(BINARY_NAME) sa --pid <java_process_pid> --analysis <type>"
	@echo "Types: memory, thread, class, heap, all"

# Uninstall from system directories
uninstall:
	@echo "Uninstalling jvmtool from $(PREFIX)..."
	@if [ -f "$(BINDIR)/$(BINARY_NAME)" ]; then \
		rm -f $(BINDIR)/$(BINARY_NAME); \
		echo "✓ Removed $(BINDIR)/$(BINARY_NAME)"; \
	else \
		echo "⚠ Binary not found at $(BINDIR)/$(BINARY_NAME)"; \
	fi
	@REMOVED_LIBS=false; \
	for ext in dylib so dll; do \
		LIB_FILE="$(LIBDIR)/jvmtool-agent.$$ext"; \
		if [ -f "$$LIB_FILE" ]; then \
			rm -f "$$LIB_FILE"; \
			echo "✓ Removed $$LIB_FILE"; \
			REMOVED_LIBS=true; \
		fi; \
	done; \
	if [ "$$REMOVED_LIBS" = false ]; then \
		echo "⚠ No agent libraries found in $(LIBDIR)"; \
	fi
	@if [ -d "$(LIBDIR)" ] && [ -z "$$(ls -A $(LIBDIR) 2>/dev/null)" ]; then \
		rmdir $(LIBDIR); \
		echo "✓ Removed empty directory $(LIBDIR)"; \
	fi
	@echo "✓ Uninstallation complete."

# Show installation information
install-info:
	@echo "Installation Information:"
	@echo "  PREFIX: $(PREFIX)"
	@echo "  BINDIR: $(BINDIR)"
	@echo "  LIBDIR: $(LIBDIR)"
	@echo ""
	@echo "To install with custom prefix:"
	@echo "  make install PREFIX=/path/to/install"
	@echo ""
	@echo "Examples:"
	@echo "  make install                     # Install to /usr/local (requires sudo)"
	@echo "  make install PREFIX=/opt/jvmtool # Install to /opt/jvmtool"
	@echo "  make install PREFIX=\$$HOME/.local # Install to user directory"

# Help target
help:
	@echo "jvmtool Makefile"
	@echo ""
	@echo "Available targets:"
	@echo "  build         Build both Go binary and native agent"
	@echo "  build-go      Build only the Go binary"
	@echo "  build-native  Build only the native agent library"
	@echo "  test          Run Go tests"
	@echo "  format        Format native C++ code"
	@echo "  format-check  Check native C++ code formatting"
	@echo "  lint          Run lint checks on native code"
	@echo "  package       Create distribution package"
	@echo "  install       Install to system (requires build first)"
	@echo "  uninstall     Remove from system"
	@echo "  install-info  Show installation information"
	@echo "  clean         Clean build artifacts"
	@echo "  help          Show this help message"
	@echo ""
	@echo "Configuration:"
	@echo "  PREFIX        Installation prefix (default: /usr/local)"
	@echo ""
	@echo "Examples:"
	@echo "  make build"
	@echo "  make format"
	@echo "  make format-check"
	@echo "  make install"
	@echo "  make install PREFIX=/opt/jvmtool"
	@echo "  make package"
	@echo "  make clean"

clean:
	rm -rf $(BUILD_DIR) $(DIST_DIR) $(NATIVE_BUILD_DIR)
	go clean