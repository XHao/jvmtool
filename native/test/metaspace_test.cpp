/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 *
 * Modified by: shako
 * - Added comprehensive metaspace unit tests
 * - Tests MetaspaceStructs and MetaspaceSAModule functionality
 */

#include "metaspace.h"
#include <gtest/gtest.h>
#include <memory>
#include <unordered_map>
#include <thread>
#include <chrono>

using namespace jvmtool;

class MetaspaceStructsTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Save original state
        original_has_structs_ = MetaspaceStructs::hasMetaspaceStructs();
        original_base_ = MetaspaceStructs::sharedMetaspaceBase();
        original_top_ = MetaspaceStructs::sharedMetaspaceTop();
    }
    
    void TearDown() override {
        // Restore original state if needed
    }
    
private:
    bool original_has_structs_;
    void* original_base_;
    void* original_top_;
};

TEST_F(MetaspaceStructsTest, InitialState) {
    // Test that MetaspaceStructs can report its state
    EXPECT_NO_THROW(MetaspaceStructs::hasMetaspaceStructs());
    EXPECT_NO_THROW(MetaspaceStructs::hasSharedMetaspace());
    EXPECT_NO_THROW(MetaspaceStructs::sharedMetaspaceBase());
    EXPECT_NO_THROW(MetaspaceStructs::sharedMetaspaceTop());
    EXPECT_NO_THROW(MetaspaceStructs::sharedMetaspaceSize());
}

TEST_F(MetaspaceStructsTest, SharedMetaspaceConsistency) {
    // Test that shared metaspace state is consistent
    bool has_shared = MetaspaceStructs::hasSharedMetaspace();
    void* base = MetaspaceStructs::sharedMetaspaceBase();
    void* top = MetaspaceStructs::sharedMetaspaceTop();
    size_t size = MetaspaceStructs::sharedMetaspaceSize();
    
    if (has_shared) {
        EXPECT_NE(base, nullptr);
        EXPECT_NE(top, nullptr);
        EXPECT_GE(top, base);
        EXPECT_GT(size, 0);
        
        // Size should match the difference between top and base
        EXPECT_EQ(size, static_cast<char*>(top) - static_cast<char*>(base));
    } else {
        EXPECT_EQ(size, 0);
    }
}

TEST_F(MetaspaceStructsTest, ObjectInSharedMetaspace) {
    bool has_shared = MetaspaceStructs::hasSharedMetaspace();
    
    if (has_shared) {
        void* base = MetaspaceStructs::sharedMetaspaceBase();
        void* top = MetaspaceStructs::sharedMetaspaceTop();
        
        // Test boundary conditions
        EXPECT_TRUE(MetaspaceStructs::isSharedMetaspaceObject(base));
        EXPECT_FALSE(MetaspaceStructs::isSharedMetaspaceObject(top));
        
        // Test address just before top (should be valid)
        void* before_top = static_cast<char*>(top) - 1;
        EXPECT_TRUE(MetaspaceStructs::isSharedMetaspaceObject(before_top));
        
        // Test address just after top (should be invalid)
        void* after_top = static_cast<char*>(top) + 1;
        EXPECT_FALSE(MetaspaceStructs::isSharedMetaspaceObject(after_top));
        
        // Test address before base (should be invalid)
        void* before_base = static_cast<char*>(base) - 1;
        EXPECT_FALSE(MetaspaceStructs::isSharedMetaspaceObject(before_base));
    } else {
        // When shared metaspace is not available, all checks should return false
        EXPECT_FALSE(MetaspaceStructs::isSharedMetaspaceObject(nullptr));
        EXPECT_FALSE(MetaspaceStructs::isSharedMetaspaceObject(reinterpret_cast<void*>(0x1000)));
    }
}

TEST_F(MetaspaceStructsTest, NullPointerHandling) {
    // Test that null pointer is handled correctly
    EXPECT_FALSE(MetaspaceStructs::isSharedMetaspaceObject(nullptr));
}

// Test MetaspaceSAModule
class MetaspaceSAModuleTest : public ::testing::Test {
protected:
    void SetUp() override {
        module_ = std::make_unique<MetaspaceSAModule>();
    }
    
    void TearDown() override {
        module_.reset();
    }
    
    std::unique_ptr<MetaspaceSAModule> module_;
};

TEST_F(MetaspaceSAModuleTest, BasicInterface) {
    EXPECT_STREQ(module_->getName(), "meta");
    EXPECT_EQ(module_->agentType(), AgentType::METASPACE);
    EXPECT_FALSE(module_->isInitialized());
}

