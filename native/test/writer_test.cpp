#include <gtest/gtest.h>
#include <memory>
#include <thread>
#include <chrono>
#include <fstream>
#include <vector>

#include "writer.h"
#include "message.h"

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace jvmtool {

class MessageWriterTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_socket_path_ = "/tmp/test_writer_socket";
        
        // Clean up any existing files
        std::remove(test_socket_path_.c_str());
    }

    void TearDown() override {
        // Clean up test files
        std::remove(test_socket_path_.c_str());
    }

    std::string test_socket_path_;
};

TEST_F(MessageWriterTest, FileWriterBasicFunctionality) {
    MessageWriter writer;

    // Initially not ready
    EXPECT_FALSE(writer.isReady());

    // Initialize with socket output (current implementation only supports sockets)
    EXPECT_TRUE(writer.initialize(test_socket_path_));
    EXPECT_TRUE(writer.isReady());

    // Start a client thread to connect and read from the socket
    std::vector<uint8_t> received_data;
    bool client_finished = false;
    
    std::thread client_thread([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Give server time to setup
        
        int client_socket = socket(AF_UNIX, SOCK_STREAM, 0);
        if (client_socket != -1) {
            struct sockaddr_un addr;
            memset(&addr, 0, sizeof(addr));
            addr.sun_family = AF_UNIX;
            strncpy(addr.sun_path, test_socket_path_.c_str(), sizeof(addr.sun_path) - 1);

            int connect_result = connect(client_socket, (struct sockaddr*)&addr, sizeof(addr));
            if (connect_result == 0) {
                uint8_t buffer[1024];
                ssize_t bytes_read = read(client_socket, buffer, sizeof(buffer));
                if (bytes_read > 0) {
                    received_data.assign(buffer, buffer + bytes_read);
                }
            }
            close(client_socket);
        }
        client_finished = true;
    });

    // Create test message
    std::string test_content = "Hello, World!";
    Message msg(AgentType::HEAP, DATA, test_content);

    // Write message
    EXPECT_TRUE(writer.writeMessage(msg));
    EXPECT_TRUE(writer.flush());

    // Wait for client to finish
    client_thread.join();
    EXPECT_TRUE(client_finished);

    // Close writer
    writer.close();
    EXPECT_FALSE(writer.isReady());

    // Verify received data
    auto expected_content = msg.serialize();
    EXPECT_EQ(received_data, expected_content);
}

TEST_F(MessageWriterTest, FileWriterMultipleMessages) {
    MessageWriter writer;

    EXPECT_TRUE(writer.initialize(test_socket_path_));

    // Start a client thread to connect and read from the socket
    std::vector<std::vector<uint8_t>> received_messages;
    bool client_finished = false;
    
    std::thread client_thread([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Give server time to setup
        
        // Since each writeMessage() call expects a separate connection,
        // we need to make multiple connections
        for (size_t i = 0; i < 3; ++i) {  // 3 messages
            int client_socket = socket(AF_UNIX, SOCK_STREAM, 0);
            if (client_socket != -1) {
                struct sockaddr_un addr;
                memset(&addr, 0, sizeof(addr));
                addr.sun_family = AF_UNIX;
                strncpy(addr.sun_path, test_socket_path_.c_str(), sizeof(addr.sun_path) - 1);

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
            std::this_thread::sleep_for(std::chrono::milliseconds(50)); // Small delay between connections
        }
        client_finished = true;
    });

    // Write multiple messages
    std::vector<Message> messages = {
        Message(AgentType::HEAP, STATUS, "Status message"),
        Message(AgentType::THREAD, DATA, "Thread data"),
        Message(AgentType::GC, ERROR, "GC error")
    };

    for (const auto& msg : messages) {
        EXPECT_TRUE(writer.writeMessage(msg));
    }

    EXPECT_TRUE(writer.flush());
    
    // Wait for client to finish
    client_thread.join();
    EXPECT_TRUE(client_finished);
    
    writer.close();

    // Verify received data contains all messages
    EXPECT_EQ(received_messages.size(), messages.size());
    for (size_t i = 0; i < messages.size() && i < received_messages.size(); ++i) {
        auto expected = messages[i].serialize();
        EXPECT_EQ(received_messages[i], expected);
    }
}

TEST_F(MessageWriterTest, UnixSocketWriterBasicFunctionality) {
    MessageWriter writer;

    EXPECT_FALSE(writer.isReady());
    EXPECT_TRUE(writer.initialize(test_socket_path_));
    EXPECT_TRUE(writer.isReady());

    // Start a client thread to connect and read from the socket
    std::vector<uint8_t> received_data;
    bool client_finished = false;
    
    std::thread client_thread([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Give server time to setup
        
        int client_socket = socket(AF_UNIX, SOCK_STREAM, 0);
        ASSERT_NE(client_socket, -1);

        struct sockaddr_un addr;
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, test_socket_path_.c_str(), sizeof(addr.sun_path) - 1);

        int connect_result = connect(client_socket, (struct sockaddr*)&addr, sizeof(addr));
        if (connect_result == 0) {
            uint8_t buffer[1024];
            ssize_t bytes_read = read(client_socket, buffer, sizeof(buffer));
            if (bytes_read > 0) {
                received_data.assign(buffer, buffer + bytes_read);
            }
        }

        close(client_socket);
        client_finished = true;
    });

    // Write message from server side
    std::string test_content = "Unix socket test";
    Message msg(AgentType::CLASS, DATA, test_content);
    
    EXPECT_TRUE(writer.writeMessage(msg));

    // Wait for client to finish
    client_thread.join();
    EXPECT_TRUE(client_finished);

    writer.close();
    EXPECT_FALSE(writer.isReady());

    // Verify received data
    auto expected_data = msg.serialize();
    EXPECT_EQ(received_data, expected_data);
}

