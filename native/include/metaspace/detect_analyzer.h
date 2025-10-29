#pragma once

#include <functional>
#include <map>
#include <string>
#include <vector>

#include "metaspace/class_info.h"

namespace jvmtool {

/**
 * @brief Result of pattern detection analysis
 *
 * Represents a detected pattern in the metaspace with statistics,
 * examples, and actionable recommendations.
 */
struct DetectionResult {
    std::string pattern_name;               // e.g., "CGLIB Proxy Leak"
    std::string pattern_type;               // e.g., "proxy", "lambda", "duplicate"
    std::string description;                // Human-readable description
    size_t match_count;                     // Number of classes matching this pattern
    size_t total_memory;                    // Total memory used by matching classes
    std::vector<ClassMemoryInfo> examples;  // Sample classes (top N)
    std::string recommendation;             // Suggested action

    enum class Severity { Low, Medium, High, Critical };
    Severity severity;

    DetectionResult() : match_count(0), total_memory(0), severity(Severity::Low) {}

    // Comparison operator for sorting by severity
    bool operator<(const DetectionResult& other) const {
        return severity < other.severity;
    }

    // Check if detection found any matches
    bool hasMatches() const {
        return match_count > 0;
    }
};

/**
 * @brief Metaspace pattern analyzer for detecting common issues
 *
 * This analyzer detects common Metaspace problems based on class naming patterns
 * and loading characteristics, such as:
 * - Proxy class leaks (JDK Dynamic Proxy, CGLIB, Javassist)
 * - Lambda expression accumulation
 * - Duplicate class loading
 * - Script engine class generation
 * - Anonymous inner class proliferation
 */
class DetectAnalyzer {
  public:
    /**
     * @brief Analyze all classes for known problem patterns
     *
     * @param loaders Vector of ClassLoaderInfo with detailed class info
     * @return Vector of detected patterns, sorted by severity
     */
    static std::vector<DetectionResult> analyze(const std::vector<ClassLoaderInfo>& loaders);

    /**
     * @brief Format pattern analysis report
     *
     * @param patterns Vector of detected patterns
     * @param show_examples Whether to show example classes
     * @return Formatted report string
     */
    static std::string formatReport(const std::vector<DetectionResult>& patterns,
                                    bool show_examples = true);

  private:
    /**
     * @brief Detect proxy class patterns
     *
     * Detects:
     * - JDK Dynamic Proxy: com.sun.proxy.$Proxy*
     * - CGLIB: *$$EnhancerByCGLIB$$*
     * - Javassist: *_$$_javassist_*
     * - ByteBuddy: *$ByteBuddy$*
     */
    static DetectionResult detectProxyClasses(const std::vector<ClassLoaderInfo>& loaders);

    /**
     * @brief Detect Lambda expression classes
     *
     * Detects: *$$Lambda$* pattern
     */
    static DetectionResult detectLambdaClasses(const std::vector<ClassLoaderInfo>& loaders);

    /**
     * @brief Detect duplicate class loading
     *
     * Finds classes with the same name loaded by different ClassLoaders
     */
    static DetectionResult detectDuplicateClasses(const std::vector<ClassLoaderInfo>& loaders);

    /**
     * @brief Detect script engine classes
     *
     * Detects:
     * - Groovy: groovy.*, org.codehaus.groovy.*
     * - JavaScript/Nashorn: jdk.nashorn.*, jdk.internal.dynalink.*
     * - JRuby: org.jruby.*
     * - Jython: org.python.*
     */
    static DetectionResult detectScriptClasses(const std::vector<ClassLoaderInfo>& loaders);

    /**
     * @brief Detect anonymous inner classes
     *
     * Detects: *$1, *$2, *$3, etc.
     */
    static DetectionResult detectAnonymousClasses(const std::vector<ClassLoaderInfo>& loaders);

    /**
     * @brief Detect reflection-heavy classes
     *
     * Classes with unusually high method/field counts
     */
    static DetectionResult detectReflectionHeavyClasses(
        const std::vector<ClassLoaderInfo>& loaders);
};

}  // namespace jvmtool
