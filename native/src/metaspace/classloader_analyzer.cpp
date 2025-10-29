// Merged implementation: strategies + analyzer
#include "metaspace/classloader_analyzer.h"

#include <algorithm>
#include <map>
#include <unordered_map>

#include "common.h"
#include "metaspace/klass_memory_accessor.h"
#include "metaspace/structs.h"
#include "vm/safeAccess.h"
#include "vm/vmStructs.h"

using jvmtool::MetaspaceStructs;

namespace jvmtool {

namespace {

/**
 * @brief Get ClassLoader name from jobject
 */
std::string getLoaderName(JNIEnv* env, jobject loader) {
    if (loader == nullptr) {
        return "bootstrap";
    }

    jclass loader_class = env->GetObjectClass(loader);
    if (loader_class == nullptr) {
        return "<unknown>";
    }

    jclass class_class = env->FindClass("java/lang/Class");
    jmethodID get_name_method = env->GetMethodID(class_class, "getName", "()Ljava/lang/String;");

    if (get_name_method == nullptr) {
        env->ExceptionClear();
        env->DeleteLocalRef(loader_class);
        env->DeleteLocalRef(class_class);
        return "<unknown>";
    }

    jstring name_obj = static_cast<jstring>(env->CallObjectMethod(loader_class, get_name_method));
    if (name_obj == nullptr || env->ExceptionCheck()) {
        env->ExceptionClear();
        env->DeleteLocalRef(loader_class);
        env->DeleteLocalRef(class_class);
        return "<unknown>";
    }

    const char* name_chars = env->GetStringUTFChars(name_obj, nullptr);
    std::string result(name_chars != nullptr ? name_chars : "<unknown>");
    if (name_chars != nullptr) {
        env->ReleaseStringUTFChars(name_obj, name_chars);
    }

    env->DeleteLocalRef(name_obj);
    env->DeleteLocalRef(loader_class);
    env->DeleteLocalRef(class_class);

    return result;
}

/**
 * @brief Get simple ClassLoader type name
 */
std::string getLoaderType(JNIEnv* env, jobject loader) {
    std::string full_name = getLoaderName(env, loader);
    size_t last_dot = full_name.find_last_of('.');
    if (last_dot != std::string::npos && last_dot + 1 < full_name.size()) {
        return full_name.substr(last_dot + 1);
    }
    return full_name;
}

/**
 * @brief Check if this is the bootstrap ClassLoader
 */
bool isBootstrapLoader(jobject loader) {
    return loader == nullptr;
}

/**
 * @brief Check if this is the platform/extension ClassLoader
 */
bool isPlatformLoader(JNIEnv* env, jobject loader) {
    if (loader == nullptr) {
        return false;
    }

    std::string name = getLoaderName(env, loader);
    return name.find("PlatformClassLoader") != std::string::npos ||
           name.find("ExtClassLoader") != std::string::npos;
}

}  // anonymous namespace

bool JVMTIAnalysisStrategy::analyze(jvmtiEnv* jvmti, JNIEnv* env,
                                    std::vector<ClassLoaderInfo>& loaders) const {
    if (jvmti == nullptr || env == nullptr) {
        return false;
    }

    // Get all loaded classes
    jint class_count = 0;
    jclass* classes = nullptr;
    jvmtiError err = jvmti->GetLoadedClasses(&class_count, &classes);

    if (err != JVMTI_ERROR_NONE || classes == nullptr) {
        return false;
    }

    // Group classes by ClassLoader
    std::map<jobject, std::vector<jclass>> loader_to_classes;

    for (jint i = 0; i < class_count; ++i) {
        jclass klass = classes[i];
        if (klass == nullptr)
            continue;

        jobject loader = nullptr;
        err = jvmti->GetClassLoader(klass, &loader);

        if (err != JVMTI_ERROR_NONE) {
            continue;
        }

        loader_to_classes[loader].push_back(klass);
    }

    // Build ClassLoaderInfo for each loader
    loaders.clear();
    loaders.reserve(loader_to_classes.size());

    for (const auto& [loader, klasses] : loader_to_classes) {
        ClassLoaderInfo info;
        info.loader_name = getLoaderName(env, loader);
        info.loader_type = getLoaderType(env, loader);
        info.loader_object = loader != nullptr ? env->NewGlobalRef(loader) : nullptr;
        info.class_count = klasses.size();
        info.is_boot_loader = isBootstrapLoader(loader);
        info.is_platform_loader = isPlatformLoader(env, loader);

        // Optionally collect class names
        if (include_class_names_) {
            info.class_names.reserve(klasses.size());
            for (jclass klass : klasses) {
                char* sig = nullptr;
                err = jvmti->GetClassSignature(klass, &sig, nullptr);
                if (err == JVMTI_ERROR_NONE && sig != nullptr) {
                    info.class_names.emplace_back(sig);
                    jvmti->Deallocate(reinterpret_cast<unsigned char*>(sig));
                }
            }
        }

        loaders.push_back(std::move(info));
    }

    jvmti->Deallocate(reinterpret_cast<unsigned char*>(classes));
    return true;
}

// ============================================================================
// VMStructsAnalysisStrategy Implementation
// ============================================================================

namespace {

/**
 * @brief Calculate metaspace usage for a ClassLoaderData
 */
size_t calculateMetaspaceUsage(void* cld_address) {
    if (cld_address == nullptr || !MetaspaceStructs::hasClassLoaderDataSupport()) {
        return 0;
    }

    const int metaspace_offset = MetaspaceStructs::cldMetaspaceOffset();
    if (metaspace_offset < 0) {
        return 0;
    }

    // Get the Metaspace pointer from ClassLoaderData
    void* metaspace_ptr =
        *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(cld_address) + metaspace_offset);

