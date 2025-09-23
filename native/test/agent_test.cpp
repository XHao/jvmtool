#include "agent.h"
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <unordered_map>

using namespace jvmtool;

// Mock JavaVM and jvmtiEnv for testing (simplified without gmock)
class MockJavaVM {
public:
    // Add minimal interface needed for testing
};

class MockJvmtiEnv {
public:
    // Simple mock without gmock - just basic interface
    jvmtiError last_create_monitor_result = JVMTI_ERROR_NONE;
    jvmtiError last_enter_result = JVMTI_ERROR_NONE;
    jvmtiError last_exit_result = JVMTI_ERROR_NONE;
    jvmtiError last_destroy_result = JVMTI_ERROR_NONE;
};

// Test AgentModule implementation for testing
class TestAgentModule : public AgentModule {
public:
    TestAgentModule() = default;

    jint onAttach(const TaskOpt& opt) override {
        attach_called_ = true;
        received_opt_ = opt;
        return 0;
    }

    const char* getName() const override { return "test"; }
    AgentType agentType() const override { return AgentType::METASPACE; }

    ModuleState getState() const { return state_; }
    void setState(ModuleState state) { state_ = state; }
    void resetState() { state_ = ModuleState::IDLE; }

    bool attach_called_ = false;
    TaskOpt received_opt_{};
};

class AgentModuleTest : public ::testing::Test {
protected:
    void SetUp() override {
        module_ = std::make_unique<TestAgentModule>();
    }
    
    void TearDown() override {
        module_.reset();
    }
    
    std::unique_ptr<TestAgentModule> module_;
};

TEST_F(AgentModuleTest, InitialState) {
    EXPECT_FALSE(module_->isInitialized());
    EXPECT_EQ(module_->getState(), ModuleState::IDLE);
    EXPECT_STREQ(module_->getName(), "test");
    EXPECT_EQ(module_->agentType(), AgentType::METASPACE);
}

TEST_F(AgentModuleTest, OnAttachCalledCorrectly) {
    TaskOpt opt{2, 10, "metaspace"};
    jint result = module_->onAttach(opt);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(module_->attach_called_);
    EXPECT_EQ(module_->received_opt_.interval, 2);
    EXPECT_EQ(module_->received_opt_.duration, 10);
    EXPECT_EQ(module_->received_opt_.type, "metaspace");
}

TEST_F(AgentModuleTest, StateManagement) {
    EXPECT_EQ(module_->getState(), ModuleState::IDLE);
    
    module_->setState(ModuleState::ANALYZING);
    EXPECT_EQ(module_->getState(), ModuleState::ANALYZING);
    
    module_->resetState();
    EXPECT_EQ(module_->getState(), ModuleState::IDLE);
}

// Test AgentManager
class AgentManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        manager_ = &AgentManager::instance();
        test_module_ = std::make_unique<TestAgentModule>();
    }
    
    void TearDown() override {
        test_module_.reset();
    }
    
    AgentManager* manager_;
    std::unique_ptr<TestAgentModule> test_module_;
};

TEST_F(AgentManagerTest, Singleton) {
    AgentManager& instance1 = AgentManager::instance();
    AgentManager& instance2 = AgentManager::instance();
    
    EXPECT_EQ(&instance1, &instance2);
}

TEST_F(AgentManagerTest, RegisterModule) {
    // Test that module can be registered
    // Transfer ownership to manager. After this call test_module_ should be null.
    EXPECT_NO_THROW({
        auto* raw = manager_->registerModule(std::move(test_module_));
        EXPECT_NE(raw, nullptr);
    });
    EXPECT_EQ(test_module_, nullptr);
}

// Test JVMTI error handling
class JvmtiErrorTest : public ::testing::Test {};

TEST_F(JvmtiErrorTest, KnownErrors) {
    // Test that known JVMTI errors are handled correctly
    testing::internal::CaptureStderr();
    logJvmtiError(JVMTI_ERROR_NULL_POINTER, "test context");
    std::string output = testing::internal::GetCapturedStderr();
    
    EXPECT_TRUE(output.find("NULL_POINTER") != std::string::npos);
    EXPECT_TRUE(output.find("test context") != std::string::npos);
    EXPECT_TRUE(output.find("Pointer parameter is NULL") != std::string::npos);
}

TEST_F(JvmtiErrorTest, NoErrorLogging) {
    // Test that no error is logged when JVMTI_ERROR_NONE is passed
    testing::internal::CaptureStderr();
    logJvmtiError(JVMTI_ERROR_NONE, "test context");
    std::string output = testing::internal::GetCapturedStderr();
    
    EXPECT_TRUE(output.empty());
}

TEST_F(JvmtiErrorTest, UnknownError) {
    // Test handling of unknown error codes
    testing::internal::CaptureStderr();
    logJvmtiError(static_cast<jvmtiError>(9999), "test context");
    std::string output = testing::internal::GetCapturedStderr();
    
    EXPECT_TRUE(output.find("UNKNOWN_ERROR") != std::string::npos);
    EXPECT_TRUE(output.find("Undefined JVMTI error") != std::string::npos);
}

// Test MonitorLock functionality (this would require more complex setup for real JVMTI)
class MonitorLockTest : public ::testing::Test {
protected:
    void SetUp() override {
        module_ = std::make_unique<TestAgentModule>();
    }
    
    std::unique_ptr<TestAgentModule> module_;
};

// Note: Real MonitorLock testing would require a full JVMTI environment
// This is a basic structure test
TEST_F(MonitorLockTest, BasicStructure) {
    // Test that the module exists and can be used
    EXPECT_NE(module_.get(), nullptr);
    EXPECT_FALSE(module_->isInitialized());
}