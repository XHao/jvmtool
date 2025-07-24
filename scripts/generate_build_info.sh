#!/bin/bash

# Script to generate build information including version and salt
# This script is called during the build process to generate build-time metadata

set -e

# Get project root directory
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# Generate version - prioritize git tag, fallback to hardcoded version
get_version() {
    # Try to get the latest git tag
    local git_tag=""
    if command -v git >/dev/null 2>&1 && [ -d "$PROJECT_ROOT/.git" ]; then
        git_tag=$(cd "$PROJECT_ROOT" && git describe --tags --exact-match HEAD 2>/dev/null)
        if [ -z "$git_tag" ]; then
            # If no exact tag on HEAD, try the latest tag
            git_tag=$(cd "$PROJECT_ROOT" && git describe --tags --abbrev=0 2>/dev/null)
        fi
    fi
    
    # Use git tag if available, otherwise fallback to hardcoded version
    if [ -n "$git_tag" ]; then
        echo "$git_tag"
    else
        echo "0.1.0"
    fi
}

VERSION=$(get_version)

# Generate a random salt (32 characters)
SALT=$(openssl rand -hex 16 2>/dev/null || python3 -c "import secrets; print(secrets.token_hex(16))" 2>/dev/null || echo "$(date +%s)$(hostname)" | sha256sum | cut -c1-32)

# Get current timestamp
BUILD_TIME=$(date -u +"%Y-%m-%dT%H:%M:%SZ")

# Calculate a simple checksum (sum of all characters in version + salt + build_time)
CHECKSUM=$(printf "%s%s%s" "$VERSION" "$SALT" "$BUILD_TIME" | od -An -N1000 -tu1 | tr -d ' \n' | fold -w1 | awk '{sum += $1} END {print sum % 4294967296}')

# Function to update Go build constants
update_go_constants() {
    local go_file="$PROJECT_ROOT/pkg/agent_validator.go"
    
    # Create a temporary file with updated constants
    sed -e "s/ExpectedAgentVersion  = \"{{JVMTOOL_VERSION}}\"/ExpectedAgentVersion  = \"$VERSION\"/" \
        -e "s/ExpectedAgentSalt     = \"{{JVMTOOL_SALT}}\"/ExpectedAgentSalt     = \"$SALT\"/" \
        -e "s/ExpectedAgentBuild    = \"{{JVMTOOL_BUILD}}\"/ExpectedAgentBuild    = \"$BUILD_TIME\"/" \
        -e "s/ExpectedAgentChecksum = 0 \/\/ {{JVMTOOL_CHECKSUM}}/ExpectedAgentChecksum = $CHECKSUM/" \
        "$go_file" > "${go_file}.tmp"
    
    # Replace the original file
    mv "${go_file}.tmp" "$go_file"
    echo "Updated Go build constants: $go_file"
}

# Function to restore Go constants to placeholder form
restore_go_placeholders() {
    local go_file="$PROJECT_ROOT/pkg/agent_validator.go"
    
    # Create a temporary file with placeholder constants
    sed -e "s/ExpectedAgentVersion  = \"[^\"]*\"/ExpectedAgentVersion  = \"{{JVMTOOL_VERSION}}\"/" \
        -e "s/ExpectedAgentSalt     = \"[^\"]*\"/ExpectedAgentSalt     = \"{{JVMTOOL_SALT}}\"/" \
        -e "s/ExpectedAgentBuild    = \"[^\"]*\"/ExpectedAgentBuild    = \"{{JVMTOOL_BUILD}}\"/" \
        -e "s/ExpectedAgentChecksum = [0-9]*/ExpectedAgentChecksum = 0 \/\/ {{JVMTOOL_CHECKSUM}}/" \
        "$go_file" > "${go_file}.tmp"
    
    # Replace the original file
    mv "${go_file}.tmp" "$go_file"
    echo "Restored Go build constants to placeholders: $go_file"
}

# Output format depends on the first argument
case "${1:-}" in
    "cmake")
        # Output for CMake: set variables
        echo "set(JVMTOOL_VERSION \"$VERSION\")"
        echo "set(JVMTOOL_SALT \"$SALT\")"
        echo "set(JVMTOOL_BUILD \"$BUILD_TIME\")"
        echo "set(JVMTOOL_CHECKSUM $CHECKSUM)"
        ;;
    "cflags")
        # Output for direct compiler flags
        echo "-DJVMTOOL_VERSION=\\\"$VERSION\\\" -DJVMTOOL_SALT=\\\"$SALT\\\" -DJVMTOOL_BUILD=\\\"$BUILD_TIME\\\" -DJVMTOOL_CHECKSUM=$CHECKSUM"
        ;;
    "env")
        # Output for environment variables
        echo "export JVMTOOL_VERSION=\"$VERSION\""
        echo "export JVMTOOL_SALT=\"$SALT\""
        echo "export JVMTOOL_BUILD=\"$BUILD_TIME\""
        echo "export JVMTOOL_CHECKSUM=$CHECKSUM"
        ;;
    "update-go")
        # Update Go build constants file
        update_go_constants
        ;;
    "restore")
        # Restore Go constants to placeholder form
        restore_go_placeholders
        ;;
    "all")
        # Update both Go constants and output for cmake
        update_go_constants
        echo "set(JVMTOOL_VERSION \"$VERSION\")"
        echo "set(JVMTOOL_SALT \"$SALT\")"
        echo "set(JVMTOOL_BUILD \"$BUILD_TIME\")"
        echo "set(JVMTOOL_CHECKSUM $CHECKSUM)"
        ;;
    *)
        # Default: human readable output
        echo "Generated build info:"
        echo "  Version: $VERSION"
        echo "  Salt: $SALT"
        echo "  Build time: $BUILD_TIME"
        echo "  Checksum: $CHECKSUM"
        ;;
esac