TEST_F(MessageWriterTest, ErrorHandling) {
    MessageWriter writer;

    // Should fail to initialize with invalid path
    EXPECT_FALSE(writer.initialize("/invalid/path/that/does/not/exist"));
    EXPECT_FALSE(writer.isReady());
    EXPECT_FALSE(writer.getLastError().empty());

    // Should fail to write when not initialized
    Message msg(AgentType::NONE, STATUS, "test");
    EXPECT_FALSE(writer.writeMessage(msg));
}

TEST_F(MessageWriterTest, MessageSerialization) {
    // Test that different message types serialize correctly
    std::vector<Message> test_messages = {
        Message(AgentType::NONE, STATUS, ""),
        Message(AgentType::HEAP, DATA, "Heap analysis data"),
        Message(AgentType::GC, ERROR, "Garbage collection error"),
        Message(AgentType::THREAD, STATUS, "Thread dump complete"),
        Message(AgentType::CLASS, DATA, std::vector<uint8_t>{0x01, 0x02, 0x03, 0x04})
    };

    MessageWriter writer;
    EXPECT_TRUE(writer.initialize(test_socket_path_));

    // Start a client thread to connect and read from the socket
    std::vector<std::vector<uint8_t>> received_messages;
    bool client_finished = false;
    
    std::thread client_thread([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Give server time to setup
        
        // Since each writeMessage() call expects a separate connection,
        // we need to make multiple connections
        for (size_t i = 0; i < 5; ++i) {  // 5 messages
            int client_socket = socket(AF_UNIX, SOCK_STREAM, 0);
            if (client_socket != -1) {
                struct sockaddr_un addr;
                memset(&addr, 0, sizeof(addr));
                addr.sun_family = AF_UNIX;
                strncpy(addr.sun_path, test_socket_path_.c_str(), sizeof(addr.sun_path) - 1);

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
            std::this_thread::sleep_for(std::chrono::milliseconds(50)); // Small delay between connections
        }
        client_finished = true;
    });

    for (const auto& msg : test_messages) {
        EXPECT_TRUE(writer.writeMessage(msg));
    }

    writer.flush();
    
    // Wait for client to finish
    client_thread.join();
    EXPECT_TRUE(client_finished);
    
    writer.close();

    // Verify received data contains all messages
    EXPECT_EQ(received_messages.size(), test_messages.size());
    for (size_t i = 0; i < test_messages.size() && i < received_messages.size(); ++i) {
        auto expected = test_messages[i].serialize();
        EXPECT_EQ(received_messages[i], expected);
    }
}

}  // namespace jvmtool
