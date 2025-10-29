#include "metaspace/detect_analyzer.h"

#include <algorithm>
#include <iomanip>
#include <map>
#include <regex>
#include <set>
#include <sstream>

#include "common.h"

namespace jvmtool {

namespace {
bool matchesProxyPattern(const std::string& class_name) {
    // JDK Dynamic Proxy: com.sun.proxy.$Proxy123
    if (class_name.find("$Proxy") != std::string::npos) {
        return true;
    }

    // CGLIB: com.example.Service$$EnhancerByCGLIB$$abc123
    if (class_name.find("$$EnhancerByCGLIB$$") != std::string::npos ||
        class_name.find("$$FastClassByCGLIB$$") != std::string::npos) {
        return true;
    }

    // Javassist: com.example.Service_$$_javassist_123
    if (class_name.find("_$$_javassist_") != std::string::npos) {
        return true;
    }

    // ByteBuddy: com.example.Service$ByteBuddy$abc
    if (class_name.find("$ByteBuddy$") != std::string::npos) {
        return true;
    }

    return false;
}

bool matchesLambdaPattern(const std::string& class_name) {
    // Lambda: com.example.Service$$Lambda$123/0x00007f1234567890
    return class_name.find("$$Lambda$") != std::string::npos;
}

bool matchesScriptPattern(const std::string& class_name) {
    // Groovy
    if (class_name.find("groovy.") == 0 || class_name.find("org.codehaus.groovy.") == 0) {
        return true;
    }

    // JavaScript/Nashorn
    if (class_name.find("jdk.nashorn.") == 0 || class_name.find("jdk.internal.dynalink.") == 0) {
        return true;
    }

    // JRuby
    if (class_name.find("org.jruby.") == 0) {
        return true;
    }

    // Jython
    if (class_name.find("org.python.") == 0) {
        return true;
    }

    return false;
}

bool matchesAnonymousPattern(const std::string& class_name) {
    // Anonymous inner class: OuterClass$1, OuterClass$2, etc.
    // Must be: Class$digit or Class$digit$something
    size_t pos = class_name.find_last_of('$');
    if (pos == std::string::npos || pos == class_name.size() - 1) {
        return false;
    }

    // Check if character after $ is a digit
    char first_char = class_name[pos + 1];
    return std::isdigit(first_char);
}

std::string extractBaseClassName(const std::string& class_name) {
    // Remove proxy/lambda/anonymous suffixes
    size_t pos = class_name.find("$$");
    if (pos != std::string::npos) {
        return class_name.substr(0, pos);
    }

    pos = class_name.find('$');
    if (pos != std::string::npos) {
        return class_name.substr(0, pos);
    }

    return class_name;
}

DetectionResult::Severity calculateSeverity(size_t count, size_t memory) {
    const size_t CRITICAL_COUNT = 10000;
    const size_t HIGH_COUNT = 1000;
    const size_t MEDIUM_COUNT = 100;

    const size_t CRITICAL_MEMORY = 50 * 1024 * 1024;  // 50 MB
    const size_t HIGH_MEMORY = 10 * 1024 * 1024;      // 10 MB
    const size_t MEDIUM_MEMORY = 1 * 1024 * 1024;     // 1 MB

    if (count >= CRITICAL_COUNT || memory >= CRITICAL_MEMORY) {
        return DetectionResult::Severity::Critical;
    } else if (count >= HIGH_COUNT || memory >= HIGH_MEMORY) {
        return DetectionResult::Severity::High;
    } else if (count >= MEDIUM_COUNT || memory >= MEDIUM_MEMORY) {
        return DetectionResult::Severity::Medium;
    }
    return DetectionResult::Severity::Low;
}

std::string generateRecommendation(const std::string& pattern_type, size_t count) {
    if (pattern_type == "proxy") {
        return "Check for proxy class leaks. Review framework configurations (Spring AOP, "
               "Hibernate, etc.). Ensure proper cleanup of proxy instances. "
               "Consider using WeakReference caches.";
    } else if (pattern_type == "lambda") {
        return "Lambda classes are accumulating. Check for lambda expressions in hot paths. "
               "Consider extracting frequently-used lambdas to static fields. "
               "Review serialization of lambdas.";
    } else if (pattern_type == "duplicate") {
        return "Classes are loaded multiple times by different ClassLoaders. "
               "This indicates potential ClassLoader leaks. Check for: "
               "1) Hot-reload/redeploy issues, 2) Servlet context leaks, "
               "3) Thread pool with context ClassLoaders.";
    } else if (pattern_type == "script") {
        return "Script engine classes are accumulating. If using Groovy/JavaScript/etc., "
               "ensure proper cleanup of CompiledScript instances. "
               "Consider caching compiled scripts. Review ScriptEngine lifecycle.";
    } else if (pattern_type == "anonymous") {
        return "High number of anonymous inner classes detected. "
               "Consider: 1) Using named inner classes instead, "
               "2) Extracting to separate classes, 3) Using lambdas (Java 8+) where appropriate.";
    }
    return "Review class loading patterns and ensure proper cleanup.";
}

}  // anonymous namespace

DetectionResult DetectAnalyzer::detectProxyClasses(const std::vector<ClassLoaderInfo>& loaders) {
    DetectionResult result;
    result.pattern_name = "Proxy Class Accumulation";
    result.pattern_type = "proxy";
    result.description = "Dynamic proxy classes (JDK Proxy, CGLIB, Javassist, ByteBuddy)";

    std::vector<ClassMemoryInfo> all_proxies;

    for (const auto& loader : loaders) {
        if (!loader.has_detailed_class_info) {
            continue;
        }

        for (const auto& cls : loader.classes) {
            if (matchesProxyPattern(cls.class_name)) {
                all_proxies.push_back(cls);
                result.match_count++;
                result.total_memory += cls.total_size;
            }
        }
    }

    // Sort by memory and keep top examples
    std::sort(all_proxies.begin(), all_proxies.end(),
              [](const ClassMemoryInfo& a, const ClassMemoryInfo& b) {
                  return a.total_size > b.total_size;
              });

    size_t num_examples = std::min(size_t(5), all_proxies.size());
    result.examples.assign(all_proxies.begin(), all_proxies.begin() + num_examples);

    result.severity = calculateSeverity(result.match_count, result.total_memory);
    result.recommendation = generateRecommendation("proxy", result.match_count);

    return result;
}

DetectionResult DetectAnalyzer::detectLambdaClasses(const std::vector<ClassLoaderInfo>& loaders) {
    DetectionResult result;
    result.pattern_name = "Lambda Expression Classes";
    result.pattern_type = "lambda";
    result.description = "Accumulated lambda expression classes ($$Lambda$)";

    std::vector<ClassMemoryInfo> all_lambdas;

    for (const auto& loader : loaders) {
        if (!loader.has_detailed_class_info) {
            continue;
        }

        for (const auto& cls : loader.classes) {
            if (matchesLambdaPattern(cls.class_name)) {
                all_lambdas.push_back(cls);
                result.match_count++;
                result.total_memory += cls.total_size;
            }
        }
    }

    std::sort(all_lambdas.begin(), all_lambdas.end(),
              [](const ClassMemoryInfo& a, const ClassMemoryInfo& b) {
                  return a.total_size > b.total_size;
              });

    size_t num_examples = std::min(size_t(5), all_lambdas.size());
    result.examples.assign(all_lambdas.begin(), all_lambdas.begin() + num_examples);

    result.severity = calculateSeverity(result.match_count, result.total_memory);
    result.recommendation = generateRecommendation("lambda", result.match_count);

    return result;
}

DetectionResult DetectAnalyzer::detectDuplicateClasses(
    const std::vector<ClassLoaderInfo>& loaders) {
    DetectionResult result;
    result.pattern_name = "Duplicate Class Loading";
    result.pattern_type = "duplicate";
    result.description =
        "Same class loaded by multiple ClassLoaders";  // Map: class_name -> list of (loader_name,
                                                       // ClassMemoryInfo)
    std::map<std::string, std::vector<std::pair<std::string, ClassMemoryInfo>>> class_map;

    for (const auto& loader : loaders) {
        if (!loader.has_detailed_class_info) {
            continue;
        }

        for (const auto& cls : loader.classes) {
            class_map[cls.class_name].push_back({loader.loader_name, cls});
        }
    }

    // Find duplicates
    std::vector<ClassMemoryInfo> duplicates;
    for (const auto& [class_name, instances] : class_map) {
        if (instances.size() > 1) {
            result.match_count += instances.size();
            for (const auto& [loader_name, cls] : instances) {
                result.total_memory += cls.total_size;
                duplicates.push_back(cls);
            }
        }
    }

    // Sort by total size and keep examples
    std::sort(duplicates.begin(), duplicates.end(),
              [](const ClassMemoryInfo& a, const ClassMemoryInfo& b) {
                  return a.total_size > b.total_size;
              });

    size_t num_examples = std::min(size_t(5), duplicates.size());
    result.examples.assign(duplicates.begin(), duplicates.begin() + num_examples);

    result.severity = calculateSeverity(result.match_count, result.total_memory);
    result.recommendation = generateRecommendation("duplicate", result.match_count);

    return result;
}

DetectionResult DetectAnalyzer::detectScriptClasses(const std::vector<ClassLoaderInfo>& loaders) {
    DetectionResult result;
    result.pattern_name = "Script Engine Classes";
    result.pattern_type = "script";
    result.description = "Classes from scripting engines (Groovy, JavaScript, JRuby, Jython)";

    std::vector<ClassMemoryInfo> all_scripts;

    for (const auto& loader : loaders) {
        if (!loader.has_detailed_class_info) {
            continue;
        }

        for (const auto& cls : loader.classes) {
            if (matchesScriptPattern(cls.class_name)) {
                all_scripts.push_back(cls);
                result.match_count++;
                result.total_memory += cls.total_size;
            }
        }
    }

    std::sort(all_scripts.begin(), all_scripts.end(),
              [](const ClassMemoryInfo& a, const ClassMemoryInfo& b) {
                  return a.total_size > b.total_size;
              });

    size_t num_examples = std::min(size_t(5), all_scripts.size());
    result.examples.assign(all_scripts.begin(), all_scripts.begin() + num_examples);

    result.severity = calculateSeverity(result.match_count, result.total_memory);
    result.recommendation = generateRecommendation("script", result.match_count);

    return result;
}

DetectionResult DetectAnalyzer::detectAnonymousClasses(
    const std::vector<ClassLoaderInfo>& loaders) {
    DetectionResult result;
    result.pattern_name = "Anonymous Inner Classes";
    result.pattern_type = "anonymous";
    result.description = "Anonymous inner classes (e.g., OuterClass$1, OuterClass$2)";

    std::vector<ClassMemoryInfo> all_anonymous;

    for (const auto& loader : loaders) {
        if (!loader.has_detailed_class_info) {
            continue;
        }

        for (const auto& cls : loader.classes) {
            if (matchesAnonymousPattern(cls.class_name)) {
                all_anonymous.push_back(cls);
                result.match_count++;
                result.total_memory += cls.total_size;
            }
        }
    }

    std::sort(all_anonymous.begin(), all_anonymous.end(),
              [](const ClassMemoryInfo& a, const ClassMemoryInfo& b) {
                  return a.total_size > b.total_size;
              });

    size_t num_examples = std::min(size_t(5), all_anonymous.size());
    result.examples.assign(all_anonymous.begin(), all_anonymous.begin() + num_examples);

    result.severity = calculateSeverity(result.match_count, result.total_memory);
    result.recommendation = generateRecommendation("anonymous", result.match_count);

    return result;
}

DetectionResult DetectAnalyzer::detectReflectionHeavyClasses(
    const std::vector<ClassLoaderInfo>& loaders) {
    DetectionResult result;
    result.pattern_name = "Reflection-Heavy Classes";
    result.pattern_type = "reflection";
    result.description = "Classes with unusually high method/field counts";

    // Threshold: classes with > 200 methods or > 100 fields
    const size_t METHOD_THRESHOLD = 200;
    const size_t FIELD_THRESHOLD = 100;

    std::vector<ClassMemoryInfo> heavy_classes;

    for (const auto& loader : loaders) {
        if (!loader.has_detailed_class_info) {
            continue;
        }

        for (const auto& cls : loader.classes) {
            if (cls.method_count > METHOD_THRESHOLD || cls.field_count > FIELD_THRESHOLD) {
                heavy_classes.push_back(cls);
                result.match_count++;
                result.total_memory += cls.total_size;
            }
        }
    }

    std::sort(heavy_classes.begin(), heavy_classes.end(),
              [](const ClassMemoryInfo& a, const ClassMemoryInfo& b) {
                  return (a.method_count + a.field_count) > (b.method_count + b.field_count);
              });

    size_t num_examples = std::min(size_t(5), heavy_classes.size());
    result.examples.assign(heavy_classes.begin(), heavy_classes.begin() + num_examples);

    result.severity = calculateSeverity(result.match_count, result.total_memory);
    result.recommendation =
        "Classes with many methods/fields often indicate code generation. "
        "Review ORM mappings, serialization frameworks, or builders.";

    return result;
}

// ============================================================================
// Main Analysis Entry Point
// ============================================================================

std::vector<DetectionResult> DetectAnalyzer::analyze(const std::vector<ClassLoaderInfo>& loaders) {
    std::vector<DetectionResult> results;
    results.push_back(detectProxyClasses(loaders));
    results.push_back(detectLambdaClasses(loaders));
    results.push_back(detectDuplicateClasses(loaders));
    results.push_back(detectScriptClasses(loaders));
    results.push_back(detectAnonymousClasses(loaders));
    results.push_back(detectReflectionHeavyClasses(loaders));

    std::sort(results.begin(), results.end(),
              [](const DetectionResult& a, const DetectionResult& b) {
                  if (a.severity != b.severity) {
                      return a.severity > b.severity;
                  }
                  return a.total_memory > b.total_memory;
              });

    return results;
}

// ============================================================================
// Reporting
// ============================================================================

std::string DetectAnalyzer::formatReport(const std::vector<DetectionResult>& patterns,
                                         bool show_examples) {
    std::ostringstream ss;

    ss << "\n[Metaspace Pattern Analysis]\n";
    ss << "Detected potential issues based on class naming patterns:\n\n";

    int pattern_num = 1;
    for (const auto& pattern : patterns) {
        if (pattern.match_count == 0) {
            continue;  // Skip patterns with no matches
        }

        // Severity indicator
        std::string severity_str;
        switch (pattern.severity) {
            case DetectionResult::Severity::Critical:
                severity_str = "🔴 CRITICAL";
                break;
            case DetectionResult::Severity::High:
                severity_str = "🟠 HIGH";
                break;
            case DetectionResult::Severity::Medium:
                severity_str = "🟡 MEDIUM";
                break;
            case DetectionResult::Severity::Low:
                severity_str = "🟢 LOW";
                break;
        }

        ss << "[" << pattern_num++ << "] " << severity_str << " - " << pattern.pattern_name << "\n";
        ss << "    " << pattern.description << "\n";
        ss << "    Count: " << pattern.match_count << " classes\n";
        ss << "    Memory: " << formatBytes(pattern.total_memory) << "\n";
        ss << "    Recommendation: " << pattern.recommendation << "\n";

        if (show_examples && !pattern.examples.empty()) {
            ss << "    Examples:\n";
            for (size_t i = 0; i < pattern.examples.size(); ++i) {
                const auto& cls = pattern.examples[i];
                ss << "      " << (i + 1) << ". " << cls.class_name << " ("
                   << formatBytes(cls.total_size);
                if (cls.method_count > 0) {
                    ss << ", " << cls.method_count << " methods";
                }
                ss << ")\n";
            }
        }
        ss << "\n";
    }

    if (pattern_num == 1) {
        ss << "✅ No significant patterns detected. Metaspace usage appears normal.\n";
    }

    return ss.str();
}

}  // namespace jvmtool
