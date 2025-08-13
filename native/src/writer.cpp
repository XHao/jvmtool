#include "writer.h"

#include <cerrno>
#include <cstring>

#include "message.h"

namespace jvmtool {

MessageWriter::MessageWriter() : is_ready_(false), socket_fd_(-1) {}

MessageWriter::~MessageWriter() {
    close();
}

bool MessageWriter::initialize(const std::string& path) {
    close();
    path_ = path;

    // Unix Socket implementation
    socket_fd_ = socket(AF_UNIX, SOCK_STREAM, 0);
    if (socket_fd_ == -1) {
        last_error_ = "Failed to create socket: " + std::string(strerror(errno));
        return false;
    }

    // Remove existing socket file if it exists
    unlink(path.c_str());

    // Bind socket
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;

    if (path.length() >= sizeof(addr.sun_path)) {
        last_error_ = "Socket path too long";
        ::close(socket_fd_);
        socket_fd_ = -1;
        return false;
    }

    strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);

    if (bind(socket_fd_, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        last_error_ = "Failed to bind socket: " + std::string(strerror(errno));
        ::close(socket_fd_);
        socket_fd_ = -1;
        return false;
    }

    // Listen for connections
    if (listen(socket_fd_, 1) == -1) {
        last_error_ = "Failed to listen on socket: " + std::string(strerror(errno));
        ::close(socket_fd_);
        socket_fd_ = -1;
        return false;
    }

    is_ready_ = true;
    return true;
}

bool MessageWriter::writeMessage(const Message& message) {
    if (!isReady()) {
        last_error_ = "Writer not initialized";
        return false;
    }

    auto serialized = message.serialize();

    // Unix Socket write
    int client_fd = accept(socket_fd_, nullptr, nullptr);
    if (client_fd == -1) {
        last_error_ = "Failed to accept connection: " + std::string(strerror(errno));
        return false;
    }

    ssize_t bytes_written = write(client_fd, serialized.data(), serialized.size());
    ::close(client_fd);

    if (bytes_written != static_cast<ssize_t>(serialized.size())) {
        last_error_ = "Failed to write complete message: " + std::string(strerror(errno));
        return false;
    }

    return true;
}

bool MessageWriter::flush() {
    if (!isReady()) {
        return false;
    }

    // Unix sockets don't need explicit flushing
    return true;
}

void MessageWriter::close() {
    if (socket_fd_ != -1) {
        ::close(socket_fd_);
        socket_fd_ = -1;
        if (!path_.empty()) {
            unlink(path_.c_str());
        }
    }
    is_ready_ = false;
}

bool MessageWriter::isReady() const {
    return is_ready_ && socket_fd_ != -1;
}

std::string MessageWriter::getLastError() const {
    return last_error_;
}

}  // namespace jvmtool
