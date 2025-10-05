#include "writer.h"

#include <fcntl.h>

#include <array>
#include <cerrno>
#include <cstring>

#include "message.h"
#include "scope_guard.h"

namespace jvmtool {

namespace {
std::string safeStrerror(int errnum) {
#if ((defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 200112L) || defined(__APPLE__)) && \
    !defined(__GLIBC__)
    std::array<char, 256> buf{};
    if (strerror_r(errnum, buf.data(), buf.size()) == 0) {
        return {buf.data()};
    }
    return std::string("Unknown error ") + std::to_string(errnum);
#elif defined(__GLIBC__)
    // GNU variant returns char*
    std::array<char, 256> buf{};
    char* msg = strerror_r(errnum, buf.data(), buf.size());
    if (msg != nullptr) {
        return std::string(msg);
    }
    return std::string("Unknown error ") + std::to_string(errnum);
#else
    return std::string(strerror(errnum));
#endif
}

inline bool fillUnixAddr(struct sockaddr_un& addr, const std::string& socket_path,
                         std::string& err) {
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    if (socket_path.size() >= sizeof(addr.sun_path)) {
        err = "Socket path too long";
        return false;
    }
    const int written =
        std::snprintf(&addr.sun_path[0], sizeof(addr.sun_path), "%s", socket_path.c_str());
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

bool MessageWriter::initialize(const std::string& socket_path) {
    close();
    path_ = socket_path;

    auto cleanup = jvmtool::make_scope_exit([&]() { this->close(); });

    socket_fd_ = socket(AF_UNIX, SOCK_STREAM, 0);
    if (socket_fd_ == -1) {
        last_error_ = "Failed to create socket: " + safeStrerror(errno);
        return false;
    }

    unlink(socket_path.c_str());

    struct sockaddr_un addr;
    if (!fillUnixAddr(addr, socket_path, last_error_)) {
        return false;
    }

    if (bind(socket_fd_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) == -1) {
        last_error_ = "Failed to bind socket: " + safeStrerror(errno);
        return false;
    }

    if (listen(socket_fd_, 1) == -1) {
        last_error_ = "Failed to listen on socket: " + safeStrerror(errno);
        return false;
    }

    cleanup.dismiss();
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

bool MessageWriter::setNonBlocking() {
    if (!isReady()) {
        last_error_ = "Writer not initialized";
        return false;
    }
    const int flags = fcntl(socket_fd_, F_GETFL, 0);
    if (flags == -1) {
        last_error_ = "Failed to get socket flags: " + safeStrerror(errno);
        return false;
    }
    // Compute new flags in unsigned domain to satisfy static analyzers about signed bitwise ops
    const unsigned new_flags_u = static_cast<unsigned>(flags) | static_cast<unsigned>(O_NONBLOCK);
    if (fcntl(socket_fd_, F_SETFL, static_cast<int>(new_flags_u)) == -1) {
        last_error_ = "Failed to set non-blocking mode: " + safeStrerror(errno);
        return false;
    }
    return true;
}

int MessageWriter::tryAccept() {
    if (!isReady()) {
        last_error_ = "Writer not initialized";
        return -1;
    }
    const int fd = accept(socket_fd_, nullptr, nullptr);
    if (fd == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
            return -2;
        }
        last_error_ = "Failed to accept connection: " + safeStrerror(errno);
        return -1;
    }
    return fd;
}

bool MessageWriter::writeMessage(int client_fd, const Message& message) {
    if (!isReady()) {
        last_error_ = "Writer not initialized";
        return false;
    }

    if (client_fd == -1) {
        last_error_ = "No client connected";
        return false;
    }

    auto serialized = message.serialize();
    size_t total = 0;
    const size_t to_write = serialized.size();
    while (total < to_write) {
        const ssize_t written_bytes =
            ::write(client_fd, serialized.data() + total, to_write - total);
        if (written_bytes > 0) {
            total += static_cast<size_t>(written_bytes);
            continue;
        }
        if (written_bytes == -1 && errno == EINTR) {
            continue;
        }
        last_error_ = (written_bytes == 0)
                          ? std::string("Failed to write to client: peer closed")
                          : std::string("Failed to write to client: ") + safeStrerror(errno);
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
