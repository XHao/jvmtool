// Auto-generated build information
// This file is generated during build process, do not edit manually

#ifndef BUILD_INFO_H
#define BUILD_INFO_H

#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

// Magic signature for identification
#define JVMTOOL_MAGIC "JVMTOOLLOOTMVJ"

// Build metadata structure
struct jvmtool_metadata {
    char magic[16];          // "JVMTOOLLOOTMVJ\0"
    uint32_t checksum;      // Simple checksum of above fields
};

// Declare the global metadata instance
extern const struct jvmtool_metadata jvmtool_build_info;

#ifdef __cplusplus
}
#endif

#endif // BUILD_INFO_H
