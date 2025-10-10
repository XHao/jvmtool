#include "common.h"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace jvmtool {

int parseInt(const std::unordered_map<std::string, std::string>& options, const std::string& key,
             int default_value) {
    auto it = options.find(key);
    if (it != options.end() && !it->second.empty()) {
        try {
            const int value = std::stoi(it->second);
            if (value <= 0) {
                throw std::invalid_argument("must be a positive integer");
            }
            return value;
        } catch (const std::exception& e) {
            throw std::invalid_argument("Invalid " + key + " parameter: " + it->second + " (" +
                                        e.what() + ")");
        }
    }
    return default_value;
}

std::string parseString(const std::unordered_map<std::string, std::string>& options,
                        const std::string& key, const std::string& default_value,
                        bool allow_empty) {
    auto it = options.find(key);
    if (it == options.end()) {
        return default_value;
    }

    const std::string& value = it->second;
    if (value.empty()) {
        if (!allow_empty) {
            throw std::invalid_argument("Invalid " + key + " parameter: value must not be empty");
        }
        return default_value;
    }
    return value;
}

std::string trimCopy(const std::string& str) {
    auto begin = std::find_if_not(str.begin(), str.end(),
                                  [](unsigned char ch) { return std::isspace(ch) != 0; });
    if (begin == str.end()) {
        return {};
    }
    auto end = std::find_if_not(str.rbegin(), str.rend(), [](unsigned char ch) {
                   return std::isspace(ch) != 0;
               }).base();
    return std::string(begin, end);
}

std::string toLowerCopy(const std::string& value) {
    std::string result(value);
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return result;
}

std::vector<std::string> splitWhitespace(const std::string& line) {
    std::vector<std::string> tokens;
    size_t i = 0;
    const size_t len = line.length();
    while (i < len) {
        while (i < len && std::isspace(static_cast<unsigned char>(line[i])) != 0) {
            ++i;
        }
        if (i >= len) {
            break;
        }
        size_t j = i;
        while (j < len && std::isspace(static_cast<unsigned char>(line[j])) == 0) {
            ++j;
        }
        tokens.emplace_back(line.substr(i, j - i));
        i = j;
    }
    return tokens;
}

size_t parseSizeToken(const std::string& token) {
    if (token.empty()) {
        return 0;
    }
    try {
        return static_cast<size_t>(std::stoull(token));
    } catch (...) {
        return 0;
    }
}

int findColumnIndex(const std::unordered_map<std::string, size_t>& columns,
                    std::initializer_list<const char*> names) {
    for (const char* name : names) {
        auto it = columns.find(name);
        if (it != columns.end()) {
            return static_cast<int>(it->second);
        }
    }
    return -1;
}

std::string formatBytes(size_t bytes) {
    static const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    double value = static_cast<double>(bytes);
    size_t unit_index = 0;
    const size_t unit_count = sizeof(units) / sizeof(units[0]);
    while (value >= 1024.0 && unit_index + 1 < unit_count) {
        value /= 1024.0;
        ++unit_index;
    }

    std::ostringstream oss;
    if (unit_index == 0) {
        oss << static_cast<size_t>(value) << ' ' << units[unit_index];
    } else {
        const double precision = (value >= 100.0) ? 1.0 : 100.0;
        const int decimals = (precision == 1.0) ? 1 : 2;
        oss << std::fixed << std::setprecision(decimals) << value << ' ' << units[unit_index];
    }
    return oss.str();
}

std::string formatPercentage(double value) {
    std::ostringstream oss;
    if (value >= 100.0) {
        oss << std::fixed << std::setprecision(0) << value << '%';
    } else if (value >= 10.0) {
        oss << std::fixed << std::setprecision(1) << value << '%';
    } else {
        oss << std::fixed << std::setprecision(2) << value << '%';
    }
    return oss.str();
}

void joinThread(std::thread& t) {
    try {
        if (t.joinable()) {
            t.join();
        }
    } catch (...) {
    }
}

}  // namespace jvmtool
