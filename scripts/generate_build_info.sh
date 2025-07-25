#!/bin/bash

# Simplified build information generation script
# Generates version, build time, salt for Go code, checksum for C++

set -e

# Get project root directory
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# Build info cache file to ensure consistency
BUILD_INFO_CACHE="$PROJECT_ROOT/.build_info_cache"

# Generate version number
get_version() {
    if command -v git >/dev/null 2>&1 && [ -d "$PROJECT_ROOT/.git" ]; then
        git describe --tags --abbrev=0 2>/dev/null || echo "0.1.0"
    else
        echo "0.1.0"
    fi
}

# Generate or load cached build information
generate_build_info() {
    # Try to load from cache first
    if [ -f "$BUILD_INFO_CACHE" ] && [ -n "${USE_CACHED_BUILD_INFO:-}" ]; then
        source "$BUILD_INFO_CACHE"
        return
    fi
    
    # Generate fresh build information
    VERSION=$(get_version)
    SALT=$(openssl rand -hex 16 2>/dev/null || date +%s | sha256sum | cut -c1-32)
    BUILD_TIME=$(date -u +"%Y-%m-%dT%H:%M:%SZ")

    # Calculate checksum (CRC32)
    CHECKSUM=$(printf "%s|%s|%s" "$VERSION" "$SALT" "$BUILD_TIME" | python3 -c "
import sys, zlib
data = sys.stdin.read().encode('utf-8')
print(zlib.crc32(data) & 0xffffffff)
" 2>/dev/null || echo "0")

    # Cache the build info for consistency across build steps
    cat > "$BUILD_INFO_CACHE" << EOF
VERSION="$VERSION"
SALT="$SALT"
BUILD_TIME="$BUILD_TIME"
CHECKSUM="$CHECKSUM"
EOF
}

# Load build information
generate_build_info

# Update Go code constants
update_go_constants() {
    local go_file="$PROJECT_ROOT/pkg/agent_validator.go"
    sed -e "s/AgentVersion = \"{{JVMTOOL_VERSION}}\"/AgentVersion = \"$VERSION\"/" \
        -e "s/AgentSalt    = \"{{JVMTOOL_SALT}}\"/AgentSalt    = \"$SALT\"/" \
        -e "s/AgentBuild   = \"{{JVMTOOL_BUILD}}\"/AgentBuild   = \"$BUILD_TIME\"/" \
        "$go_file" > "${go_file}.tmp" && mv "${go_file}.tmp" "$go_file"
    echo "✓ Updated Go build constants"
}

# Restore Go constants to placeholder form (for security)
restore_go_placeholders() {
    local go_file="$PROJECT_ROOT/pkg/agent_validator.go"
    sed -e "s/AgentVersion = \"[^\"]*\"/AgentVersion = \"{{JVMTOOL_VERSION}}\"/" \
        -e "s/AgentSalt    = \"[^\"]*\"/AgentSalt    = \"{{JVMTOOL_SALT}}\"/" \
        -e "s/AgentBuild   = \"[^\"]*\"/AgentBuild   = \"{{JVMTOOL_BUILD}}\"/" \
        "$go_file" > "${go_file}.tmp" && mv "${go_file}.tmp" "$go_file"
    echo "✓ Restored Go build constants to placeholders"
}

# Handle different operations based on command line arguments
case "${1:-}" in
    "cmake")
        # Output CMake variable definition for C++ compilation
        echo "set(JVMTOOL_CHECKSUM $CHECKSUM)"
        ;;
    "go")
        # Update Go code constants only
        update_go_constants
        ;;
    "build"|"all")
        # Build mode: update Go code and output CMake variables
        update_go_constants
        echo "set(JVMTOOL_VERSION \"$VERSION\")"
        echo "set(JVMTOOL_BUILD \"$BUILD_TIME\")"
        echo "set(JVMTOOL_CHECKSUM $CHECKSUM)"
        ;;
    "restore")
        # Restore placeholders (for security after build)
        restore_go_placeholders
        ;;
    "clean")
        # Clean any cached build information
        restore_go_placeholders
        rm -f "$BUILD_INFO_CACHE"
        echo "✓ Build information cleaned"
        ;;
    *)
        # Default: display build information (for debugging only in development)
        echo "Build Info:"
        echo "  Version: $VERSION"
        echo "  Build Time: $BUILD_TIME"
        echo "  Salt: [REDACTED]"
        echo "  Checksum: [REDACTED]"
        ;;
esac