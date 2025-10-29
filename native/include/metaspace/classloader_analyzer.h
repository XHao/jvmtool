#pragma once

#include <jvmti.h>

#include <memory>
#include <string>
#include <vector>

#include "metaspace/class_info.h"

namespace jvmtool {
/**
 * @brief Strategy interface for ClassLoader analysis
 *
 * Different strategies can be implemented to enhance ClassLoaderInfo:
 * - JVMTIAnalysisStrategy: Use JVMTI API for cross-version compatibility
 * - VMStructsAnalysisStrategy: Use VMStructs for precise memory measurement
 * - ClassDetailAnalysisStrategy: Detailed per-class memory breakdown
 */
class ClassLoaderAnalysisStrategy {
  public:
    virtual ~ClassLoaderAnalysisStrategy() = default;

    /**
     * @brief Analyze and populate ClassLoaderInfo
     *
     * @param jvmti JVMTI environment
     * @param env JNI environment
     * @param loaders Input/Output vector of ClassLoaderInfo to populate/enhance
     * @return true if analysis succeeded, false otherwise
     */
    virtual bool analyze(jvmtiEnv* jvmti, JNIEnv* env,
                         std::vector<ClassLoaderInfo>& loaders) const = 0;

    /**
     * @brief Get strategy name for debugging/logging
     */
    virtual std::string name() const = 0;
};

/**
 * @brief JVMTI-based analysis strategy
 *
 * Collects basic ClassLoader information using standard JVMTI API:
 * - ClassLoader objects and their loaded classes
 * - Class names and counts
 * - Loader types (bootstrap, platform, application)
 */
class JVMTIAnalysisStrategy : public ClassLoaderAnalysisStrategy {
  public:
    explicit JVMTIAnalysisStrategy(bool include_class_names = false)
        : include_class_names_(include_class_names) {}

    bool analyze(jvmtiEnv* jvmti, JNIEnv* env,
                 std::vector<ClassLoaderInfo>& loaders) const override;

    std::string name() const override {
        return "JVMTI";
    }

  private:
    bool include_class_names_;
};

/**
 * @brief VMStructs-based enhancement strategy
 *
 * Enhances existing ClassLoaderInfo with precise memory data from VMStructs:
 * - ClassLoaderData addresses
 * - Exact metaspace usage and capacity
 * - Native pointer information
 */
class VMStructsAnalysisStrategy : public ClassLoaderAnalysisStrategy {
  public:
    bool analyze(jvmtiEnv* jvmti, JNIEnv* env,
                 std::vector<ClassLoaderInfo>& loaders) const override;

    std::string name() const override {
        return "VMStructs";
    }
};

/**
 * @brief Detailed class-level memory analysis strategy
 *
 * Provides fine-grained per-class memory breakdown:
 * - InstanceKlass size
 * - Method metadata (bytecode, method structures)
 * - ConstantPool size
 * - Vtable/Itable sizes
 */
class ClassDetailAnalysisStrategy : public ClassLoaderAnalysisStrategy {
  public:
    explicit ClassDetailAnalysisStrategy(size_t max_classes_per_loader = 0)
        : max_classes_per_loader_(max_classes_per_loader) {}

    bool analyze(jvmtiEnv* jvmti, JNIEnv* env,
                 std::vector<ClassLoaderInfo>& loaders) const override;

    std::string name() const override {
        return "ClassDetail";
    }

  private:
    size_t max_classes_per_loader_;
};

/**
 * @brief Configuration options for ClassLoader analysis
 */
struct ClassLoaderAnalysisOptions {
    bool include_class_names = false;    // Collect individual class names (verbose)
    bool use_vmstructs = true;           // Use VMStructs for precise measurement
    bool include_class_details = false;  // Perform detailed per-class analysis
    size_t max_classes_per_loader = 0;   // Limit classes to analyze per loader (0 = all)

    // Sorting and filtering
    enum class SortBy { MEMORY, CLASS_COUNT, NAME };
    SortBy sort_by = SortBy::MEMORY;
    size_t top_n = 0;  // Limit result count (0 = all)

    ClassLoaderAnalysisOptions() = default;

    // Convenience factory methods
    static ClassLoaderAnalysisOptions basic() {
        ClassLoaderAnalysisOptions opts;
        opts.use_vmstructs = false;
        opts.include_class_details = false;
        return opts;
    }

    static ClassLoaderAnalysisOptions detailed() {
        ClassLoaderAnalysisOptions opts;
        opts.include_class_names = true;
        opts.use_vmstructs = true;
        opts.include_class_details = true;
        return opts;
    }
};

/**
 * @brief Facade for ClassLoader metaspace analysis
 *
 * This class provides a simplified, extensible interface for analyzing
 * ClassLoader memory usage. It uses the Strategy pattern internally to
 * support different analysis approaches (JVMTI, VMStructs, custom).
 *
 * Design principles:
 * - Simple public interface (2 main methods)
 * - Strategy pattern for extensibility
 * - Separation of analysis and formatting
 * - Builder pattern via options object
 *
 * Usage:
 *   // Simple analysis
 *   auto loaders = ClassLoaderAnalyzer::analyze(jvmti, env);
 *
 *   // Detailed analysis
 *   auto opts = ClassLoaderAnalysisOptions::detailed();
 *   auto loaders = ClassLoaderAnalyzer::analyze(jvmti, env, opts);
 *
 *   // Format results
 *   auto report = ClassLoaderReportFormatter::formatSummary(loaders, summary);
 */
class ClassLoaderAnalyzer {
  public:
    /**
     * @brief Main entry point for ClassLoader analysis
     *
     * This method orchestrates the analysis pipeline:
     * 1. Execute configured strategies (JVMTI, VMStructs, ClassDetail)
     * 2. Sort and filter results
     * 3. Calculate summary statistics
     *
     * @param jvmti JVMTI environment
     * @param env JNI environment
     * @param options Analysis configuration
     * @param summary Output summary statistics (optional)
     * @return Vector of ClassLoaderInfo sorted and filtered per options
     */
    static std::vector<ClassLoaderInfo> analyze(
        jvmtiEnv* jvmti, JNIEnv* env,
        const ClassLoaderAnalysisOptions& options = ClassLoaderAnalysisOptions(),
        ClassLoaderSummary* summary = nullptr);

  private:
    // Internal glue layer - composes strategies based on options
    static std::vector<std::shared_ptr<ClassLoaderAnalysisStrategy>> buildStrategies(
        const ClassLoaderAnalysisOptions& options);

    /**
     * @brief Analyze with custom strategies (advanced usage)
     *
     * Allows users to provide their own analysis strategies for
     * specialized analysis or testing purposes.
     *
     * @param jvmti JVMTI environment
     * @param env JNI environment
     * @param strategies Vector of custom strategies to execute
     * @param summary Output summary statistics (optional)
     * @return Vector of ClassLoaderInfo
     */
    static std::vector<ClassLoaderInfo> analyzeWithStrategies(
        jvmtiEnv* jvmti, JNIEnv* env,
        const std::vector<std::shared_ptr<ClassLoaderAnalysisStrategy>>& strategies,
        ClassLoaderSummary* summary = nullptr);

    // Post-processing
    static void sortAndFilter(std::vector<ClassLoaderInfo>& loaders,
                              const ClassLoaderAnalysisOptions& options);
    static void calculateSummary(const std::vector<ClassLoaderInfo>& loaders,
                                 ClassLoaderSummary& summary);
};

}  // namespace jvmtool
