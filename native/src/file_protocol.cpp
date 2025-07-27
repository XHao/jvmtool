#include "file_protocol.h"
#include <chrono>
#include <iomanip>
#include <sstream>
#include <iostream>

namespace jvmtool {

FileProtocol::FileProtocol(const std::string& base_path) : base_path_(base_path) {
    // Base path is provided by jvmtool
}

FileProtocol::~FileProtocol() {
    // Cleanup can be handled by the host process
}

bool FileProtocol::sendStatus(StatusCode status, const std::string& message) {
    Message msg(MessageType::STATUS, status, message);
    return writeMessage(msg);
}

bool FileProtocol::sendError(const std::string& error_message) {
    Message msg(MessageType::ERROR, StatusCode::ERROR, error_message);
    return writeMessage(msg);
}

bool FileProtocol::sendData(const std::string& data, const std::string& data_type) {
    Message msg(MessageType::DATA, StatusCode::RUNNING, data);
    if (!data_type.empty()) {
        msg.metadata["data_type"] = data_type;
    }
    return writeMessage(msg);
}

bool FileProtocol::sendProgress(int percentage, const std::string& description) {
    Message msg(MessageType::PROGRESS, StatusCode::RUNNING, description);
    msg.metadata["percentage"] = std::to_string(percentage);
    return writeMessage(msg);
}

bool FileProtocol::sendResult(const std::string& result) {
    Message msg(MessageType::RESULT, StatusCode::COMPLETED, result);
    return writeMessage(msg);
}

std::string FileProtocol::getProtocolFile() const {
    return base_path_ + ".jt";
}

bool FileProtocol::writeMessage(const Message& message) {
    try {
        std::string filename = getProtocolFile();
        std::ofstream file(filename, std::ios::app);
        if (!file.is_open()) {
            return false;
        }
        
        file << formatMessage(message) << std::endl;
        file.flush();
        file.close();
        return true;
    } catch (...) {
        return false;
    }
}

std::string FileProtocol::getCurrentTimestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
}

std::string FileProtocol::formatMessage(const Message& message) const {
    std::stringstream ss;
    
    // Timestamp
    ss << "[" << getCurrentTimestamp() << "] ";
    
    // Message type
    switch (message.type) {
        case MessageType::STATUS: ss << "STATUS"; break;
        case MessageType::ERROR: ss << "ERROR"; break;
        case MessageType::DATA: ss << "DATA"; break;
        case MessageType::PROGRESS: ss << "PROGRESS"; break;
        case MessageType::RESULT: ss << "RESULT"; break;
        default: ss << "UNKNOWN"; break;
    }
    
    // Status code
    ss << " " << static_cast<int>(message.status);
    
    // Metadata
    for (const auto& pair : message.metadata) {
        ss << " " << pair.first << "=" << pair.second;
    }
    
    // Content
    if (!message.content.empty()) {
        ss << " | " << message.content;
    }
    
    return ss.str();
}

}
