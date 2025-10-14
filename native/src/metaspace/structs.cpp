#include "metaspace/structs.h"

#include <cstring>

#include "common.h"

namespace jvmtool {
namespace {

template <typename T>
inline T ptrAt(uintptr_t base, uintptr_t offset) {
    return reinterpret_cast<T>(base + offset);  // NOLINT(performance-no-int-to-ptr)
}

}  // namespace

bool MetaspaceStructs::_has_metaspace_structs = false;
void** MetaspaceStructs::_shared_metaspace_base_addr = nullptr;
void** MetaspaceStructs::_shared_metaspace_top_addr = nullptr;
void* MetaspaceStructs::_shared_metaspace_base = nullptr;
void* MetaspaceStructs::_shared_metaspace_top = nullptr;
size_t* MetaspaceStructs::_metaspace_capacity_until_gc_addr = nullptr;
size_t* MetaspaceStructs::_metaspace_used_words_addr = nullptr;
size_t* MetaspaceStructs::_metaspace_committed_words_addr = nullptr;
size_t MetaspaceStructs::_metaspace_capacity_until_gc_words = 0;
size_t MetaspaceStructs::_metaspace_used_words = 0;
size_t MetaspaceStructs::_metaspace_committed_words = 0;
void** MetaspaceStructs::_compressed_class_space_virtual_space_addr = nullptr;
void* MetaspaceStructs::_compressed_class_space_virtual_space = nullptr;
void* MetaspaceStructs::_compressed_class_space_low = nullptr;
void* MetaspaceStructs::_compressed_class_space_high = nullptr;
void* MetaspaceStructs::_compressed_class_space_low_boundary = nullptr;
void* MetaspaceStructs::_compressed_class_space_high_boundary = nullptr;
int MetaspaceStructs::_virtual_space_low_offset = -1;
int MetaspaceStructs::_virtual_space_high_offset = -1;
int MetaspaceStructs::_virtual_space_low_boundary_offset = -1;
int MetaspaceStructs::_virtual_space_high_boundary_offset = -1;

void MetaspaceStructs::init(CodeCache* libjvm) {
    if (libjvm != nullptr) {
        initMetaspaceOffsets();
    }
}

void MetaspaceStructs::ready() {
    resolveMetaspaceOffsets();
}

void MetaspaceStructs::initMetaspaceOffsets() {
    uintptr_t entry = readSymbol("gHotSpotVMStructs");
    const uintptr_t stride = readSymbol("gHotSpotVMStructEntryArrayStride");
    const uintptr_t type_offset = readSymbol("gHotSpotVMStructEntryTypeNameOffset");
    const uintptr_t field_offset = readSymbol("gHotSpotVMStructEntryFieldNameOffset");
    const uintptr_t offset_offset = readSymbol("gHotSpotVMStructEntryOffsetOffset");
    const uintptr_t address_offset = readSymbol("gHotSpotVMStructEntryAddressOffset");

    if (entry != 0 && stride != 0) {
        for (;; entry += stride) {
            const char* type = *ptrAt<const char**>(entry, type_offset);
            const char* field = *ptrAt<const char**>(entry, field_offset);
            if (type == nullptr || field == nullptr) {
                break;
            }

            const int field_offset_value =
                (offset_offset != 0) ? *ptrAt<int*>(entry, offset_offset) : -1;

            if (strcmp(type, "MetaspaceObj") == 0) {
                if (strcmp(field, "_shared_metaspace_base") == 0) {
                    _shared_metaspace_base_addr = *ptrAt<void***>(entry, address_offset);
                } else if (strcmp(field, "_shared_metaspace_top") == 0) {
                    _shared_metaspace_top_addr = *ptrAt<void***>(entry, address_offset);
                }
            } else if (strcmp(type, "Metaspace") == 0) {
                if (strcmp(field, "_capacity_until_GC") == 0) {
                    _metaspace_capacity_until_gc_addr = *ptrAt<size_t**>(entry, address_offset);
                } else if (strcmp(field, "_used_words") == 0) {
                    _metaspace_used_words_addr = *ptrAt<size_t**>(entry, address_offset);
                } else if (strcmp(field, "_committed_words") == 0) {
                    _metaspace_committed_words_addr = *ptrAt<size_t**>(entry, address_offset);
                }
            } else if (strcmp(type, "CompressedClassSpace") == 0) {
                if (strcmp(field, "_virtual_space") == 0) {
                    _compressed_class_space_virtual_space_addr =
                        *ptrAt<void***>(entry, address_offset);
                }
            } else if (strcmp(type, "VirtualSpace") == 0) {
                if (strcmp(field, "_low") == 0) {
                    _virtual_space_low_offset = field_offset_value;
                } else if (strcmp(field, "_high") == 0) {
                    _virtual_space_high_offset = field_offset_value;
                } else if (strcmp(field, "_low_boundary") == 0) {
                    _virtual_space_low_boundary_offset = field_offset_value;
                } else if (strcmp(field, "_high_boundary") == 0) {
                    _virtual_space_high_boundary_offset = field_offset_value;
                }
            }
        }
    }
}

void MetaspaceStructs::resolveMetaspaceOffsets() {
    if (_shared_metaspace_base_addr != nullptr) {
        _shared_metaspace_base = *_shared_metaspace_base_addr;
    }

    if (_shared_metaspace_top_addr != nullptr) {
        _shared_metaspace_top = *_shared_metaspace_top_addr;
    }

    if (_metaspace_capacity_until_gc_addr != nullptr) {
        _metaspace_capacity_until_gc_words = *_metaspace_capacity_until_gc_addr;
    } else {
        _metaspace_capacity_until_gc_words = 0;
    }

    if (_metaspace_used_words_addr != nullptr) {
        _metaspace_used_words = *_metaspace_used_words_addr;
    } else {
        _metaspace_used_words = 0;
    }

    if (_metaspace_committed_words_addr != nullptr) {
        _metaspace_committed_words = *_metaspace_committed_words_addr;
    } else {
        _metaspace_committed_words = 0;
    }

    _compressed_class_space_low = nullptr;
    _compressed_class_space_high = nullptr;
    _compressed_class_space_low_boundary = nullptr;
    _compressed_class_space_high_boundary = nullptr;

    if (_compressed_class_space_virtual_space_addr != nullptr) {
        _compressed_class_space_virtual_space = *_compressed_class_space_virtual_space_addr;
    } else {
        _compressed_class_space_virtual_space = nullptr;
    }

    if (_compressed_class_space_virtual_space != nullptr) {
        const uintptr_t vs_base =
            reinterpret_cast<uintptr_t>(_compressed_class_space_virtual_space);
        auto read_vs_ptr = [vs_base](int offset) -> void* {
            if (offset < 0) {
                return nullptr;
            }
            return *reinterpret_cast<void* const*>(vs_base + static_cast<uintptr_t>(offset));
        };

        _compressed_class_space_low = read_vs_ptr(_virtual_space_low_offset);
        _compressed_class_space_high = read_vs_ptr(_virtual_space_high_offset);
        _compressed_class_space_low_boundary = read_vs_ptr(_virtual_space_low_boundary_offset);
        _compressed_class_space_high_boundary = read_vs_ptr(_virtual_space_high_boundary_offset);
    }

    _has_metaspace_structs =
        (_shared_metaspace_base_addr != nullptr && _shared_metaspace_top_addr != nullptr);
}

}  // namespace jvmtool
