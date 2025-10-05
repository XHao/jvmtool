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

    bool initialize(const std::string& socket_path);

    int waitForClient();

    bool setNonBlocking();

    // Try accepting a client once in non-blocking mode.
    // Return >=0 : client fd
    //        -2 : no client yet (EAGAIN / EWOULDBLOCK / EINTR)
    //        -1 : fatal error (check getLastError())
    int tryAccept();

    bool writeMessage(int client_fd, const Message& message);

    void close();

    [[nodiscard]] bool isReady() const;

    [[nodiscard]] std::string getLastError() const;

    // Deleted copy constructor and assignment operator (public for better error messages)
    MessageWriter(const MessageWriter&) = delete;
    MessageWriter& operator=(const MessageWriter&) = delete;

  private:
    std::string path_;
    std::string last_error_;
    int socket_fd_;
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