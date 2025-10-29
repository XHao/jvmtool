#pragma once

#include <jni.h>
#include <jvmti.h>

#include <cstddef>

#include "metaspace/class_info.h"

namespace jvmtool {

/**
 * @brief Low-level accessor for Klass structure memory analysis
 *
 * This class encapsulates direct access to JVM internal Klass structures
 * using VMStructs. It provides precise measurement of various metadata
 * components.
 *
 * Responsibilities:
 * - Access InstanceKlass, Method, ConstantPool structures
 * - Calculate accurate sizes for metadata components
 * - Provide safe pointer dereferencing with boundary checks
 *
 * Design:
 * - All methods are static (utility class)
 * - Returns 0 for invalid/unsafe operations
 * - Uses SafeAccess for crash protection
 */
class KlassMemoryAccessor {
  public:
    /**
     * @brief Get native Klass* pointer from jclass
     *
     * @param jvmti JVMTI environment
     * @param env JNI environment
     * @param klass Java class reference
     * @return Klass* pointer or nullptr on failure
     */
    static void* getKlassPointer(jvmtiEnv* jvmti, JNIEnv* env, jclass klass);

    /**
     * @brief Get size of InstanceKlass structure
     *
     * Uses InstanceKlass::_size_helper for precise measurement.
     *
     * @param klass_ptr Klass* pointer (from getKlassPointer)
     * @return Size in bytes, or 0 if invalid
     */
    static size_t getKlassSize(void* klass_ptr);

    /**
     * @brief Calculate total size of all Method structures
     *
     * Iterates through _methods array and sums up:
     * - Method structure sizes
     * - Bytecode sizes (ConstMethod::_code_size)
     *
     * @param klass_ptr Klass* pointer
     * @return Total methods size in bytes, or 0 if invalid
     */
    static size_t getMethodsSize(void* klass_ptr);

    /**
     * @brief Calculate ConstantPool size
     *
     * Uses ConstantPool::_length to determine actual size.
     * Also accounts for ConstantPoolCache if present.
     *
     * @param klass_ptr Klass* pointer
     * @return ConstantPool size in bytes, or 0 if invalid
     */
    static size_t getConstantPoolSize(void* klass_ptr);

    /**
     * @brief Get virtual method table size
     *
     * Uses InstanceKlass::_vtable_len.
     *
     * @param klass_ptr Klass* pointer
     * @return Vtable size in bytes, or 0 if invalid
     */
    static size_t getVtableSize(void* klass_ptr);

    /**
     * @brief Get interface method table size
     *
     * Uses InstanceKlass::_itable_len.
     *
     * @param klass_ptr Klass* pointer
     * @return Itable size in bytes, or 0 if invalid
     */
    static size_t getItableSize(void* klass_ptr);

    /**
     * @brief Comprehensive class memory analysis
     *
     * Convenience method that calls all individual size methods
     * and populates a ClassMemoryInfo structure.
     *
     * @param jvmti JVMTI environment
     * @param env JNI environment
     * @param klass Java class reference
     * @return Populated ClassMemoryInfo structure
     */
    static ClassMemoryInfo analyzeClassMemory(jvmtiEnv* jvmti, JNIEnv* env, jclass klass);

  private:
    // Helper for getting class name from jclass
    static std::string getClassName(jvmtiEnv* jvmti, jclass klass);

    // Helper for getting class signature
    static std::string getClassSignature(jvmtiEnv* jvmti, jclass klass);

    // Helper for counting methods/fields via JVMTI
    static void getClassCounts(jvmtiEnv* jvmti, jclass klass, size_t& method_count,
                               size_t& field_count);
};

}  // namespace jvmtool
