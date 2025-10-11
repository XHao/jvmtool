#include "metaspace/class_loader_stats.h"

#include <cstring>

#include "common.h"

namespace jvmtool {

ClassLoaderStatsParser::ClassLoaderStatsParser(std::vector<ClassLoaderStats>& out,
                                               ClassLoaderSummary* summary)
    : out_(out), summary_(summary) {}

bool ClassLoaderStatsParser::startsWith(const std::string& line, const char* prefix) {
    return line.compare(0, std::strlen(prefix), prefix) == 0;
}

bool ClassLoaderStatsParser::detectHeader(const std::string& line) {
    const std::string trimmed = trimCopy(line);
    if (trimmed.empty()) {
        return false;
    }

    if (startsWith(trimmed, "Index")) {
        column_index_.clear();
        auto tokens = splitWhitespace(trimmed);
        for (size_t i = 0; i < tokens.size(); ++i) {
            column_index_.emplace(tokens[i], i);
        }
        format_ = ClassLoaderStatsTableFormat::kIndex;
        return true;
    }

    if (trimmed.find("ClassLoader") != std::string::npos &&
        (trimmed.find("Chunk") != std::string::npos ||
         trimmed.find("ChunkSz") != std::string::npos)) {
        format_ = ClassLoaderStatsTableFormat::kChunk;
        return true;
    }

    return false;
}

void ClassLoaderStatsParser::parseTotals(const std::string& line) {
    if (summary_ == nullptr) {
        return;
    }

    auto tokens = splitWhitespace(line);
    if (tokens.empty()) {
        return;
    }

    size_t start_index = 0;
    if (tokens.size() > 1 && tokens[1] == "=") {
        start_index = 2;
    } else if (tokens[0] == "Total") {
        start_index = 1;
    }

    std::vector<size_t> values;
    for (size_t i = start_index; i < tokens.size(); ++i) {
        if (tokens[i] == "=") {
            continue;
        }
        const size_t value = parseSizeToken(tokens[i]);
        values.push_back(value);
    }

    if (!values.empty()) {
        summary_->reported_loader_count = values[0];
    }
    if (values.size() >= 2) {
        summary_->reported_total_classes = values[1];
    }
    if (values.size() >= 3) {
        summary_->reported_total_chunk_bytes = values[2];
    }
    if (values.size() >= 4) {
        summary_->reported_total_block_bytes = values[3];
    }
}

bool ClassLoaderStatsParser::parseHiddenLine(const std::string& line) {
    if (format_ != ClassLoaderStatsTableFormat::kChunk) {
        return false;
    }
    if (line.find("+ hidden classes") == std::string::npos) {
        return false;
    }

    auto tokens = splitWhitespace(line);
    if (tokens.size() < 3 || current_ == nullptr) {
        return true;
    }

    size_t idx = 0;
    const size_t hidden_class_count = parseSizeToken(tokens[idx++]);
    const size_t hidden_chunk_bytes = (idx < tokens.size()) ? parseSizeToken(tokens[idx++]) : 0;
    const size_t hidden_block_bytes = (idx < tokens.size()) ? parseSizeToken(tokens[idx++]) : 0;

    current_->hidden_classes = hidden_class_count;
    current_->space.hidden_committed_bytes = hidden_chunk_bytes;
    current_->space.hidden_used_bytes = hidden_block_bytes;
    if (current_->space.fallback_total_bytes == 0) {
        current_->space.fallback_total_bytes = hidden_block_bytes;
    } else {
        current_->space.fallback_total_bytes += hidden_block_bytes;
    }

    return true;
}

bool ClassLoaderStatsParser::parseIndexRow(const std::string& line) {
    auto tokens = splitWhitespace(line);
    if (tokens.size() < 2) {
        return true;
    }

    const int loaderIdx = findColumnIndex(column_index_, {"Loader", "ClassLoader"});
    const int moduleIdx = findColumnIndex(column_index_, {"Module"});
    const int metaIdx = findColumnIndex(column_index_, {"Metaspace", "Meta"});
    const int klassIdx = findColumnIndex(column_index_, {"Class", "Klass"});
    const int methodIdx = findColumnIndex(column_index_, {"Method"});
    const int cpIdx = findColumnIndex(column_index_, {"ConstantPool", "CP"});

    if (loaderIdx < 0 || metaIdx < 0) {
        return true;
    }

    ClassLoaderStats stats;
    if (static_cast<size_t>(loaderIdx) < tokens.size()) {
        stats.loader_name = tokens[loaderIdx];
    }
    if (moduleIdx >= 0 && static_cast<size_t>(moduleIdx) < tokens.size()) {
        stats.module_name = tokens[moduleIdx];
    }
    if (static_cast<size_t>(metaIdx) < tokens.size()) {
        const size_t total_bytes = parseSizeToken(tokens[metaIdx]);
        stats.space.fallback_total_bytes = total_bytes;
        stats.space.committed_bytes = total_bytes;
        stats.space.used_bytes = total_bytes;
    }
    if (klassIdx >= 0 && static_cast<size_t>(klassIdx) < tokens.size()) {
        stats.metadata.klass_bytes = parseSizeToken(tokens[klassIdx]);
    }
    if (methodIdx >= 0 && static_cast<size_t>(methodIdx) < tokens.size()) {
        stats.metadata.method_bytes = parseSizeToken(tokens[methodIdx]);
    }
    if (cpIdx >= 0 && static_cast<size_t>(cpIdx) < tokens.size()) {
        stats.metadata.constant_pool_bytes = parseSizeToken(tokens[cpIdx]);
    }

    out_.emplace_back(std::move(stats));
    current_ = &out_.back();

    return true;
}

bool ClassLoaderStatsParser::parseChunkRow(const std::string& line) {
    auto tokens = splitWhitespace(line);
    if (tokens.size() < 6) {
        return true;
    }

    ClassLoaderStats stats;
    stats.loader_address = tokens[0];
    if (tokens.size() > 1) {
        stats.parent_address = tokens[1];
    }
    if (tokens.size() > 2) {
        stats.cld_address = tokens[2];
    }

    size_t index = 3;
    if (index < tokens.size()) {
        stats.class_count = parseSizeToken(tokens[index++]);
    }
    if (index < tokens.size()) {
        stats.space.committed_bytes = parseSizeToken(tokens[index++]);
    }
    if (index < tokens.size()) {
        stats.space.used_bytes = parseSizeToken(tokens[index++]);
        stats.space.fallback_total_bytes = stats.space.used_bytes;
    }

    if (stats.space.fallback_total_bytes == 0) {
        stats.space.fallback_total_bytes = stats.space.committed_bytes;
    }

    if (index < tokens.size()) {
        stats.loader_name = tokens[index++];
        for (; index < tokens.size(); ++index) {
            stats.loader_name.append(" ");
            stats.loader_name.append(tokens[index]);
        }
    } else {
        stats.loader_name = "<unknown>";
    }

    out_.emplace_back(std::move(stats));
    current_ = &out_.back();

    return true;
}

bool ClassLoaderStatsParser::addLine(const std::string& line) {
    if (!header_parsed_) {
        header_parsed_ = detectHeader(line);
        return true;
    }

    const std::string trimmed = trimCopy(line);
    if (trimmed.empty()) {
        return true;
    }

    if (trimmed.rfind("Total", 0) == 0) {
        parseTotals(trimmed);
        return false;
    }

    if (parseHiddenLine(trimmed)) {
        return true;
    }

    switch (format_) {
        case ClassLoaderStatsTableFormat::kIndex:
            return parseIndexRow(trimmed);
        case ClassLoaderStatsTableFormat::kChunk:
            return parseChunkRow(trimmed);
        case ClassLoaderStatsTableFormat::kUnknown:
        default:
            if (detectHeader(trimmed)) {
                return true;
            }
            return true;
    }
}

}  // namespace jvmtool
