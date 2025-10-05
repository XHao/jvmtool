#pragma once

#include <string>
#include <unordered_map>

namespace jvmtool {

int parseInt(const std::unordered_map<std::string, std::string>& options, const std::string& key,
             int default_value);

std::string parseString(const std::unordered_map<std::string, std::string>& options,
                        const std::string& key, const std::string& default_value = "",
                        bool allow_empty = true);

}  // namespace jvmtool
