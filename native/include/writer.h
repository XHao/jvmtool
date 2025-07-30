#pragma once

#include <memory>
#include <string>
#include <vector>

namespace jvmtool {

// Forward declaration
class Message;

// Abstract message writer interface
class MessageWriter {
  public:
    virtual ~MessageWriter() = default;
    
    // Initialize the writer with connection parameters
    virtual bool initialize(const std::string& params) = 0;
    
    // Write a message (uses Message's serialize method internally)
    virtual bool writeMessage(const Message& message) = 0;
    
    // Flush any buffered data
    virtual bool flush() = 0;
    
    // Close and cleanup
    virtual void close() = 0;
    
    // Check if writer is ready for writing
    virtual bool isReady() const = 0;
    
    // Get last error message
    virtual std::string getLastError() const = 0;

  protected:
    MessageWriter() = default;
};

// Factory for creating platform-specific writers
class MessageWriterFactory {
  public:
    // Create writer based on connection string (auto-detects platform)
    static std::unique_ptr<MessageWriter> createWriter(const std::string& connection_params);

  private:
    MessageWriterFactory() = delete;
};

}  // namespace jvmtool