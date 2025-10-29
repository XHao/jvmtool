#include "metaspace/klass_memory_accessor.h"

#include "common.h"
#include "metaspace/structs.h"
#include "vm/safeAccess.h"
#include "vm/vmStructs.h"

namespace jvmtool {

// ============================================================================
// Public Methods
// ============================================================================

void* KlassMemoryAccessor::getKlassPointer(jvmtiEnv* jvmti, JNIEnv* env, jclass klass) {
    // This is a placeholder implementation
    // In reality, we need to use JVMTI or internal APIs to get the Klass* pointer
    // One approach is to use the fact that jclass is a jobject which internally
    // points to a java.lang.Class instance, and we can read its internal field

    (void)jvmti;
    (void)env;
    (void)klass;

    // TODO: Implement actual Klass* extraction
    // This requires either:
    // 1. Using JVMTI GetTag/SetTag to associate data
    // 2. Reading internal fields of java.lang.Class
    // 3. Using unsafe operations

    return nullptr;
}

size_t KlassMemoryAccessor::getKlassSize(void* klass_ptr) {
    if (klass_ptr == nullptr || !MetaspaceStructs::hasKlassDetails()) {
        return 0;
    }

    // Try to get precise size from VMStructs using _size_helper field
    VMKlass* klass = reinterpret_cast<VMKlass*>(klass_ptr);
    const int size_in_words = klass->instanceKlassSizeInWords();

    if (size_in_words > 0) {
        // _size_helper contains size in words (8 bytes each on 64-bit)
        return size_in_words * 8;
    }

    // Fallback to estimate if VMStructs unavailable
    return 512;
}

size_t KlassMemoryAccessor::getMethodsSize(void* klass_ptr) {
    if (klass_ptr == nullptr || !MetaspaceStructs::hasKlassDetails()) {
        return 0;
    }

    VMKlass* klass = reinterpret_cast<VMKlass*>(klass_ptr);

    // Get methods array using VMKlass wrapper (returns Array<Method*>*)
    const void* methods_array_ptr = klass->methodsArray();
    if (methods_array_ptr == nullptr) {
        return 0;
    }

    // Safely read method count from Array header
    int32_t* count_ptr = const_cast<int32_t*>(reinterpret_cast<const int32_t*>(methods_array_ptr));
    const int method_count = SafeAccess::load32(count_ptr, 0);

    // Sanity check: method count should be reasonable
    if (method_count < 0 || method_count > MAX_REASONABLE_METHOD_COUNT) {
        // LOG: Unreasonable method count, falling back to estimation
        return 0;
    }

    // Try to get precise size by examining ConstMethod structures
    size_t total_size = 0;
    const int const_method_offset = MetaspaceStructs::methodConstMethodOffset();
    const int code_size_offset = VMStructs::constMethodCodeSizeOffset();

    if (const_method_offset >= 0 && code_size_offset >= 0) {
        // Iterate through methods to get precise sizes
        for (int i = 0; i < method_count; i++) {
            // Get Method* pointer from array (skip 4-byte length header + 4-byte padding)
            void** method_slot_ptr = reinterpret_cast<void**>(
                const_cast<char*>(reinterpret_cast<const char*>(methods_array_ptr) + 8 + i * 8));
            const void* method_ptr = SafeAccess::load(method_slot_ptr, nullptr);
            if (method_ptr == nullptr) {
                continue;
            }

            // Get ConstMethod* from Method
            void** const_method_slot_ptr = reinterpret_cast<void**>(
                const_cast<char*>(reinterpret_cast<const char*>(method_ptr) + const_method_offset));
            const void* const_method_ptr = SafeAccess::load(const_method_slot_ptr, nullptr);
            if (const_method_ptr == nullptr) {
                continue;
            }

            // Get bytecode size from ConstMethod
            int32_t* code_size_ptr = const_cast<int32_t*>(reinterpret_cast<const int32_t*>(
                reinterpret_cast<const char*>(const_method_ptr) + code_size_offset));
            const int code_size = SafeAccess::load32(code_size_ptr, 0);

            // Sanity check: code size should be reasonable
            if (code_size < 0 || code_size > MAX_REASONABLE_CODE_SIZE) {
                continue;
            }

            // Method structure: ~40-80 bytes
            // ConstMethod structure: base (~100 bytes) + bytecode
            total_size += 60;               // Method struct average
            total_size += 100 + code_size;  // ConstMethod + bytecode
        }

        return total_size;
    }

    // Fallback to estimation
    return method_count * 150;  // Rough estimate per method
}

size_t KlassMemoryAccessor::getConstantPoolSize(void* klass_ptr) {
    if (klass_ptr == nullptr || !MetaspaceStructs::hasKlassDetails()) {
        return 0;
    }

    VMKlass* klass = reinterpret_cast<VMKlass*>(klass_ptr);

    // Get ConstantPool* using VMKlass method (via Method→ConstMethod→ConstantPool)
    const void* cp_ptr = klass->constantPool();
    if (cp_ptr == nullptr) {
        return 256;  // Fallback estimate
    }

    // Safely read constant pool length
    const int length_offset = MetaspaceStructs::constantPoolLengthOffset();
    if (length_offset >= 0) {
        int32_t* length_ptr = const_cast<int32_t*>(reinterpret_cast<const int32_t*>(
            reinterpret_cast<const char*>(cp_ptr) + length_offset));
        const int length = SafeAccess::load32(length_ptr, 0);

        // Sanity check: constant pool length should be reasonable
        if (length > 0 && length <= MAX_REASONABLE_CP_LENGTH) {
            // ConstantPool base structure + entries
            // Base: ~64 bytes overhead
            // Each entry: 8 bytes (on 64-bit) or varies by type
            size_t base_size = 64;
            size_t entries_size = length * 8;

            // Check if ConstantPoolCache exists
            const int cache_offset = VMStructs::constantPoolCacheOffset();
            if (cache_offset >= 0) {
                void** cache_slot_ptr = reinterpret_cast<void**>(
                    const_cast<char*>(reinterpret_cast<const char*>(cp_ptr) + cache_offset));
                const void* cache_ptr = SafeAccess::load(cache_slot_ptr, nullptr);
                if (cache_ptr != nullptr) {
                    // ConstantPoolCache: rough estimate based on pool size
                    entries_size += length * 4;  // Cache entries are smaller
                }
            }

            return base_size + entries_size;
        }
    }

    return 256;  // Default estimate
}

size_t KlassMemoryAccessor::getVtableSize(void* klass_ptr) {
    if (klass_ptr == nullptr || !MetaspaceStructs::hasKlassDetails()) {
        return 0;
    }

    // Use VMKlass wrapper to safely read vtable length
    VMKlass* klass = reinterpret_cast<VMKlass*>(klass_ptr);
    const int vtable_len = klass->vtableLength();

    if (vtable_len >= 0) {
        // Each vtable entry is a pointer (8 bytes on 64-bit)
        return vtable_len * 8;
    }

    return 0;  // Unable to determine
}

size_t KlassMemoryAccessor::getItableSize(void* klass_ptr) {
    if (klass_ptr == nullptr || !MetaspaceStructs::hasKlassDetails()) {
        return 0;
    }

    // Use VMKlass wrapper to safely read itable length
    VMKlass* klass = reinterpret_cast<VMKlass*>(klass_ptr);
    const int itable_len = klass->itableLength();

    if (itable_len >= 0) {
        // Each itable entry includes interface Klass* and offset table
        return itable_len * 16;  // Rough estimate
    }

    return 0;  // Unable to determine
}

ClassMemoryInfo KlassMemoryAccessor::analyzeClassMemory(jvmtiEnv* jvmti, JNIEnv* env,
                                                        jclass klass) {
    ClassMemoryInfo info;

    // Get class name and signature
    info.class_name = getClassName(jvmti, klass);
    info.class_signature = getClassSignature(jvmti, klass);

    // Create global reference
    info.class_ref = static_cast<jclass>(env->NewGlobalRef(klass));

    // Get method/field counts
    getClassCounts(jvmti, klass, info.method_count, info.field_count);

    // Get implemented interfaces count
    jint interface_count = 0;
    jclass* interfaces = nullptr;
    if (jvmti->GetImplementedInterfaces(klass, &interface_count, &interfaces) == JVMTI_ERROR_NONE) {
        info.interface_count = interface_count;
        if (interfaces != nullptr) {
            for (jint i = 0; i < interface_count; ++i) {
                env->DeleteLocalRef(interfaces[i]);
            }
            jvmti->Deallocate(reinterpret_cast<unsigned char*>(interfaces));
        }
    }

    // Check if it's an array class
    jboolean is_array = JNI_FALSE;
    if (jvmti->IsArrayClass(klass, &is_array) == JVMTI_ERROR_NONE) {
        info.is_array_class = (is_array == JNI_TRUE);
    }

    // Try to get Klass pointer for detailed analysis
    void* klass_ptr = getKlassPointer(jvmti, env, klass);
    info.klass_address = klass_ptr;

    if (klass_ptr != nullptr && MetaspaceStructs::hasKlassDetails()) {
        // Calculate detailed sizes using VMStructs
        info.klass_size = getKlassSize(klass_ptr);
        info.methods_size = getMethodsSize(klass_ptr);
        info.constant_pool_size = getConstantPoolSize(klass_ptr);
        info.vtable_size = getVtableSize(klass_ptr);
        info.itable_size = getItableSize(klass_ptr);
    } else {
        // Use JVMTI-based estimates
        // Base InstanceKlass structure
        info.klass_size = 512;

        // Methods: Method + ConstMethod structures
        info.methods_size = info.method_count * 150;

        // Constant pool: rough estimate based on class complexity
        info.constant_pool_size = 128 + info.method_count * 32 + info.field_count * 16;

        // Vtables: typically one entry per overridable method
        info.vtable_size = info.method_count * 8;

        // Itables: one entry per interface method
        info.itable_size = info.interface_count * 16;

        // Annotations: estimated based on method/field count
        info.annotations_size = (info.method_count + info.field_count) * 8;
    }

    // Calculate total
    info.total_size = info.klass_size + info.methods_size + info.constant_pool_size +
                      info.vtable_size + info.itable_size + info.annotations_size +
                      info.inner_classes_size;

    return info;
}

// ============================================================================
// Private Helper Methods
// ============================================================================

std::string KlassMemoryAccessor::getClassName(jvmtiEnv* jvmti, jclass klass) {
    char* signature = nullptr;
    if (jvmti->GetClassSignature(klass, &signature, nullptr) == JVMTI_ERROR_NONE &&
        signature != nullptr) {
        std::string name(signature);
        jvmti->Deallocate(reinterpret_cast<unsigned char*>(signature));
        return name;
    }
    return "<unknown>";
}

std::string KlassMemoryAccessor::getClassSignature(jvmtiEnv* jvmti, jclass klass) {
    char* signature = nullptr;
    char* generic = nullptr;
    if (jvmti->GetClassSignature(klass, &signature, &generic) == JVMTI_ERROR_NONE) {
        std::string result;
        if (signature != nullptr) {
            result = signature;
            jvmti->Deallocate(reinterpret_cast<unsigned char*>(signature));
        }
        if (generic != nullptr) {
            jvmti->Deallocate(reinterpret_cast<unsigned char*>(generic));
        }
        return result;
    }
    return "";
}

void KlassMemoryAccessor::getClassCounts(jvmtiEnv* jvmti, jclass klass, size_t& method_count,
                                         size_t& field_count) {
    // Get method count
    jint mcount = 0;
    jmethodID* methods = nullptr;
    if (jvmti->GetClassMethods(klass, &mcount, &methods) == JVMTI_ERROR_NONE) {
        method_count = mcount;
        if (methods != nullptr) {
            jvmti->Deallocate(reinterpret_cast<unsigned char*>(methods));
        }
    }

    // Get field count
    jint fcount = 0;
    jfieldID* fields = nullptr;
    if (jvmti->GetClassFields(klass, &fcount, &fields) == JVMTI_ERROR_NONE) {
        field_count = fcount;
        if (fields != nullptr) {
            jvmti->Deallocate(reinterpret_cast<unsigned char*>(fields));
        }
    }
}

}  // namespace jvmtool
