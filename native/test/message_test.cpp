#include "message.h"
#include <gtest/gtest.h>
#include <vector>
#include <string>

using namespace jvmtool;

class MessageTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup test data
    }
    
    void TearDown() override {
        // Cleanup
    }
};

// Test MessageHeader
TEST_F(MessageTest, MessageHeaderConstruction) {
    MessageHeader header(AgentType::HEAP, STATUS, 100);
    
    EXPECT_EQ(header.version, PROTOCOL_VERSION);
    EXPECT_EQ(header.agent_type, AgentType::HEAP);
    EXPECT_EQ(header.content_type, STATUS);
    EXPECT_EQ(header.reserved, 0);
    EXPECT_EQ(header.content_length, 100);
    EXPECT_TRUE(header.isValid());
}

TEST_F(MessageTest, MessageHeaderDefaultConstruction) {
    MessageHeader header;
    
    EXPECT_EQ(header.version, PROTOCOL_VERSION);
    EXPECT_EQ(header.agent_type, AgentType::NONE);
    EXPECT_EQ(header.content_type, STATUS);
    EXPECT_EQ(header.reserved, 0);
    EXPECT_EQ(header.content_length, 0);
    EXPECT_TRUE(header.isValid());
}

// Test Message construction
TEST_F(MessageTest, MessageConstructionFromString) {
    std::string content = "Hello, World!";
    Message msg(AgentType::THREAD, DATA, content);
    
    EXPECT_EQ(msg.getHeader().agent_type, AgentType::THREAD);
    EXPECT_EQ(msg.getHeader().content_type, DATA);
    EXPECT_EQ(msg.getHeader().content_length, content.size());
    EXPECT_EQ(msg.getContent().size(), content.size());
    EXPECT_TRUE(msg.isValid());
}

TEST_F(MessageTest, MessageConstructionFromVector) {
    std::vector<uint8_t> content = {0x01, 0x02, 0x03, 0x04};
    Message msg(AgentType::CLASS, STATUS, content);
    
    EXPECT_EQ(msg.getHeader().agent_type, AgentType::CLASS);
    EXPECT_EQ(msg.getHeader().content_type, STATUS);
    EXPECT_EQ(msg.getHeader().content_length, content.size());
    EXPECT_EQ(msg.getContent(), content);
    EXPECT_TRUE(msg.isValid());
}

// Test factory methods (using direct constructors)
TEST_F(MessageTest, CreateStatusMessage) {
    std::string status = "Agent started successfully";
    Message msg(AgentType::HEAP, STATUS, status);
    
    EXPECT_EQ(msg.getHeader().agent_type, AgentType::HEAP);
    EXPECT_EQ(msg.getHeader().content_type, STATUS);
    EXPECT_EQ(msg.getContent().size(), status.size());
}

TEST_F(MessageTest, CreateErrorMessage) {
    std::string error = "Failed to initialize agent";
    Message msg(AgentType::GC, ERROR, error);
    
    EXPECT_EQ(msg.getHeader().agent_type, AgentType::GC);
    EXPECT_EQ(msg.getHeader().content_type, ERROR);
    EXPECT_EQ(msg.getContent().size(), error.size());
}

TEST_F(MessageTest, CreateDataMessage) {
    std::string data = "Heap usage: 75%";
    Message msg(AgentType::HEAP, DATA, data);
    
    EXPECT_EQ(msg.getHeader().agent_type, AgentType::HEAP);
    EXPECT_EQ(msg.getHeader().content_type, DATA);
    EXPECT_EQ(msg.getContent().size(), data.size());
}

// Test serialization (write-only for JVMTI agent)
TEST_F(MessageTest, Serialize) {
    std::string content = "Test serialization data";
    Message msg(AgentType::THREAD, DATA, content);
    
    // Serialize
    std::vector<uint8_t> serialized = msg.serialize();
    EXPECT_EQ(serialized.size(), msg.getTotalSize());
    
    // Verify header is correctly serialized
    EXPECT_EQ(serialized.size(), sizeof(MessageHeader) + content.size());
}

TEST_F(MessageTest, MessageHeaderPackedSize) {
    // Verify that the struct is properly packed to 8 bytes
    EXPECT_EQ(sizeof(MessageHeader), 8);
}

TEST_F(MessageTest, MessageTotalSize) {
    std::string content = "Test content";
    Message msg(AgentType::HEAP, DATA, content);
    
    EXPECT_EQ(msg.getTotalSize(), sizeof(MessageHeader) + content.size());
    EXPECT_EQ(msg.getTotalSize(), 8 + content.size()); // 8 bytes header + content
}
