#include "common.h"
#include <gtest/gtest.h>
#include <unordered_map>

using namespace jvmtool;

TEST(CommonParseStringTest, ReturnsValueWhenPresent) {
    std::unordered_map<std::string, std::string> opts{{"key", "value"}};
    EXPECT_EQ(parseString(opts, "key", "default"), "value");
}

TEST(CommonParseStringTest, ReturnsDefaultWhenMissing) {
    std::unordered_map<std::string, std::string> opts{{"other", "value"}};
    EXPECT_EQ(parseString(opts, "key", "default"), "default");
}

TEST(CommonParseStringTest, ReturnsDefaultWhenEmptyAndAllowEmptyTrue) {
    std::unordered_map<std::string, std::string> opts{{"key", ""}};
    EXPECT_EQ(parseString(opts, "key", "default", true), "default");
}

TEST(CommonParseStringTest, ThrowsWhenEmptyAndAllowEmptyFalse) {
    std::unordered_map<std::string, std::string> opts{{"key", ""}};
    EXPECT_THROW(parseString(opts, "key", "default", false), std::invalid_argument);
}

TEST(CommonParseStringTest, ReturnsNonEmptyEvenIfAllowEmptyFalse) {
    std::unordered_map<std::string, std::string> opts{{"key", "abc"}};
    EXPECT_EQ(parseString(opts, "key", "default", false), "abc");
}
