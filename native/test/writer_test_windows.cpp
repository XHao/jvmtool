#include <gtest/gtest.h>
#include <memory>
#include <thread>
#include <chrono>
#include <vector>

#include "writer.h"
#include "message.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#endif

namespace jvmtool {

class CrossPlatformMessageWriterTest : public ::testing::Test {
protected:
    void SetUp() override {
#ifdef _WIN32
        test_path_ = "test_writer_pipe";
#else
        test_path_ = "/tmp/test_writer_socket";
        // Clean up any existing files
        std::remove(test_path_.c_str());
#endif
    }

    void TearDown() override {
#ifndef _WIN32
        // Clean up test files on Unix
        std::remove(test_path_.c_str());
#endif
    }

    std::string test_path_;
};

#ifdef _WIN32
// Windows-specific client for named pipes
class WindowsPipeClient {
public:
    explicit WindowsPipeClient(const std::string& pipe_name) 
        : pipe_name_(pipe_name), handle_(INVALID_HANDLE_VALUE) {}

    ~WindowsPipeClient() {
        close();
    }

    bool connect() {
        std::string full_pipe_name = "\\\\.\\pipe\\" + pipe_name_;
        
        // Wait for pipe to become available
        if (!WaitNamedPipeA(full_pipe_name.c_str(), 5000)) {
            return false;
        }

        handle_ = CreateFileA(
            full_pipe_name.c_str(),
            GENERIC_READ,
            0,
            nullptr,
            OPEN_EXISTING,
            0,
            nullptr
        );

        return handle_ != INVALID_HANDLE_VALUE;
    }

    bool readData(std::vector<uint8_t>& data) {
        if (handle_ == INVALID_HANDLE_VALUE) {
            return false;
        }

        DWORD bytes_available = 0;
        if (!PeekNamedPipe(handle_, nullptr, 0, nullptr, &bytes_available, nullptr)) {
            return false;
        }

        if (bytes_available == 0) {
            return true; // No data available, but not an error
        }

        data.resize(bytes_available);
        DWORD bytes_read = 0;
        BOOL result = ReadFile(handle_, data.data(), bytes_available, &bytes_read, nullptr);
        
        if (!result || bytes_read != bytes_available) {
            data.clear();
            return false;
        }

        return true;
    }

