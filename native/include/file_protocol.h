#pragma once

#include <string>
#include <fstream>
#include <memory>
#include <unordered_map>

namespace jvmtool {

// File-based communication protocol for agent-host interaction
enum class MessageType {
    STATUS = 0,     // Agent status (attach success/failure)
    ERROR = 1,      // Error messages
    DATA = 2,       // Analysis data
    PROGRESS = 3,   // Progress updates
    RESULT = 4      // Final results
};

enum class StatusCode {
    SUCCESS = 0,
    ERROR = 1,
    RUNNING = 2,
    COMPLETED = 3
};

// Message structure for file-based communication
struct Message {
    MessageType type;
    StatusCode status;
    std::string content;
    std::string timestamp;
    std::unordered_map<std::string, std::string> metadata;
    
    Message(MessageType t, StatusCode s, const std::string& c) 
        : type(t), status(s), content(c) {}
};

// File communication manager
class FileProtocol {
private:
    std::string base_path_;
    
public:
    // Constructor takes the base path from jvmtool
    FileProtocol(const std::string& base_path);
    ~FileProtocol();
    
    // Send messages to the unified .jt file
    bool sendStatus(StatusCode status, const std::string& message = "");
    bool sendError(const std::string& error_message);
    bool sendData(const std::string& data, const std::string& data_type = "");
    bool sendProgress(int percentage, const std::string& description = "");
    bool sendResult(const std::string& result);
    
    // Utility methods
    std::string getProtocolFile() const;
    
private:
    bool writeMessage(const Message& message);
    std::string getCurrentTimestamp() const;
    std::string formatMessage(const Message& message) const;
};

}