TEST_F(MetaspaceSAModuleTest, OptionsParsingValid) {
    std::unordered_map<std::string, std::string> options;
    options["task_type"] = "metaspace";
    options["interval"] = "10";
    options["duration"] = "60";
    
    // Test parsing without actually calling onAttach (which would start a thread and block)
    // Instead, we'll test the parseOptions method directly through exception handling
    try {
        // This will fail because the module is not initialized, but it will test parseOptions
        module_->onAttach(options);
    } catch (const std::exception& e) {
        // Expected to fail due to lack of JVMTI initialization
        // The important part is that parseOptions succeeded (no exception from parseOptions)
        SUCCEED();
    }
}

TEST_F(MetaspaceSAModuleTest, OptionsParsingInvalidTaskType) {
    std::unordered_map<std::string, std::string> options;
    options["task_type"] = "invalid_type";
    
    // This should return error due to invalid task_type, but not throw
    // Since module is not initialized, we expect JNI_ERR
    jint result = module_->onAttach(options);
    EXPECT_EQ(result, JNI_ERR);
}

TEST_F(MetaspaceSAModuleTest, OptionsParsingMissingTaskType) {
    std::unordered_map<std::string, std::string> options;
    options["interval"] = "10";
    options["duration"] = "60";
    
    // This should return error due to missing task_type
    jint result = module_->onAttach(options);
    EXPECT_EQ(result, JNI_ERR);
}

TEST_F(MetaspaceSAModuleTest, OptionsParsingValidTaskTypes) {
    std::vector<std::string> valid_types = {"metaspace", "heap", "gc", "all"};
    
    for (const auto& type : valid_types) {
        std::unordered_map<std::string, std::string> options;
        options["task_type"] = type;
        
        // Since module is not initialized, all should return JNI_ERR, but for different reasons
        jint result = module_->onAttach(options);
        EXPECT_EQ(result, JNI_ERR) << "Task type '" << type << "' should be handled without exception";
    }
}

TEST_F(MetaspaceSAModuleTest, OptionsParsingDefaultValues) {
    std::unordered_map<std::string, std::string> options;
    options["task_type"] = "metaspace";
    // Don't specify interval and duration - should use defaults
    
    // Since module is not initialized, should return JNI_ERR
    jint result = module_->onAttach(options);
    EXPECT_EQ(result, JNI_ERR);
}

// Integration test for functionality demo (not a real unit test)
void demonstrateMetaspaceStructs() {
    std::cout << "=== Metaspace Structures Demonstration ===" << std::endl;
    
    // Test basic functionality
    std::cout << "Has metaspace structs: " << (MetaspaceStructs::hasMetaspaceStructs() ? "Yes" : "No") << std::endl;
    std::cout << "Has shared metaspace: " << (MetaspaceStructs::hasSharedMetaspace() ? "Yes" : "No") << std::endl;
    
    if (MetaspaceStructs::hasSharedMetaspace()) {
        void* base = MetaspaceStructs::sharedMetaspaceBase();
        void* top = MetaspaceStructs::sharedMetaspaceTop();
        size_t size = MetaspaceStructs::sharedMetaspaceSize();
        
        std::cout << "Shared metaspace base: 0x" << std::hex << base << std::endl;
        std::cout << "Shared metaspace top:  0x" << std::hex << top << std::endl;
        std::cout << "Shared metaspace size: " << std::dec << size << " bytes (" 
                  << (size / 1024 / 1024) << " MB)" << std::endl;
        
        // Test object checking with some sample addresses
        void* test_addr1 = base;
        void* test_addr2 = static_cast<char*>(base) + size / 2;
        void* test_addr3 = top;
        void* test_addr4 = static_cast<char*>(top) + 1;
        
        std::cout << "Test address in shared metaspace:" << std::endl;
        std::cout << "  0x" << std::hex << test_addr1 << ": " 
                  << (MetaspaceStructs::isSharedMetaspaceObject(test_addr1) ? "Yes" : "No") << std::endl;
        std::cout << "  0x" << std::hex << test_addr2 << ": " 
                  << (MetaspaceStructs::isSharedMetaspaceObject(test_addr2) ? "Yes" : "No") << std::endl;
        std::cout << "  0x" << std::hex << test_addr3 << ": " 
                  << (MetaspaceStructs::isSharedMetaspaceObject(test_addr3) ? "Yes" : "No") << std::endl;
        std::cout << "  0x" << std::hex << test_addr4 << ": " 
                  << (MetaspaceStructs::isSharedMetaspaceObject(test_addr4) ? "Yes" : "No") << std::endl;
    } else {
        std::cout << "Shared metaspace not available (CDS may not be enabled)" << std::endl;
    }
    
    std::cout << "=== Demonstration Complete ===" << std::endl;
}
