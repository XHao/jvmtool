#include "message.h"

#include <cstring>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace jvmtool {

// MessageHeader Implementation
MessageHeader::MessageHeader()
    : version(PROTOCOL_VERSION),
      agent_type(AgentType::NONE),
      content_type(STATUS),
      reserved(0),
      content_length(0) {}

MessageHeader::MessageHeader(AgentType agent_type, ContentType type, uint32_t length)
    : version(PROTOCOL_VERSION),
      agent_type(agent_type),
      content_type(type),
      reserved(0),
      content_length(length) {}

bool MessageHeader::isValid() const {
    return version == PROTOCOL_VERSION &&
           static_cast<uint8_t>(agent_type) <= static_cast<uint8_t>(AgentType::CLASS) &&
           content_type <= DATA;
}

// Message Implementation
Message::Message(AgentType agent_type, ContentType type, const std::vector<uint8_t>& content)
    : header_(agent_type, type, static_cast<uint32_t>(content.size())), content_(content) {}

Message::Message(AgentType agent_type, ContentType type, const std::string& content)
    : header_(agent_type, type, static_cast<uint32_t>(content.size())),
      content_(content.begin(), content.end()) {}

std::vector<uint8_t> Message::serialize() const {
    std::vector<uint8_t> result;
    result.reserve(sizeof(MessageHeader) + content_.size());

    // Serialize header
    const uint8_t* header_ptr = reinterpret_cast<const uint8_t*>(&header_);
    result.insert(result.end(), header_ptr, header_ptr + sizeof(MessageHeader));

    // Serialize content
    result.insert(result.end(), content_.begin(), content_.end());

    return result;
}

size_t Message::getTotalSize() const {
    return sizeof(MessageHeader) + content_.size();
}

bool Message::isValid() const {
    return header_.isValid() && header_.content_length == content_.size();
}

}  // namespace jvmtool
