#include "metaspace/classloader_formatter.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace jvmtool {

namespace {

/**
 * @brief Format bytes to human-readable string
 */
std::string formatBytes(size_t bytes) {
    if (bytes < 1024) {
        return std::to_string(bytes) + "B";
    } else if (bytes < 1024 * 1024) {
        double kb = static_cast<double>(bytes) / 1024.0;
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(1) << kb << "KB";
        return ss.str();
    } else if (bytes < 1024 * 1024 * 1024) {
        double mb = static_cast<double>(bytes) / (1024.0 * 1024.0);
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(1) << mb << "MB";
        return ss.str();
    } else {
        double gb = static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0);
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(1) << gb << "GB";
        return ss.str();
    }
}

/**
 * @brief Format percentage
 */
std::string formatPercentage(double percent) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(1) << percent << "%";
    return ss.str();
}

/**
 * @brief Escape string for JSON
 */
std::string escapeJSON(const std::string& str) {
    std::ostringstream ss;
    for (char c : str) {
        switch (c) {
            case '"':
                ss << "\\\"";
                break;
            case '\\':
                ss << "\\\\";
                break;
            case '\b':
                ss << "\\b";
                break;
            case '\f':
                ss << "\\f";
                break;
            case '\n':
                ss << "\\n";
                break;
            case '\r':
                ss << "\\r";
                break;
            case '\t':
                ss << "\\t";
                break;
            default:
                ss << c;
        }
    }
    return ss.str();
}

}  // anonymous namespace

std::string ClassLoaderReportFormatter::formatSummary(const std::vector<ClassLoaderInfo>& loaders,
                                                      const ClassLoaderSummary& summary,
                                                      size_t top_n, Style style) {
    switch (style) {
        case Style::TEXT:
            return formatSummaryText(loaders, summary, top_n);
        case Style::JSON:
            return formatSummaryJSON(loaders, summary, top_n);
        default:
            return formatSummaryText(loaders, summary, top_n);
    }
}

std::string ClassLoaderReportFormatter::formatClassDetails(const ClassLoaderInfo& loader,
                                                           size_t top_n, Style style) {
    switch (style) {
        case Style::TEXT:
            return formatClassDetailsText(loader, top_n);
        case Style::JSON:
            return formatClassDetailsJSON(loader, top_n);
        default:
            return formatClassDetailsText(loader, top_n);
    }
}

std::string ClassLoaderReportFormatter::formatComplete(const std::vector<ClassLoaderInfo>& loaders,
                                                       const ClassLoaderSummary& summary,
                                                       size_t top_loaders,
                                                       size_t top_classes_per_loader, Style style) {
    std::ostringstream ss;

    // Format summary
    ss << formatSummary(loaders, summary, top_loaders, style);

    // Format class details for top loaders
    size_t count = std::min(top_loaders, loaders.size());
    for (size_t i = 0; i < count; ++i) {
        if (loaders[i].has_detailed_class_info && !loaders[i].classes.empty()) {
            ss << "\n" << formatClassDetails(loaders[i], top_classes_per_loader, style);
        }
    }

    return ss.str();
}

