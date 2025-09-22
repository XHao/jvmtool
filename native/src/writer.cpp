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

    socket_fd_ = socket(AF_UNIX, SOCK_STREAM, 0);
    if (socket_fd_ == -1) {
        last_error_ = "Failed to create socket: " + std::string(strerror(errno));
        return false;
    }

    unlink(path.c_str());

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

    if (listen(socket_fd_, 1) == -1) {
        last_error_ = "Failed to listen on socket: " + std::string(strerror(errno));
        ::close(socket_fd_);
        socket_fd_ = -1;
        return false;
    }

    is_ready_ = true;
    return true;
}

int MessageWriter::waitForClient() {
    if (!isReady()) {
        last_error_ = "Writer not initialized";
        return -1;
    }

    int fd = accept(socket_fd_, nullptr, nullptr);
    if (fd == -1) {
        last_error_ = "Failed to accept connection: " + std::string(strerror(errno));
        return -1;
    }

    return fd;
}

void MessageWriter::disconnectClient(int fd) {
    if (fd != -1) {
        ::close(fd);
    }
}

bool MessageWriter::writeMessage(int fd, const Message& message) {
    if (!isReady()) {
        last_error_ = "Writer not initialized";
        return false;
    }

    if (fd == -1) {
        last_error_ = "No client connected";
        return false;
    }

    auto serialized = message.serialize();

    ssize_t bytes_written = write(fd, serialized.data(), serialized.size());

    if (bytes_written == -1) {
        last_error_ = "Failed to write to client: " + std::string(strerror(errno));
        return false;
    }

    if (bytes_written != static_cast<ssize_t>(serialized.size())) {
        last_error_ = "Failed to write complete message";
        return false;
    }

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
