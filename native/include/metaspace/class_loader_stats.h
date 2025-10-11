#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

namespace jvmtool {

struct ClassLoaderSpaceUsage {
    size_t committed_bytes{0};
    size_t used_bytes{0};
    size_t hidden_committed_bytes{0};
    size_t hidden_used_bytes{0};
    size_t fallback_total_bytes{0};

    size_t totalCommittedBytes() const {
        size_t total = committed_bytes + hidden_committed_bytes;
        if (total == 0) {
            total = fallback_total_bytes;
        }
        return total;
    }

    size_t totalUsedBytes() const {
        size_t total = used_bytes + hidden_used_bytes;
        if (total == 0) {
            total = fallback_total_bytes;
        }
        return total;
    }
};

struct ClassLoaderMetadataUsage {
    size_t klass_bytes{0};
    size_t method_bytes{0};
    size_t constant_pool_bytes{0};

    size_t total() const {
        return klass_bytes + method_bytes + constant_pool_bytes;
    }
};

struct ClassLoaderStats {
    std::string loader_name;
    std::string module_name;
    std::string loader_address;
    std::string parent_address;
    std::string cld_address;
    size_t class_count{0};
    size_t hidden_classes{0};
    ClassLoaderSpaceUsage space;
    ClassLoaderMetadataUsage metadata;

    size_t usedBytes() const {
        size_t used = space.totalUsedBytes();
        if (used == 0) {
            used = metadata.total();
        }
        return used;
    }

    size_t committedBytes() const {
        size_t committed = space.totalCommittedBytes();
        if (committed == 0) {
            committed = usedBytes();
        }
        return committed;
    }
};

struct ClassLoaderSummary {
    size_t loader_count{0};
    size_t total_class_count{0};
    size_t total_hidden_classes{0};
    size_t aggregated_committed_bytes{0};
    size_t aggregated_used_bytes{0};
    size_t aggregated_hidden_committed_bytes{0};
    size_t aggregated_hidden_used_bytes{0};
    size_t aggregated_metadata_bytes{0};
    size_t reported_loader_count{0};
    size_t reported_total_classes{0};
    size_t reported_total_chunk_bytes{0};
    size_t reported_total_block_bytes{0};

    size_t totalCommittedBytes() const {
        return aggregated_committed_bytes + aggregated_hidden_committed_bytes;
    }

    size_t totalUsedBytes() const {
        return aggregated_used_bytes + aggregated_hidden_used_bytes;
    }
};

enum class ClassLoaderStatsTableFormat { kUnknown, kIndex, kChunk };

class ClassLoaderStatsParser {
  public:
    ClassLoaderStatsParser(std::vector<ClassLoaderStats>& out, ClassLoaderSummary* summary);

    bool addLine(const std::string& line);

  private:
    static bool startsWith(const std::string& line, const char* prefix);

    bool detectHeader(const std::string& line);
    void parseTotals(const std::string& line);
    bool parseHiddenLine(const std::string& line);
    bool parseIndexRow(const std::string& line);
    bool parseChunkRow(const std::string& line);

    std::vector<ClassLoaderStats>& out_;
    ClassLoaderSummary* summary_;
    bool header_parsed_{false};
    ClassLoaderStatsTableFormat format_{ClassLoaderStatsTableFormat::kUnknown};
    std::unordered_map<std::string, size_t> column_index_{};
    ClassLoaderStats* current_{nullptr};
};

}  // namespace jvmtool
