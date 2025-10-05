#include "common.h"

#include <stdexcept>

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

}  // namespace jvmtool
