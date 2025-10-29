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

bool MetaspaceStructs::_has_cld_support = false;
void** MetaspaceStructs::_class_loader_data_graph_head_addr = nullptr;
void* MetaspaceStructs::_class_loader_data_graph_head = nullptr;
int MetaspaceStructs::_cld_next_offset = -1;
int MetaspaceStructs::_cld_klasses_offset = -1;
int MetaspaceStructs::_cld_metaspace_offset = -1;
int MetaspaceStructs::_cld_class_loader_offset = -1;
int MetaspaceStructs::_klass_next_link_offset = -1;
int MetaspaceStructs::_klass_class_loader_data_offset = -1;

bool MetaspaceStructs::_has_klass_details = false;
int MetaspaceStructs::_klass_layout_helper_offset = -1;
int MetaspaceStructs::_klass_super_offset = -1;
int MetaspaceStructs::_klass_java_mirror_offset = -1;
int MetaspaceStructs::_klass_name_offset = -1;
int MetaspaceStructs::_instance_klass_methods_offset = -1;
int MetaspaceStructs::_instance_klass_constants_offset = -1;
int MetaspaceStructs::_instance_klass_fields_offset = -1;
int MetaspaceStructs::_instance_klass_vtable_len_offset = -1;
int MetaspaceStructs::_instance_klass_itable_len_offset = -1;
int MetaspaceStructs::_instance_klass_annotations_offset = -1;
int MetaspaceStructs::_instance_klass_inner_classes_offset = -1;
int MetaspaceStructs::_method_size_offset = -1;
int MetaspaceStructs::_method_constMethod_offset = -1;
int MetaspaceStructs::_constMethod_size_offset = -1;
int MetaspaceStructs::_constantPool_length_offset = -1;
int MetaspaceStructs::_constantPool_size_offset = -1;

void MetaspaceStructs::init(CodeCache* libjvm) {
    if (libjvm != nullptr) {
        initMetaspaceOffsets();
        initClassLoaderDataOffsets();
        initKlassDetailOffsets();
    }
}

void MetaspaceStructs::ready() {
    resolveMetaspaceOffsets();
    resolveClassLoaderDataOffsets();
    resolveKlassDetailOffsets();
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

void MetaspaceStructs::initClassLoaderDataOffsets() {
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

            // ClassLoaderDataGraph support
            if (strcmp(type, "ClassLoaderDataGraph") == 0) {
                if (strcmp(field, "_head") == 0) {
                    _class_loader_data_graph_head_addr = *ptrAt<void***>(entry, address_offset);
                }
            }
            // ClassLoaderData structure offsets
            else if (strcmp(type, "ClassLoaderData") == 0) {
                if (strcmp(field, "_next") == 0) {
                    _cld_next_offset = field_offset_value;
                } else if (strcmp(field, "_klasses") == 0) {
                    _cld_klasses_offset = field_offset_value;
                } else if (strcmp(field, "_metaspace") == 0) {
                    _cld_metaspace_offset = field_offset_value;
                } else if (strcmp(field, "_class_loader") == 0 ||
                           strcmp(field, "_class_loader_klass") == 0) {
                    _cld_class_loader_offset = field_offset_value;
                }
            }
            // Klass structure offsets
            else if (strcmp(type, "Klass") == 0 || strcmp(type, "InstanceKlass") == 0) {
                if (strcmp(field, "_next_link") == 0) {
                    _klass_next_link_offset = field_offset_value;
                } else if (strcmp(field, "_class_loader_data") == 0) {
                    _klass_class_loader_data_offset = field_offset_value;
                }
            }
        }
    }
}

void MetaspaceStructs::resolveClassLoaderDataOffsets() {
    if (_class_loader_data_graph_head_addr != nullptr) {
        _class_loader_data_graph_head = *_class_loader_data_graph_head_addr;
    } else {
        _class_loader_data_graph_head = nullptr;
    }

    _has_cld_support = (_class_loader_data_graph_head_addr != nullptr && _cld_next_offset >= 0 &&
                        _cld_klasses_offset >= 0);
}

void MetaspaceStructs::initKlassDetailOffsets() {
    uintptr_t entry = readSymbol("gHotSpotVMStructs");
    const uintptr_t stride = readSymbol("gHotSpotVMStructEntryArrayStride");
    const uintptr_t type_offset = readSymbol("gHotSpotVMStructEntryTypeNameOffset");
    const uintptr_t field_offset = readSymbol("gHotSpotVMStructEntryFieldNameOffset");
    const uintptr_t offset_offset = readSymbol("gHotSpotVMStructEntryOffsetOffset");

    if (entry != 0 && stride != 0) {
        for (;; entry += stride) {
            const char* type = *ptrAt<const char**>(entry, type_offset);
            const char* field = *ptrAt<const char**>(entry, field_offset);
            if (type == nullptr || field == nullptr) {
                break;
            }

            const int field_offset_value =
                (offset_offset != 0) ? *ptrAt<int*>(entry, offset_offset) : -1;

            // Klass structure
            if (strcmp(type, "Klass") == 0) {
                if (strcmp(field, "_layout_helper") == 0) {
                    _klass_layout_helper_offset = field_offset_value;
                } else if (strcmp(field, "_super") == 0) {
                    _klass_super_offset = field_offset_value;
                } else if (strcmp(field, "_java_mirror") == 0) {
                    _klass_java_mirror_offset = field_offset_value;
                } else if (strcmp(field, "_name") == 0) {
                    _klass_name_offset = field_offset_value;
                }
            }
            // InstanceKlass structure
            else if (strcmp(type, "InstanceKlass") == 0) {
                if (strcmp(field, "_methods") == 0) {
                    _instance_klass_methods_offset = field_offset_value;
                } else if (strcmp(field, "_constants") == 0) {
                    _instance_klass_constants_offset = field_offset_value;
                } else if (strcmp(field, "_fields") == 0) {
                    _instance_klass_fields_offset = field_offset_value;
                } else if (strcmp(field, "_vtable_len") == 0) {
                    _instance_klass_vtable_len_offset = field_offset_value;
                } else if (strcmp(field, "_itable_len") == 0) {
                    _instance_klass_itable_len_offset = field_offset_value;
                } else if (strcmp(field, "_annotations") == 0) {
                    _instance_klass_annotations_offset = field_offset_value;
                } else if (strcmp(field, "_inner_classes") == 0) {
                    _instance_klass_inner_classes_offset = field_offset_value;
                }
            }
            // Method structure
            else if (strcmp(type, "Method") == 0) {
                if (strcmp(field, "_constMethod") == 0) {
                    _method_constMethod_offset = field_offset_value;
                } else if (strcmp(field, "_size") == 0 || strcmp(field, "_method_size") == 0) {
                    _method_size_offset = field_offset_value;
                }
            }
            // ConstMethod structure
            else if (strcmp(type, "ConstMethod") == 0) {
                if (strcmp(field, "_size") == 0 || strcmp(field, "_constMethod_size") == 0) {
                    _constMethod_size_offset = field_offset_value;
                }
            }
            // ConstantPool structure
            else if (strcmp(type, "ConstantPool") == 0) {
                if (strcmp(field, "_length") == 0) {
                    _constantPool_length_offset = field_offset_value;
                } else if (strcmp(field, "_size") == 0) {
                    _constantPool_size_offset = field_offset_value;
                }
            }
        }
    }
}

void MetaspaceStructs::resolveKlassDetailOffsets() {
    _has_klass_details =
        (_instance_klass_methods_offset >= 0 && _instance_klass_constants_offset >= 0);
}

}  // namespace jvmtool