std::string ClassLoaderReportFormatter::formatSummaryText(
    const std::vector<ClassLoaderInfo>& loaders, const ClassLoaderSummary& summary, size_t top_n) {
    std::ostringstream ss;

    ss << "\n[ClassLoader Analysis Summary]\n";
    ss << "- Total ClassLoaders: " << summary.total_loaders << "\n";
    ss << "- Total Classes: " << summary.total_classes << "\n";
    ss << "  - Bootstrap: " << summary.boot_loader_classes << "\n";
    ss << "  - Platform: " << summary.platform_loader_classes << "\n";
    ss << "  - Application: "
       << (summary.total_classes - summary.boot_loader_classes - summary.platform_loader_classes)
       << "\n";

    bool has_metaspace_data = summary.total_metaspace_used > 0;
    if (has_metaspace_data) {
        ss << "- Total Metaspace Used: " << formatBytes(summary.total_metaspace_used) << " ("
           << summary.total_metaspace_used << " bytes)\n";
        ss << "- Total Metaspace Committed: " << formatBytes(summary.total_metaspace_committed)
           << " (" << summary.total_metaspace_committed << " bytes)\n";
    } else {
        ss << "- Metaspace usage: Not available (VMStructs not enabled)\n";
    }

    const size_t display_count = (top_n == 0) ? loaders.size() : std::min(top_n, loaders.size());
    if (display_count > 0) {
        ss << "\n[Top " << display_count << " ClassLoaders by "
           << (has_metaspace_data ? "Memory Usage" : "Class Count") << "]\n";
        ss << std::setw(4) << "Rank" << "  " << std::setw(10)
           << (has_metaspace_data ? "Used" : "Classes") << "  " << std::setw(8) << "Percent"
           << "  " << "Loader\n";

        for (size_t i = 0; i < display_count; ++i) {
            const auto& loader = loaders[i];
            double percent = 0.0;

            if (has_metaspace_data && summary.total_metaspace_used > 0) {
                percent = (static_cast<double>(loader.metaspace_used_bytes) /
                           static_cast<double>(summary.total_metaspace_used)) *
                          100.0;
                ss << std::setw(4) << (i + 1) << "  " << std::setw(10)
                   << formatBytes(loader.metaspace_used_bytes) << "  " << std::setw(7)
                   << formatPercentage(percent) << "  " << loader.loader_name;
            } else {
                percent = (static_cast<double>(loader.class_count) /
                           static_cast<double>(summary.total_classes)) *
                          100.0;
                ss << std::setw(4) << (i + 1) << "  " << std::setw(10) << loader.class_count << "  "
                   << std::setw(7) << formatPercentage(percent) << "  " << loader.loader_name;
            }

            if (loader.is_boot_loader) {
                ss << " [bootstrap]";
            } else if (loader.is_platform_loader) {
                ss << " [platform]";
            }

            ss << "\n";

            // Show additional details for top 3
            if (i < 3 && loader.has_vmstructs_data) {
                if (loader.cld_address != nullptr) {
                    ss << "      CLD=" << loader.cld_address;
                }
                if (loader.metaspace_capacity_bytes > 0) {
                    ss << " capacity=" << formatBytes(loader.metaspace_capacity_bytes);
                }
                if (loader.cld_address != nullptr || loader.metaspace_capacity_bytes > 0) {
                    ss << "\n";
                }
            }
        }

        if (loaders.size() > display_count) {
            ss << "... (" << (loaders.size() - display_count) << " more loaders)\n";
        }
    }

    if (!has_metaspace_data) {
        ss << "\n[Note]\n";
        ss << "VMStructs-based metaspace analysis is not yet available.\n";
        ss << "Currently showing class count statistics only.\n";
        ss << "Enable VMStructs integration for precise memory analysis.\n";
    }

    return ss.str();
}

std::string ClassLoaderReportFormatter::formatClassDetailsText(const ClassLoaderInfo& loader,
                                                               size_t top_n) {
    std::ostringstream ss;

    ss << "\n[Class Details for: " << loader.loader_name << "]\n";
    ss << "Total Classes: " << loader.classes.size() << "\n";

    if (loader.classes.empty()) {
        ss << "No detailed class information available.\n";
        return ss.str();
    }

    const size_t display_count =
        (top_n == 0) ? loader.classes.size() : std::min(top_n, loader.classes.size());

    ss << "\n[Top " << display_count << " Classes by Memory Usage]\n";
    ss << std::setw(4) << "Rank" << "  " << std::setw(10) << "Total" << "  " << std::setw(8)
       << "Klass" << "  " << std::setw(8) << "Methods" << "  " << std::setw(8) << "CP" << "  "
       << "Class Name\n";

    for (size_t i = 0; i < display_count; ++i) {
        const auto& cls = loader.classes[i];
        ss << std::setw(4) << (i + 1) << "  " << std::setw(10) << formatBytes(cls.total_size)
           << "  " << std::setw(8) << formatBytes(cls.klass_size) << "  " << std::setw(8)
           << formatBytes(cls.methods_size) << "  " << std::setw(8)
           << formatBytes(cls.constant_pool_size) << "  " << cls.class_name << "\n";
    }

    if (loader.classes.size() > display_count) {
        ss << "... (" << (loader.classes.size() - display_count) << " more classes)\n";
    }

    return ss.str();
}