    if (metaspace_ptr == nullptr) {
        return 0;
    }

    // TODO: Implement Metaspace structure traversal to calculate actual usage
    // For now, return 0 to indicate we need more VMStructs information
    return 0;
}

}  // anonymous namespace

bool VMStructsAnalysisStrategy::analyze(jvmtiEnv* jvmti, JNIEnv* env,
                                        std::vector<ClassLoaderInfo>& loaders) const {
    if (!MetaspaceStructs::hasClassLoaderDataSupport()) {
        return false;
    }

    // Strategy: Traverse ClassLoaderDataGraph and match with our JVMTI-collected loaders
    void* current_cld = MetaspaceStructs::classLoaderDataGraphHead();
    const int next_offset = MetaspaceStructs::cldNextOffset();
    const int klasses_offset = MetaspaceStructs::cldKlassesOffset();
    const int metaspace_offset = MetaspaceStructs::cldMetaspaceOffset();

    if (current_cld == nullptr || next_offset < 0) {
        return false;
    }

    size_t cld_count = 0;
    const size_t max_iterations = 10000;  // Safety limit

    while (current_cld != nullptr && cld_count < max_iterations) {
        // For each CLD, count the classes it owns
        void* klass_list = nullptr;
        if (klasses_offset >= 0) {
            klass_list = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(current_cld) +
                                                   klasses_offset);
        }

        size_t class_count = 0;
        if (klass_list != nullptr) {
            void* current_klass = klass_list;
            const int next_link_offset = MetaspaceStructs::klassNextLinkOffset();
            const size_t max_klass_iterations = 100000;
            size_t klass_iterations = 0;

            while (current_klass != nullptr && klass_iterations < max_klass_iterations) {
                class_count++;
                if (next_link_offset >= 0) {
                    current_klass = *reinterpret_cast<void**>(
                        reinterpret_cast<uintptr_t>(current_klass) + next_link_offset);
                } else {
                    break;
                }
                klass_iterations++;
            }
        }

        // Try to match this CLD with a loader in our list by class count
        // This is a heuristic - ideally we'd match by Java object reference
        for (auto& loader : loaders) {
            if (!loader.has_vmstructs_data && loader.class_count == class_count) {
                loader.cld_address = current_cld;
                loader.has_vmstructs_data = true;

                // Get metaspace pointer
                if (metaspace_offset >= 0) {
                    loader.metaspace_address = *reinterpret_cast<void**>(
                        reinterpret_cast<uintptr_t>(current_cld) + metaspace_offset);
                }

                // Calculate metaspace usage
                loader.metaspace_used_bytes = calculateMetaspaceUsage(current_cld);

                // For now, use a rough estimate based on class count if VMStructs
                // doesn't provide the actual value
                if (loader.metaspace_used_bytes == 0 && class_count > 0) {
                    // Rough estimate: ~1-2 KB per class for metadata
                    loader.metaspace_used_bytes = class_count * 1536;
                }

                break;  // Move to next CLD
            }
        }

        // Move to next ClassLoaderData
        if (next_offset >= 0) {
            current_cld =
                *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(current_cld) + next_offset);
        } else {
            break;
        }
        cld_count++;
    }

    // Return true if we successfully enhanced at least some loaders
    bool any_enhanced = false;
    for (const auto& loader : loaders) {
        if (loader.has_vmstructs_data) {
            any_enhanced = true;
            break;
        }
    }

    return any_enhanced;
}

