#pragma once

#include "vm/vmStructs.h"

namespace jvmtool {

class MetaspaceStructs : public VMStructs {
  protected:
    static bool _has_metaspace_structs;

    static void** _shared_metaspace_base_addr;
    static void** _shared_metaspace_top_addr;
    static void* _shared_metaspace_base;
    static void* _shared_metaspace_top;

    static size_t* _metaspace_capacity_until_gc_addr;
    static size_t* _metaspace_used_words_addr;
    static size_t* _metaspace_committed_words_addr;
    static size_t _metaspace_capacity_until_gc_words;
    static size_t _metaspace_used_words;
    static size_t _metaspace_committed_words;

    static void** _compressed_class_space_virtual_space_addr;
    static void* _compressed_class_space_virtual_space;
    static void* _compressed_class_space_low;
    static void* _compressed_class_space_high;
    static void* _compressed_class_space_low_boundary;
    static void* _compressed_class_space_high_boundary;

    static int _virtual_space_low_offset;
    static int _virtual_space_high_offset;
    static int _virtual_space_low_boundary_offset;
    static int _virtual_space_high_boundary_offset;

    static bool _has_cld_support;
    static void** _class_loader_data_graph_head_addr;
    static void* _class_loader_data_graph_head;
    static int _cld_next_offset;
    static int _cld_klasses_offset;
    static int _cld_metaspace_offset;
    static int _cld_class_loader_offset;
    static int _klass_next_link_offset;
    static int _klass_class_loader_data_offset;

    static bool _has_klass_details;
    static int _klass_layout_helper_offset;
    static int _klass_super_offset;
    static int _klass_java_mirror_offset;
    static int _klass_name_offset;
    static int _instance_klass_methods_offset;
    static int _instance_klass_constants_offset;
    static int _instance_klass_fields_offset;
    static int _instance_klass_vtable_len_offset;
    static int _instance_klass_itable_len_offset;
    static int _instance_klass_annotations_offset;
    static int _instance_klass_inner_classes_offset;
    static int _method_size_offset;
    static int _method_constMethod_offset;
    static int _constMethod_size_offset;
    static int _constantPool_length_offset;
    static int _constantPool_size_offset;

    static void initMetaspaceOffsets();
    static void resolveMetaspaceOffsets();
    static void initClassLoaderDataOffsets();
    static void resolveClassLoaderDataOffsets();
    static void initKlassDetailOffsets();
    static void resolveKlassDetailOffsets();

  public:
    static void init(CodeCache* libjvm);
    static void ready();

    static bool hasMetaspaceStructs() {
        return _has_metaspace_structs;
    }

    static void* sharedMetaspaceBase() {
        return _shared_metaspace_base;
    }

    static void* sharedMetaspaceTop() {
        return _shared_metaspace_top;
    }

    static size_t capacityUntilGCWords() {
        return _metaspace_capacity_until_gc_words;
    }

    static size_t committedWords() {
        return _metaspace_committed_words;
    }

    static size_t usedWords() {
        return _metaspace_used_words;
    }

    static void* compressedClassSpaceLow() {
        return _compressed_class_space_low;
    }

    static void* compressedClassSpaceHigh() {
        return _compressed_class_space_high;
    }

    static void* compressedClassSpaceLowBoundary() {
        return _compressed_class_space_low_boundary;
    }

    static void* compressedClassSpaceHighBoundary() {
        return _compressed_class_space_high_boundary;
    }

    static bool isSharedMetaspaceObject(const void* obj) {
        if (_shared_metaspace_base == nullptr || _shared_metaspace_top == nullptr) {
            return false;
        }
        return obj >= _shared_metaspace_base && obj < _shared_metaspace_top;
    }

    static size_t sharedMetaspaceSize() {
        if (_shared_metaspace_base == nullptr || _shared_metaspace_top == nullptr) {
            return 0;
        }
        return static_cast<char*>(_shared_metaspace_top) -
               static_cast<char*>(_shared_metaspace_base);
    }

    static bool hasSharedMetaspace() {
        return _shared_metaspace_base != nullptr && _shared_metaspace_top != nullptr;
    }

    // ClassLoaderData traversal support
    static bool hasClassLoaderDataSupport() {
        return _has_cld_support;
    }

    static void* classLoaderDataGraphHead() {
        return _class_loader_data_graph_head;
    }

    static int cldNextOffset() {
        return _cld_next_offset;
    }

    static int cldKlassesOffset() {
        return _cld_klasses_offset;
    }

    static int cldMetaspaceOffset() {
        return _cld_metaspace_offset;
    }

    static int cldClassLoaderOffset() {
        return _cld_class_loader_offset;
    }

    static int klassNextLinkOffset() {
        return _klass_next_link_offset;
    }

    static int klassClassLoaderDataOffset() {
        return _klass_class_loader_data_offset;
    }

    static bool hasKlassDetails() {
        return _has_klass_details;
    }

    static int klassLayoutHelperOffset() {
        return _klass_layout_helper_offset;
    }

    static int klassSuperOffset() {
        return _klass_super_offset;
    }

    static int klassJavaMirrorOffset() {
        return _klass_java_mirror_offset;
    }

    static int klassNameOffset() {
        return _klass_name_offset;
    }

    static int instanceKlassMethodsOffset() {
        return _instance_klass_methods_offset;
    }

    static int instanceKlassConstantsOffset() {
        return _instance_klass_constants_offset;
    }

    static int instanceKlassFieldsOffset() {
        return _instance_klass_fields_offset;
    }

    static int instanceKlassVtableLenOffset() {
        return _instance_klass_vtable_len_offset;
    }

    static int instanceKlassItableLenOffset() {
        return _instance_klass_itable_len_offset;
    }

    static int instanceKlassAnnotationsOffset() {
        return _instance_klass_annotations_offset;
    }

    static int methodSizeOffset() {
        return _method_size_offset;
    }

    static int methodConstMethodOffset() {
        return _method_constMethod_offset;
    }

    static int constMethodSizeOffset() {
        return _constMethod_size_offset;
    }

    static int constantPoolLengthOffset() {
        return _constantPool_length_offset;
    }

    static int constantPoolSizeOffset() {
        return _constantPool_size_offset;
    }
};

}  // namespace jvmtool
