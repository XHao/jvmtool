#include "writer.h"

#include <cerrno>
#include <cstring>

#include "message.h"

namespace jvmtool {

namespace {
std::string safeStrerror(int errnum) {
#if ((defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 200112L) || defined(__APPLE__)) && \
    !defined(__GLIBC__)
    char buf[256];
    if (strerror_r(errnum, buf, sizeof(buf)) == 0) {
        return {buf};
    }
    return std::string("Unknown error ") + std::to_string(errnum);
#elif defined(__GLIBC__)
    // GNU variant returns char*
    char buf[256];
    char* msg = strerror_r(errnum, buf, sizeof(buf));
    if (msg != nullptr) {
        return std::string(msg);
    }
    return std::string("Unknown error ") + std::to_string(errnum);
#else
    return std::string(strerror(errnum));
#endif
}

inline bool fillUnixAddr(struct sockaddr_un& addr, const std::string& path, std::string& err) {
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    if (path.size() >= sizeof(addr.sun_path)) {
        err = "Socket path too long";
        return false;
    }
    const int written = std::snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", path.c_str());
    if (written < 0 || static_cast<size_t>(written) >= sizeof(addr.sun_path)) {
        err = "Failed to set socket path";
        return false;
    }
    return true;
}
}  // anonymous namespace

MessageWriter::MessageWriter() : socket_fd_(-1) {}

MessageWriter::~MessageWriter() {
    close();
}

bool MessageWriter::initialize(const std::string& path) {
    close();
    path_ = path;

    socket_fd_ = socket(AF_UNIX, SOCK_STREAM, 0);
    if (socket_fd_ == -1) {
        last_error_ = "Failed to create socket: " + safeStrerror(errno);
        return false;
    }

    unlink(path.c_str());

    struct sockaddr_un addr;
    if (!fillUnixAddr(addr, path, last_error_)) {
        ::close(socket_fd_);
        socket_fd_ = -1;
        return false;
    }

    if (bind(socket_fd_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) == -1) {
        last_error_ = "Failed to bind socket: " + safeStrerror(errno);
        ::close(socket_fd_);
        socket_fd_ = -1;
        return false;
    }

    if (listen(socket_fd_, 1) == -1) {
        last_error_ = "Failed to listen on socket: " + safeStrerror(errno);
        ::close(socket_fd_);
        socket_fd_ = -1;
        return false;
    }

    return true;
}

int MessageWriter::waitForClient() {
    if (!isReady()) {
        last_error_ = "Writer not initialized";
        return -1;
    }

    const int fd = accept(socket_fd_, nullptr, nullptr);
    if (fd == -1) {
        last_error_ = "Failed to accept connection: " + safeStrerror(errno);
        return -1;
    }

    return fd;
}

/* static */ void MessageWriter::disconnectClient(int fd) {
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

    const ssize_t bytes_written = write(fd, serialized.data(), serialized.size());

    if (bytes_written == -1) {
        last_error_ = "Failed to write to client: " + safeStrerror(errno);
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
}

bool MessageWriter::isReady() const {
    return socket_fd_ != -1;
}

std::string MessageWriter::getLastError() const {
    return last_error_;
}

}  // namespace jvmtool
