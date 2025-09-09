#pragma once

#include "vm/vmStructs.h"

namespace jvmtool {

/**
 * MetaspaceStructs extends VMStructs with metaspace-related functionality.
 * This class provides access to metaspace object offsets and shared metaspace boundaries.
 */
class MetaspaceStructs : public VMStructs {
  protected:
    // Metaspace-related flags
    static bool _has_metaspace_structs;
    
    // MetaspaceObj static fields
    static void** _shared_metaspace_base_addr;
    static void** _shared_metaspace_top_addr;
    static void* _shared_metaspace_base;
    static void* _shared_metaspace_top;

    // Initialize metaspace-specific offsets
    static void initMetaspaceOffsets();
    static void resolveMetaspaceOffsets();

  public:
    // Initialization methods
    static void init(CodeCache* libjvm);
    static void ready();

    // Metaspace functionality checks
    static bool hasMetaspaceStructs() {
        return _has_metaspace_structs;
    }

    // Shared metaspace boundary access
    static void* sharedMetaspaceBase() {
        return _shared_metaspace_base;
    }

    static void* sharedMetaspaceTop() {
        return _shared_metaspace_top;
    }

    // Check if an object is in shared metaspace
    static bool isSharedMetaspaceObject(const void* obj) {
        if (_shared_metaspace_base == nullptr || _shared_metaspace_top == nullptr) {
            return false;
        }
        return obj >= _shared_metaspace_base && obj < _shared_metaspace_top;
    }

    // Get shared metaspace size
    static size_t sharedMetaspaceSize() {
        if (_shared_metaspace_base == nullptr || _shared_metaspace_top == nullptr) {
            return 0;
        }
        return static_cast<char*>(_shared_metaspace_top) - static_cast<char*>(_shared_metaspace_base);
    }

    // Check if shared metaspace is available
    static bool hasSharedMetaspace() {
        return _shared_metaspace_base != nullptr && _shared_metaspace_top != nullptr;
    }
};

} // namespace jvmtool
