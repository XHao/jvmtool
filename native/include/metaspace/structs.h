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

    static void initMetaspaceOffsets();
    static void resolveMetaspaceOffsets();

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
};

}  // namespace jvmtool
