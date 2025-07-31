#pragma once

#include <memory>
#include <string>

#include "message.h"
#ifdef _WIN32
    #include <windows.h>
#else
    #include <sys/socket.h>
    #include <sys/un.h>
    #include <unistd.h>
#endif

namespace jvmtool {

// Uses Unix domain socket on Unix/Linux, Named pipe on Windows
class MessageWriter {
  public:
    MessageWriter();
    ~MessageWriter();

    // path: Unix socket path on Unix/Linux, pipe name on Windows
    bool initialize(const std::string& path);

    bool writeMessage(const Message& message);

    bool flush();

    void close();

    bool isReady() const;

    // Get last error message
    std::string getLastError() const;

  private:
    std::string path_;
    std::string last_error_;
    bool is_ready_;

#ifdef _WIN32
    HANDLE pipe_handle_;
#else
    int socket_fd_;
#endif

    MessageWriter(const MessageWriter&) = delete;
    MessageWriter& operator=(const MessageWriter&) = delete;
};

}  // namespace jvmtool