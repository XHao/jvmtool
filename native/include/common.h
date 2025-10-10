#pragma once

#include <initializer_list>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace jvmtool {

int parseInt(const std::unordered_map<std::string, std::string>& options, const std::string& key,
             int default_value);

std::string parseString(const std::unordered_map<std::string, std::string>& options,
                        const std::string& key, const std::string& default_value = "",
                        bool allow_empty = true);

std::string trimCopy(const std::string& str);

std::string toLowerCopy(const std::string& value);

std::vector<std::string> splitWhitespace(const std::string& line);

size_t parseSizeToken(const std::string& token);

int findColumnIndex(const std::unordered_map<std::string, size_t>& columns,
                    std::initializer_list<const char*> names);

std::string formatBytes(size_t bytes);

std::string formatPercentage(double value);

void joinThread(std::thread& t);

}  // namespace jvmtool