// ============================================================================
// ClassDetailAnalysisStrategy Implementation
// ============================================================================

bool ClassDetailAnalysisStrategy::analyze(jvmtiEnv* jvmti, JNIEnv* env,
                                          std::vector<ClassLoaderInfo>& loaders) const {
    if (jvmti == nullptr || env == nullptr) {
        return false;
    }

    // Get all loaded classes
    jint class_count = 0;
    jclass* classes = nullptr;
    jvmtiError err = jvmti->GetLoadedClasses(&class_count, &classes);

    if (err != JVMTI_ERROR_NONE || classes == nullptr) {
        return false;
    }

    // Group classes by ClassLoader (match with existing loaders)
    std::unordered_map<jobject, std::vector<jclass>> loader_to_classes;

    for (jint i = 0; i < class_count; ++i) {
        jclass klass = classes[i];
        if (klass == nullptr)
            continue;

        jobject loader = nullptr;
        err = jvmti->GetClassLoader(klass, &loader);

        if (err != JVMTI_ERROR_NONE) {
            continue;
        }

        loader_to_classes[loader].push_back(klass);
    }

    // Analyze classes for each loader
    for (auto& loader : loaders) {
        jobject loader_obj = loader.loader_object;

        // Find matching classes
        auto it = loader_to_classes.find(loader_obj);
        if (it == loader_to_classes.end()) {
            continue;
        }

        const auto& klasses = it->second;
        loader.classes.reserve(klasses.size());

        size_t classes_analyzed = 0;
        const size_t max_classes =
            (max_classes_per_loader_ == 0) ? klasses.size() : max_classes_per_loader_;

        for (jclass klass : klasses) {
            if (classes_analyzed >= max_classes) {
                break;
            }

            // Analyze this class using KlassMemoryAccessor
            ClassMemoryInfo class_info = KlassMemoryAccessor::analyzeClassMemory(jvmti, env, klass);
            loader.classes.push_back(std::move(class_info));

            classes_analyzed++;
        }

        // Sort classes by memory usage (descending)
        std::sort(loader.classes.begin(), loader.classes.end(),
                  [](const ClassMemoryInfo& a, const ClassMemoryInfo& b) {
                      return a.total_size > b.total_size;
                  });
    }

    jvmti->Deallocate(reinterpret_cast<unsigned char*>(classes));
    return true;
}

std::vector<ClassLoaderInfo> ClassLoaderAnalyzer::analyze(jvmtiEnv* jvmti, JNIEnv* env,
                                                          const ClassLoaderAnalysisOptions& options,
                                                          ClassLoaderSummary* summary) {
    std::vector<std::shared_ptr<ClassLoaderAnalysisStrategy>> strategies;

    strategies = buildStrategies(options);

    // Execute strategies
    auto loaders = analyzeWithStrategies(jvmti, env, strategies, nullptr);

    // Post-process: sort and filter
    sortAndFilter(loaders, options);

    // Calculate summary after sorting/filtering
    if (summary != nullptr) {
        calculateSummary(loaders, *summary);
    }

    return loaders;
}