std::string ClassLoaderReportFormatter::formatSummaryJSON(
    const std::vector<ClassLoaderInfo>& loaders, const ClassLoaderSummary& summary, size_t top_n) {
    std::ostringstream ss;

    ss << "{\n";
    ss << "  \"summary\": {\n";
    ss << "    \"total_loaders\": " << summary.total_loaders << ",\n";
    ss << "    \"total_classes\": " << summary.total_classes << ",\n";
    ss << "    \"boot_loader_classes\": " << summary.boot_loader_classes << ",\n";
    ss << "    \"platform_loader_classes\": " << summary.platform_loader_classes << ",\n";
    ss << "    \"total_metaspace_used\": " << summary.total_metaspace_used << ",\n";
    ss << "    \"total_metaspace_committed\": " << summary.total_metaspace_committed << "\n";
    ss << "  },\n";

    ss << "  \"loaders\": [\n";

    const size_t display_count = (top_n == 0) ? loaders.size() : std::min(top_n, loaders.size());
    for (size_t i = 0; i < display_count; ++i) {
        const auto& loader = loaders[i];

        ss << "    {\n";
        ss << "      \"rank\": " << (i + 1) << ",\n";
        ss << "      \"name\": \"" << escapeJSON(loader.loader_name) << "\",\n";
        ss << "      \"type\": \"" << escapeJSON(loader.loader_type) << "\",\n";
        ss << "      \"class_count\": " << loader.class_count << ",\n";
        ss << "      \"metaspace_used\": " << loader.metaspace_used_bytes << ",\n";
        ss << "      \"metaspace_capacity\": " << loader.metaspace_capacity_bytes << ",\n";
        ss << "      \"is_boot_loader\": " << (loader.is_boot_loader ? "true" : "false") << ",\n";
        ss << "      \"is_platform_loader\": " << (loader.is_platform_loader ? "true" : "false")
           << "\n";
        ss << "    }";

        if (i < display_count - 1) {
            ss << ",";
        }
        ss << "\n";
    }

    ss << "  ]\n";
    ss << "}\n";

    return ss.str();
}

std::string ClassLoaderReportFormatter::formatClassDetailsJSON(const ClassLoaderInfo& loader,
                                                               size_t top_n) {
    std::ostringstream ss;

    ss << "{\n";
    ss << "  \"loader\": \"" << escapeJSON(loader.loader_name) << "\",\n";
    ss << "  \"total_classes\": " << loader.classes.size() << ",\n";
    ss << "  \"classes\": [\n";

    const size_t display_count =
        (top_n == 0) ? loader.classes.size() : std::min(top_n, loader.classes.size());

    for (size_t i = 0; i < display_count; ++i) {
        const auto& cls = loader.classes[i];

        ss << "    {\n";
        ss << "      \"rank\": " << (i + 1) << ",\n";
        ss << "      \"name\": \"" << escapeJSON(cls.class_name) << "\",\n";
        ss << "      \"total_size\": " << cls.total_size << ",\n";
        ss << "      \"klass_size\": " << cls.klass_size << ",\n";
        ss << "      \"methods_size\": " << cls.methods_size << ",\n";
        ss << "      \"constant_pool_size\": " << cls.constant_pool_size << ",\n";
        ss << "      \"vtable_size\": " << cls.vtable_size << ",\n";
        ss << "      \"itable_size\": " << cls.itable_size << "\n";
        ss << "    }";

        if (i < display_count - 1) {
            ss << ",";
        }
        ss << "\n";
    }

    ss << "  ]\n";
    ss << "}\n";

    return ss.str();
}

std::string ClassLoaderReportFormatter::formatBytes(size_t bytes) {
    return ::jvmtool::formatBytes(bytes);
}

std::string ClassLoaderReportFormatter::escapeJSON(const std::string& str) {
    return ::jvmtool::escapeJSON(str);
}

}  // namespace jvmtool
