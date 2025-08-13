#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace jvmtool {

// Protocol version 1.0
static constexpr uint8_t PROTOCOL_VERSION = 1;

// Agent type
enum class AgentType : uint8_t { NONE = 0, HEAP = 1, GC = 2, THREAD = 3, CLASS = 4 };

// Message content type
using ContentType = uint8_t;
inline constexpr ContentType STATUS = 0;
inline constexpr ContentType ERROR = 1;
inline constexpr ContentType DATA = 2;

// Message header (fixed size for easy parsing)
#if defined(_MSC_VER)
    #pragma pack(push, 1)
#endif
struct MessageHeader {
    uint8_t version;           // Protocol version
    AgentType agent_type;      // Agent type
    ContentType content_type;  // Content type
    uint8_t reserved;          // Reserved for future use
    uint32_t content_length;   // Content length in bytes

    MessageHeader();
    MessageHeader(AgentType agent_type, ContentType type, uint32_t length);

    // Utility methods
    [[nodiscard]] bool isValid() const;
}
#if defined(_MSC_VER)
;
    #pragma pack(pop)
#elif defined(__GNUC__)
__attribute__((packed));
#else
;
#endif

// Complete message structure
class Message {
  private:
    MessageHeader header_;
    std::vector<uint8_t> content_;

  public:
    // Constructors
    Message(AgentType agent_type, ContentType type, const std::vector<uint8_t>& content);
    Message(AgentType agent_type, ContentType type, const std::string& content);

    // Copy constructor and assignment operator
    Message(const Message& other) = default;
    Message& operator=(const Message& other) = default;

    // Move constructor and assignment operator
    Message(Message&& other) noexcept = default;
    Message& operator=(Message&& other) noexcept = default;

    // Destructor
    ~Message() = default;

    // Getters
    [[nodiscard]] const MessageHeader& getHeader() const {
        return header_;
    }
    [[nodiscard]] const std::vector<uint8_t>& getContent() const {
        return content_;
    }

    // Utility methods
    [[nodiscard]] std::vector<uint8_t> serialize() const;
    [[nodiscard]] size_t getTotalSize() const;
    [[nodiscard]] bool isValid() const;
};

}  // namespace jvmtool
