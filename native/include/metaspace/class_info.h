#pragma once

#include <jni.h>

#include <cstddef>
#include <string>
#include <vector>

namespace jvmtool {

/**
 * @brief Detailed memory breakdown for a single class
 *
 * This structure provides fine-grained analysis of metaspace usage per class,
 * breaking down into different metadata categories.
 */
struct ClassMemoryInfo {
    std::string class_name;       // Full class name (e.g., "java.lang.String")
    std::string class_signature;  // JVM signature
    jclass class_ref;             // Global reference to jclass
    void* klass_address;          // Native Klass* pointer

    // Size breakdown (all in bytes)
    size_t total_size;          // Total metaspace for this class
    size_t klass_size;          // InstanceKlass structure size
    size_t methods_size;        // All Method structures
    size_t constant_pool_size;  // ConstantPool size
    size_t vtable_size;         // Virtual method table
    size_t itable_size;         // Interface method table
    size_t annotations_size;    // Annotations data
    size_t inner_classes_size;  // Inner class info

    // Derived information
    size_t method_count;     // Number of methods
    size_t field_count;      // Number of fields
    size_t interface_count;  // Number of implemented interfaces
    bool is_array_class;     // Whether this is an array type
    bool is_anonymous;       // Anonymous/hidden class

    ClassMemoryInfo()
        : class_ref(nullptr),
          klass_address(nullptr),
          total_size(0),
          klass_size(0),
          methods_size(0),
          constant_pool_size(0),
          vtable_size(0),
          itable_size(0),
          annotations_size(0),
          inner_classes_size(0),
          method_count(0),
          field_count(0),
          interface_count(0),
          is_array_class(false),
          is_anonymous(false) {}
};

/**
 * @brief Information about a single ClassLoader
 *
 * Contains both JVMTI-collected data and VMStructs-enhanced data
 * for a single ClassLoader instance.
 */
struct ClassLoaderInfo {
    std::string loader_name;               // e.g., "sun.misc.Launcher$AppClassLoader"
    std::string loader_type;               // Simple class name
    jobject loader_object;                 // Global reference to Java ClassLoader object (nullable)
    size_t class_count;                    // Number of classes loaded by this loader
    std::vector<std::string> class_names;  // Optional: list of loaded class names

    std::vector<ClassMemoryInfo> classes;  // Detailed info for each class

    void* cld_address;                // ClassLoaderData pointer
    void* metaspace_address;          // Metaspace pointer associated with this CLD
    size_t metaspace_used_bytes;      // Actual metaspace usage (from VMStructs)
    size_t metaspace_capacity_bytes;  // Committed metaspace capacity

    // Flags
    bool is_boot_loader;           // Bootstrap ClassLoader
    bool is_platform_loader;       // Platform/Extension ClassLoader
    bool has_vmstructs_data;       // Whether VMStructs data is available
    bool has_detailed_class_info;  // Whether per-class breakdown is available

    ClassLoaderInfo()
        : loader_object(nullptr),
          class_count(0),
          cld_address(nullptr),
          metaspace_address(nullptr),
          metaspace_used_bytes(0),
          metaspace_capacity_bytes(0),
          is_boot_loader(false),
          is_platform_loader(false),
          has_vmstructs_data(false),
          has_detailed_class_info(false) {}
};

/**
 * @brief Aggregated statistics for all ClassLoaders
 *
 * Provides summary information across all ClassLoaders in the JVM.
 */
struct ClassLoaderSummary {
    size_t total_loaders;
    size_t total_classes;
    size_t total_metaspace_used;
    size_t total_metaspace_committed;
    size_t boot_loader_classes;
    size_t platform_loader_classes;

    ClassLoaderSummary()
        : total_loaders(0),
          total_classes(0),
          total_metaspace_used(0),
          total_metaspace_committed(0),
          boot_loader_classes(0),
          platform_loader_classes(0) {}
};

}  // namespace jvmtool
