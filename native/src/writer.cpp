#include "writer.h"
#include "message.h"

#ifndef _WIN32
#include <cerrno>
#include <cstring>
#endif

namespace jvmtool {

MessageWriter::MessageWriter() 
    : is_ready_(false)
#ifdef _WIN32
    , pipe_handle_(INVALID_HANDLE_VALUE)
#else
    , socket_fd_(-1)
#endif
{
}

MessageWriter::~MessageWriter() {
    close();
}

bool MessageWriter::initialize(const std::string& path) {
    close();
    path_ = path;

#ifdef _WIN32
    // Windows Named Pipe implementation
    std::string pipe_name = path;
    
    // Ensure pipe name starts with \\.\pipe\
    if (pipe_name.find("\\\\.\\pipe\\") != 0) {
        pipe_name = "\\\\.\\pipe\\" + pipe_name;
    }

    // Create named pipe
    pipe_handle_ = CreateNamedPipeA(
        pipe_name.c_str(),
        PIPE_ACCESS_OUTBOUND,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
        1, // Max instances
        8192, // Output buffer size
        8192, // Input buffer size
        0, // Default timeout
        nullptr // Default security
    );

    if (pipe_handle_ == INVALID_HANDLE_VALUE) {
        last_error_ = "Failed to create named pipe: " + std::to_string(GetLastError());
        return false;
    }

    // Wait for client connection
    BOOL connected = ConnectNamedPipe(pipe_handle_, nullptr);
    if (!connected && GetLastError() != ERROR_PIPE_CONNECTED) {
        last_error_ = "Failed to connect to pipe client: " + std::to_string(GetLastError());
        CloseHandle(pipe_handle_);
        pipe_handle_ = INVALID_HANDLE_VALUE;
        return false;
    }

#else
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
#endif

    is_ready_ = true;
    return true;
}

bool MessageWriter::writeMessage(const Message& message) {
    if (!isReady()) {
        last_error_ = "Writer not initialized";
        return false;
    }

    auto serialized = message.serialize();

#ifdef _WIN32
    // Windows Named Pipe write
    DWORD bytes_written = 0;
    
    BOOL result = WriteFile(
        pipe_handle_,
        serialized.data(),
        static_cast<DWORD>(serialized.size()),
        &bytes_written,
        nullptr
    );

    if (!result || bytes_written != serialized.size()) {
        last_error_ = "Failed to write message: " + std::to_string(GetLastError());
        return false;
    }

#else
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
#endif

    return true;
}

bool MessageWriter::flush() {
    if (!isReady()) {
        return false;
    }

#ifdef _WIN32
    return FlushFileBuffers(pipe_handle_) != FALSE;
#else
    // Unix sockets don't need explicit flushing
    return true;
#endif
}

void MessageWriter::close() {
#ifdef _WIN32
    if (pipe_handle_ != INVALID_HANDLE_VALUE) {
        DisconnectNamedPipe(pipe_handle_);
        CloseHandle(pipe_handle_);
        pipe_handle_ = INVALID_HANDLE_VALUE;
    }
#else
    if (socket_fd_ != -1) {
        ::close(socket_fd_);
        socket_fd_ = -1;
        if (!path_.empty()) {
            unlink(path_.c_str());
        }
    }
#endif
    is_ready_ = false;
}

bool MessageWriter::isReady() const {
#ifdef _WIN32
    return is_ready_ && pipe_handle_ != INVALID_HANDLE_VALUE;
#else
    return is_ready_ && socket_fd_ != -1;
#endif
}

std::string MessageWriter::getLastError() const {
    return last_error_;
}

}  // namespace jvmtool
