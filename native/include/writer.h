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

    // Wait for client connection (blocking call)
    int waitForClient();

    // Disconnect current client
    void disconnectClient(int fd);

    bool writeMessage(int fd, const Message& message);

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

    int socket_fd_;  // Server socket
};

class ClosableFd {
  public:
    ClosableFd(int fd) : fd_(fd) {};
    ~ClosableFd() {
        try {
            if (fd_ != -1) {
                ::close(fd_);
            }
        } catch (...) {
        }
    };

    ClosableFd(const ClosableFd&) = delete;
    ClosableFd& operator=(const ClosableFd&) = delete;

    operator int() const noexcept {
        return fd_;
    }

  private:
    int fd_;
};

}  // namespace jvmtool