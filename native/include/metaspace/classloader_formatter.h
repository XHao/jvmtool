#pragma once

#include <string>
#include <vector>

#include "metaspace/class_info.h"

namespace jvmtool {

/**
 * @brief Formatter for ClassLoader analysis results
 *
 * Separates presentation logic from analysis logic.
 * Supports different output formats and levels of detail.
 */
class ClassLoaderReportFormatter {
  public:
    /**
     * @brief Format output style
     */
    enum class Style {
        TEXT,    // Human-readable text
        JSON,    // JSON format
    };

    /**
     * @brief Format ClassLoader summary report
     *
     * @param loaders Vector of ClassLoaderInfo (should be sorted)
     * @param summary Aggregated statistics
     * @param top_n Number of top loaders to include (0 = all)
     * @param style Output style
     * @return Formatted report string
     */
    static std::string formatSummary(const std::vector<ClassLoaderInfo>& loaders,
                                     const ClassLoaderSummary& summary, size_t top_n = 10,
                                     Style style = Style::TEXT);

    /**
     * @brief Format detailed class-level memory report for a single ClassLoader
     *
     * @param loader ClassLoaderInfo with class details populated
     * @param top_n Number of top classes to show (0 = all)
     * @param style Output style
     * @return Formatted report string
     */
    static std::string formatClassDetails(const ClassLoaderInfo& loader, size_t top_n = 20,
                                          Style style = Style::TEXT);

    /**
     * @brief Format complete analysis report (summary + details)
     *
     * @param loaders Vector of ClassLoaderInfo
     * @param summary Aggregated statistics
     * @param top_loaders Number of top loaders to include
     * @param top_classes_per_loader Number of top classes per loader
     * @param style Output style
     * @return Formatted report string
     */
    static std::string formatComplete(const std::vector<ClassLoaderInfo>& loaders,
                                      const ClassLoaderSummary& summary, size_t top_loaders = 10,
                                      size_t top_classes_per_loader = 20,
                                      Style style = Style::TEXT);

  private:
    // Text formatting helpers
    static std::string formatSummaryText(const std::vector<ClassLoaderInfo>& loaders,
                                         const ClassLoaderSummary& summary, size_t top_n);
    static std::string formatClassDetailsText(const ClassLoaderInfo& loader, size_t top_n);

    // JSON formatting helpers
    static std::string formatSummaryJSON(const std::vector<ClassLoaderInfo>& loaders,
                                         const ClassLoaderSummary& summary, size_t top_n);
    static std::string formatClassDetailsJSON(const ClassLoaderInfo& loader, size_t top_n);

    // Utility functions
    static std::string formatBytes(size_t bytes);
    static std::string escapeJSON(const std::string& str);
};

}  // namespace jvmtool
