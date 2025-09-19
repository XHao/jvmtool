#include "common.h"

#include <stdexcept>

namespace jvmtool {

int parseInt(const std::unordered_map<std::string, std::string>& options, const std::string& key,
             int defaultValue) {
    auto it = options.find(key);
    if (it != options.end() && !it->second.empty()) {
        try {
            int value = std::stoi(it->second);
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

}  // namespace jvmtool
