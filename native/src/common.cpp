#include "common.h"

#include <stdexcept>

namespace jvmtool {

int parseInt(const std::unordered_map<std::string, std::string>& options, const std::string& key,
             int defaultValue) {
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
    return defaultValue;
}

std::string parseString(const std::unordered_map<std::string, std::string>& options,
                        const std::string& key, const std::string& defaultValue, bool allowEmpty) {
    auto it = options.find(key);
    if (it == options.end()) {
        return defaultValue;
    }

    const std::string& value = it->second;
    if (value.empty()) {
        if (!allowEmpty) {
            throw std::invalid_argument("Invalid " + key + " parameter: value must not be empty");
        }
        return defaultValue;
    }
    return value;
}

}  // namespace jvmtool
