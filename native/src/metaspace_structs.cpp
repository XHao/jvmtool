/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 *
 * Modified by: shako
 * - Added metaspace-related VM structures support
 * - This file extends the original VM structures with metaspace functionality
 * - Located outside vm/ directory as it's project-specific extension
 */

#include "metaspace_structs.h"
#include "vm/vm.h"

namespace jvmtool {

// Static member definitions
bool MetaspaceStructs::_has_metaspace_structs = false;
void** MetaspaceStructs::_shared_metaspace_base_addr = nullptr;
void** MetaspaceStructs::_shared_metaspace_top_addr = nullptr;
void* MetaspaceStructs::_shared_metaspace_base = nullptr;
void* MetaspaceStructs::_shared_metaspace_top = nullptr;

// Run at agent load time
void MetaspaceStructs::init(CodeCache* libjvm) {
    if (libjvm != nullptr) {
        initMetaspaceOffsets();
    }
}

// Run when VM is initialized and JNI is available
void MetaspaceStructs::ready() {
    VMStructs::ready();
    
    resolveMetaspaceOffsets();
}

void MetaspaceStructs::initMetaspaceOffsets() {
    uintptr_t entry = readSymbol("gHotSpotVMStructs");
    uintptr_t stride = readSymbol("gHotSpotVMStructEntryArrayStride");
    uintptr_t type_offset = readSymbol("gHotSpotVMStructEntryTypeNameOffset");
    uintptr_t field_offset = readSymbol("gHotSpotVMStructEntryFieldNameOffset");
    uintptr_t address_offset = readSymbol("gHotSpotVMStructEntryAddressOffset");

    if (entry != 0 && stride != 0) {
        for (;; entry += stride) {
            const char* type = *(const char**)(entry + type_offset);
            const char* field = *(const char**)(entry + field_offset);
            if (type == nullptr || field == nullptr) {
                break;
            }

            // Look for MetaspaceObj static fields
            if (strcmp(type, "MetaspaceObj") == 0) {
                if (strcmp(field, "_shared_metaspace_base") == 0) {
                    _shared_metaspace_base_addr = *(void***)(entry + address_offset);
                } else if (strcmp(field, "_shared_metaspace_top") == 0) {
                    _shared_metaspace_top_addr = *(void***)(entry + address_offset);
                }
            }
        }
    }
}

void MetaspaceStructs::resolveMetaspaceOffsets() {
    // Resolve shared metaspace boundaries
    if (_shared_metaspace_base_addr != nullptr) {
        _shared_metaspace_base = *_shared_metaspace_base_addr;
    }
    
    if (_shared_metaspace_top_addr != nullptr) {
        _shared_metaspace_top = *_shared_metaspace_top_addr;
    }

    // Check if we have valid metaspace structures
    _has_metaspace_structs = (_shared_metaspace_base_addr != nullptr && 
                              _shared_metaspace_top_addr != nullptr);
}

} // namespace jvmtool