    void close() {
        if (handle_ != INVALID_HANDLE_VALUE) {
            CloseHandle(handle_);
            handle_ = INVALID_HANDLE_VALUE;
        }
    }

private:
    std::string pipe_name_;
    HANDLE handle_;
};
#endif

TEST_F(CrossPlatformMessageWriterTest, BasicWriteTest) {
    MessageWriter writer;

    // Initially not ready
    EXPECT_FALSE(writer.isReady());

    // Initialize
    EXPECT_TRUE(writer.initialize(test_path_));
    EXPECT_TRUE(writer.isReady());

    // Test data
    std::string test_content = "Hello, Cross-platform World!";
    Message msg(AgentType::HEAP, DATA, test_content);

    // Client thread to read data
    std::vector<uint8_t> received_data;
    bool client_finished = false;
    std::string error_message;

#ifdef _WIN32
    std::thread client_thread([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        WindowsPipeClient client(test_path_);
        if (client.connect()) {
            // Give some time for the write operation
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            if (!client.readData(received_data)) {
                error_message = "Failed to read from pipe";
            }
        } else {
            error_message = "Failed to connect to pipe";
        }
        client_finished = true;
    });
#else
    std::thread client_thread([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        int client_socket = socket(AF_UNIX, SOCK_STREAM, 0);
        if (client_socket != -1) {
            struct sockaddr_un addr;
            memset(&addr, 0, sizeof(addr));
            addr.sun_family = AF_UNIX;
            strncpy(addr.sun_path, test_path_.c_str(), sizeof(addr.sun_path) - 1);

            int connect_result = connect(client_socket, (struct sockaddr*)&addr, sizeof(addr));
            if (connect_result == 0) {
                uint8_t buffer[1024];
                ssize_t bytes_read = read(client_socket, buffer, sizeof(buffer));
                if (bytes_read > 0) {
                    received_data.assign(buffer, buffer + bytes_read);
                }
            } else {
                error_message = "Failed to connect to socket";
            }
            close(client_socket);
        } else {
            error_message = "Failed to create socket";
        }
        client_finished = true;
    });
#endif

    // Write message
    EXPECT_TRUE(writer.writeMessage(msg)) << "Failed to write message: " << writer.getLastError();
    EXPECT_TRUE(writer.flush());

    // Wait for client
    client_thread.join();
    EXPECT_TRUE(client_finished);
    EXPECT_TRUE(error_message.empty()) << "Client error: " << error_message;

    writer.close();
    EXPECT_FALSE(writer.isReady());

    // Verify data
    auto expected_data = msg.serialize();
    EXPECT_EQ(received_data.size(), expected_data.size()) 
        << "Expected " << expected_data.size() << " bytes, got " << received_data.size();
    EXPECT_EQ(received_data, expected_data);
}

TEST_F(CrossPlatformMessageWriterTest, MultipleMessagesTest) {
    MessageWriter writer;
    EXPECT_TRUE(writer.initialize(test_path_));

    std::vector<Message> messages = {
        Message(AgentType::HEAP, STATUS, "Status message"),
        Message(AgentType::THREAD, DATA, "Thread data"),
        Message(AgentType::GC, ERROR, "GC error")
    };

    std::vector<std::vector<uint8_t>> received_messages;
    bool client_finished = false;

#ifdef _WIN32
    std::thread client_thread([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // On Windows, we need to handle each message separately due to pipe design
        for (size_t i = 0; i < messages.size(); ++i) {
            WindowsPipeClient client(test_path_ + "_" + std::to_string(i));
            if (client.connect()) {
                std::vector<uint8_t> msg_data;
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                if (client.readData(msg_data) && !msg_data.empty()) {
                    received_messages.push_back(msg_data);
                }
            }
        }
        client_finished = true;
    });

    // Write messages to different pipes
    for (size_t i = 0; i < messages.size(); ++i) {
        MessageWriter msg_writer;
        std::string pipe_path = test_path_ + "_" + std::to_string(i);
        EXPECT_TRUE(msg_writer.initialize(pipe_path));
        EXPECT_TRUE(msg_writer.writeMessage(messages[i]));
        msg_writer.close();
    }
#else
    std::thread client_thread([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        for (size_t i = 0; i < messages.size(); ++i) {
            int client_socket = socket(AF_UNIX, SOCK_STREAM, 0);
            if (client_socket != -1) {
                struct sockaddr_un addr;
                memset(&addr, 0, sizeof(addr));
                addr.sun_family = AF_UNIX;
                strncpy(addr.sun_path, test_path_.c_str(), sizeof(addr.sun_path) - 1);

                int connect_result = connect(client_socket, (struct sockaddr*)&addr, sizeof(addr));
                if (connect_result == 0) {
                    uint8_t buffer[1024];
                    ssize_t bytes_read = read(client_socket, buffer, sizeof(buffer));
                    if (bytes_read > 0) {
                        received_messages.push_back(std::vector<uint8_t>(buffer, buffer + bytes_read));
                    }
                }
                close(client_socket);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        client_finished = true;
    });

    // Write all messages
    for (const auto& msg : messages) {
        EXPECT_TRUE(writer.writeMessage(msg));
    }
#endif

    client_thread.join();
    EXPECT_TRUE(client_finished);

#ifndef _WIN32
    writer.close();
#endif

    // Verify all messages received
    EXPECT_EQ(received_messages.size(), messages.size());
    for (size_t i = 0; i < messages.size() && i < received_messages.size(); ++i) {
        auto expected = messages[i].serialize();
        EXPECT_EQ(received_messages[i], expected) 
            << "Message " << i << " mismatch";
    }
}

TEST_F(CrossPlatformMessageWriterTest, ErrorHandling) {
    MessageWriter writer;

    // Should fail with invalid path
#ifdef _WIN32
    EXPECT_FALSE(writer.initialize("invalid\\path\\that\\cannot\\be\\created"));
#else
    EXPECT_FALSE(writer.initialize("/invalid/path/that/does/not/exist"));
#endif
    EXPECT_FALSE(writer.isReady());
    EXPECT_FALSE(writer.getLastError().empty());

    // Should fail to write when not initialized
    Message msg(AgentType::NONE, STATUS, "test");
    EXPECT_FALSE(writer.writeMessage(msg));
}

TEST_F(CrossPlatformMessageWriterTest, LargeMessageTest) {
    MessageWriter writer;
    EXPECT_TRUE(writer.initialize(test_path_));

    // Create large message (4KB)
    std::string large_content(4096, 'A');
    Message msg(AgentType::HEAP, DATA, large_content);

    std::vector<uint8_t> received_data;
    bool client_finished = false;

#ifdef _WIN32
    std::thread client_thread([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        WindowsPipeClient client(test_path_);
        if (client.connect()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            client.readData(received_data);
        }
        client_finished = true;
    });
#else
    std::thread client_thread([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        int client_socket = socket(AF_UNIX, SOCK_STREAM, 0);
        if (client_socket != -1) {
            struct sockaddr_un addr;
            memset(&addr, 0, sizeof(addr));
            addr.sun_family = AF_UNIX;
            strncpy(addr.sun_path, test_path_.c_str(), sizeof(addr.sun_path) - 1);

            int connect_result = connect(client_socket, (struct sockaddr*)&addr, sizeof(addr));
            if (connect_result == 0) {
                // Read in chunks for large messages
                std::vector<uint8_t> buffer(8192);
                ssize_t total_read = 0;
                ssize_t bytes_read;
                
                while ((bytes_read = read(client_socket, buffer.data() + total_read, 
                                        buffer.size() - total_read)) > 0) {
                    total_read += bytes_read;
                    if (total_read >= static_cast<ssize_t>(msg.getTotalSize())) {
                        break;
                    }
                }
                
                if (total_read > 0) {
                    received_data.assign(buffer.begin(), buffer.begin() + total_read);
                }
            }
            close(client_socket);
        }
        client_finished = true;
    });
#endif

    EXPECT_TRUE(writer.writeMessage(msg));
    writer.flush();

    client_thread.join();
    EXPECT_TRUE(client_finished);
    
    writer.close();

    // Verify large message
    auto expected_data = msg.serialize();
    EXPECT_EQ(received_data.size(), expected_data.size());
    EXPECT_EQ(received_data, expected_data);
}

}  // namespace jvmtool