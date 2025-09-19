#pragma once

#include <string>
#include <unordered_map>

namespace jvmtool {

int parseInt(const std::unordered_map<std::string, std::string>& options, const std::string& key,
             int defaultValue);

}  // namespace jvmtool