std::vector<ClassLoaderInfo> ClassLoaderAnalyzer::analyzeWithStrategies(
    jvmtiEnv* jvmti, JNIEnv* env,
    const std::vector<std::shared_ptr<ClassLoaderAnalysisStrategy>>& strategies,
    ClassLoaderSummary* summary) {
    std::vector<ClassLoaderInfo> loaders;

    // Execute each strategy in sequence
    for (const auto& strategy : strategies) {
        if (strategy) {
            strategy->analyze(jvmti, env, loaders);
        }
    }

    // Note: We don't sort here because ClassLoaderAnalysisOptions has sorting config
    // Sorting will be done in the main analyze() method

    // Calculate summary if requested
    if (summary != nullptr) {
        calculateSummary(loaders, *summary);
    }

    return loaders;
}

std::vector<std::shared_ptr<ClassLoaderAnalysisStrategy>> ClassLoaderAnalyzer::buildStrategies(
    const ClassLoaderAnalysisOptions& options) {
    std::vector<std::shared_ptr<ClassLoaderAnalysisStrategy>> strategies;

    // 1. Always start with JVMTI to collect basic info
    strategies.push_back(std::make_shared<JVMTIAnalysisStrategy>(options.include_class_names));

    // 2. Optionally enhance with VMStructs
    if (options.use_vmstructs) {
        strategies.push_back(std::make_shared<VMStructsAnalysisStrategy>());
    }

    // 3. Optionally add detailed class-level analysis
    if (options.include_class_details) {
        strategies.push_back(
            std::make_shared<ClassDetailAnalysisStrategy>(options.max_classes_per_loader));
    }

    return strategies;
}

void ClassLoaderAnalyzer::sortAndFilter(std::vector<ClassLoaderInfo>& loaders,
                                        const ClassLoaderAnalysisOptions& options) {
    // Sort based on specified criteria
    switch (options.sort_by) {
        case ClassLoaderAnalysisOptions::SortBy::MEMORY:
            std::sort(loaders.begin(), loaders.end(),
                      [](const ClassLoaderInfo& a, const ClassLoaderInfo& b) {
                          return a.metaspace_used_bytes > b.metaspace_used_bytes;
                      });
            break;

        case ClassLoaderAnalysisOptions::SortBy::CLASS_COUNT:
            std::sort(loaders.begin(), loaders.end(),
                      [](const ClassLoaderInfo& a, const ClassLoaderInfo& b) {
                          return a.class_count > b.class_count;
                      });
            break;

        case ClassLoaderAnalysisOptions::SortBy::NAME:
            std::sort(loaders.begin(), loaders.end(),
                      [](const ClassLoaderInfo& a, const ClassLoaderInfo& b) {
                          return a.loader_name < b.loader_name;
                      });
            break;
    }

    // Apply top_n filter if specified
    if (options.top_n > 0 && loaders.size() > options.top_n) {
        loaders.resize(options.top_n);
    }
}

void ClassLoaderAnalyzer::calculateSummary(const std::vector<ClassLoaderInfo>& loaders,
                                           ClassLoaderSummary& summary) {
    summary = ClassLoaderSummary();  // Reset

    for (const auto& loader : loaders) {
        summary.total_loaders++;
        summary.total_classes += loader.class_count;
        summary.total_metaspace_used += loader.metaspace_used_bytes;
        summary.total_metaspace_committed += loader.metaspace_capacity_bytes;

        if (loader.is_boot_loader) {
            summary.boot_loader_classes = loader.class_count;
        } else if (loader.is_platform_loader) {
            summary.platform_loader_classes += loader.class_count;
        }
    }
}

}  // namespace jvmtool
