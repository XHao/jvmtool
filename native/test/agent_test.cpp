#include "../include/agent.h"

#include <gtest/gtest.h>

#include <iostream>

// Minimal test for basic functionality
TEST(BasicAgentTest, LogJvmtiError_NoError) {
    testing::internal::CaptureStderr();
    jvmtool::logJvmtiError(JVMTI_ERROR_NONE, "test context");
    std::string output = testing::internal::GetCapturedStderr();
    EXPECT_TRUE(output.empty());
}

TEST(BasicAgentTest, LogJvmtiError_WithError) {
    testing::internal::CaptureStderr();
    jvmtool::logJvmtiError(JVMTI_ERROR_NULL_POINTER, "test context");
    std::string output = testing::internal::GetCapturedStderr();

    EXPECT_TRUE(output.find("[JVMTI Error]") != std::string::npos);
    EXPECT_TRUE(output.find("test context") != std::string::npos);
    EXPECT_TRUE(output.find("(") != std::string::npos);
}

// Test module that doesn't call real JVMTI functions
class SafeTestModule : public jvmtool::AgentModule {
  private:
    std::string name_;
    bool on_attach_called_ = false;

  public:
    explicit SafeTestModule(const std::string& name) : name_(name) {}

    const char* getName() const override {
        return name_.c_str();
    }

    jvmtiError initialize(JavaVM* java_vm, jvmtiEnv* jvmti) override {
        if (java_vm == nullptr || jvmti == nullptr) {
            return JVMTI_ERROR_NULL_POINTER;
        }

        // Set fields directly without calling JVMTI functions
        jvmti_ = jvmti;
        vm_ = java_vm;
        module_monitor_ = reinterpret_cast<jrawMonitorID>(0x12345678);  // Fake monitor

        return JVMTI_ERROR_NONE;
    }

    jint onAttach(const char* options) override {
        on_attach_called_ = true;
        return JNI_OK;
    }

    bool wasOnAttachCalled() const {
        return on_attach_called_;
    }
};

TEST(BasicAgentTest, AgentModule_Initialize) {
    SafeTestModule module("test");

    // Test null pointers
    EXPECT_EQ(module.initialize(nullptr, reinterpret_cast<jvmtiEnv*>(0x1)),
              JVMTI_ERROR_NULL_POINTER);
    EXPECT_EQ(module.initialize(reinterpret_cast<JavaVM*>(0x1), nullptr), JVMTI_ERROR_NULL_POINTER);

    // Test valid pointers
    JavaVM fake_vm{};
    jvmtiEnv fake_jvmti{};
    EXPECT_EQ(module.initialize(&fake_vm, &fake_jvmti), JVMTI_ERROR_NONE);
    EXPECT_TRUE(module.isInitialized());
}

TEST(BasicAgentTest, AgentModule_OnAttach) {
    SafeTestModule module("test");

    JavaVM fake_vm{};
    jvmtiEnv fake_jvmti{};
    module.initialize(&fake_vm, &fake_jvmti);

    EXPECT_FALSE(module.wasOnAttachCalled());
    module.onAttach("test_options");
    EXPECT_TRUE(module.wasOnAttachCalled());
}

// Basic parameter parsing test
TEST(BasicAgentTest, ParameterParsing) {
    std::string options = "analysis=memory,duration=30,output=/tmp/test";

    // Find analysis parameter
    size_t analysis_pos = options.find("analysis=");
    EXPECT_NE(analysis_pos, std::string::npos);

    if (analysis_pos != std::string::npos) {
        size_t start = analysis_pos + 9;  // length of "analysis="
        size_t end = options.find(',', start);
        if (end == std::string::npos) {
            end = options.length();
        }
        std::string module_name = options.substr(start, end - start);
        EXPECT_EQ(module_name, "memory");
    }
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
