#pragma once

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <memory>
#include <string>

#include "message.h"

namespace jvmtool {

// Uses Unix domain socket on Unix/Linux
class MessageWriter {
  public:
    MessageWriter();
    ~MessageWriter();

    // path: Unix socket path
    bool initialize(const std::string& path);

    bool writeMessage(const Message& message);

    bool flush();

    void close();

    [[nodiscard]] bool isReady() const;

    // Get last error message
    [[nodiscard]] std::string getLastError() const;

    // Deleted copy constructor and assignment operator (public for better error messages)
    MessageWriter(const MessageWriter&) = delete;
    MessageWriter& operator=(const MessageWriter&) = delete;

  private:
    std::string path_;
    std::string last_error_;
    bool is_ready_;

    int socket_fd_;
};

}  // namespace jvmtool