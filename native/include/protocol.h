#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace jvmtool {

// Protocol version 1.0
static constexpr uint8_t PROTOCOL_VERSION = 1; // Version 1

// Agent running state
enum class AgentState : uint8_t {
    INITIALIZING = 0,
    READY = 1,
    COLLECTING = 2,
    ERROR = 3,
    COMPLETED = 4
};

// Message content type
enum class ContentType : uint8_t {
    STATUS = 0,
    ERROR = 1,
    HEAP_DATA = 2,
    GC_EVENT = 3,
    CLASS_INFO = 4,
    THREAD_INFO = 5
};

// Message header (fixed size for easy parsing)
struct MessageHeader {
    uint8_t version;          // Protocol version
    AgentState agent_state;   // Current agent state
    ContentType content_type; // Content type
    uint32_t content_length;  // Content length in bytes
    uint64_t timestamp;       // Timestamp
    uint8_t reserved[10];     // Reserved for future use
    
    MessageHeader(AgentState state, ContentType type, uint32_t length);
} __attribute__((packed));

// Complete message structure
class Message {
  private:
    MessageHeader header_;
    std::vector<uint8_t> content_;
    
  public:
    Message(AgentState state, ContentType type, const std::vector<uint8_t>& content);
    Message(AgentState state, ContentType type, const std::string& content);
    
    const MessageHeader& getHeader() const { return header_; }
    const std::vector<uint8_t>& getContent() const { return content_; }
    
    std::vector<uint8_t> serialize() const;
    size_t getTotalSize() const;
};

}  // namespace jvmtool
