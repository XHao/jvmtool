#include <gtest/gtest.h>
#include <memory>

#include "agent.h"
#include "metaspace/module.h"
#include "message.h"
#include "writer.h"

using namespace jvmtool;

class IntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Clean up any existing test files
        test_socket_path_ = "/tmp/integration_test_socket";
        std::remove(test_socket_path_.c_str());
    }
    
    void TearDown() override {
        // Clean up test files
        std::remove(test_socket_path_.c_str());
    }
    
    std::string test_socket_path_;
};

TEST_F(IntegrationTest, MessageWriterIntegration) {
    // Simplified integration test without actual socket communication
    MessageWriter writer;
    ASSERT_TRUE(writer.initialize(test_socket_path_));
    
    // Create and test a metaspace message
    std::string metaspace_data = R"({
        "type": "metaspace_analysis",
        "timestamp": 1609459200,
        "data": {
            "total_used": 12345678,
            "total_committed": 23456789,
            "shared_metaspace_size": 5432109
        }
    })";
    
    Message msg(AgentType::METASPACE, DATA, metaspace_data);
    
    // Test message creation and serialization
    EXPECT_TRUE(msg.isValid());
    EXPECT_EQ(msg.getHeader().agent_type, AgentType::METASPACE);
    EXPECT_EQ(msg.getHeader().content_type, DATA);
    EXPECT_EQ(msg.getHeader().content_length, metaspace_data.size());
    
    // Test serialization
    auto serialized = msg.serialize();
    EXPECT_EQ(serialized.size(), msg.getTotalSize());
    EXPECT_GE(serialized.size(), sizeof(MessageHeader));
    
    // Verify message structure
    MessageHeader* header = reinterpret_cast<MessageHeader*>(serialized.data());
    EXPECT_EQ(header->version, PROTOCOL_VERSION);
    EXPECT_EQ(header->agent_type, AgentType::METASPACE);
    EXPECT_EQ(header->content_type, DATA);
    EXPECT_EQ(header->content_length, metaspace_data.size());
    
    writer.close();
}

TEST_F(IntegrationTest, AgentModuleLifecycle) {
    // Test basic agent module lifecycle without socket operations
    auto module = std::make_unique<MetaspaceSAModule>();
    
    // Test initial state
    EXPECT_FALSE(module->isInitialized());
    EXPECT_STREQ(module->getName(), "meta");
    EXPECT_EQ(module->agentType(), AgentType::METASPACE);
    
    // Test that module can be created and destroyed safely
    EXPECT_NE(module.get(), nullptr);
}

TEST_F(IntegrationTest, MessageTypesIntegration) {
    // Test different message types
    std::vector<Message> test_messages = {
        Message(AgentType::METASPACE, STATUS, "Agent initialized"),
        Message(AgentType::METASPACE, DATA, R"({"heap_usage": 75})"),
        Message(AgentType::METASPACE, ERROR, "Analysis failed"),
        Message(AgentType::GC, STATUS, "GC monitoring started"),
        Message(AgentType::THREAD, DATA, "Thread dump data")
    };
    
    // Test message creation and serialization
    for (const auto& msg : test_messages) {
        EXPECT_TRUE(msg.isValid());
        auto serialized = msg.serialize();
        EXPECT_EQ(serialized.size(), msg.getTotalSize());
        EXPECT_GE(serialized.size(), sizeof(MessageHeader));
    }
}