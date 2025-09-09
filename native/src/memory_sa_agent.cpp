#include <fcntl.h>      // for open()
#include <sys/types.h>  // for pid_t
#include <unistd.h>     // for getpid()

#include <cerrno>  // for errno (modernize-deprecated-headers)
#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>

#include "memory.h"
#include "metaspace_structs.h"

namespace jvmtool {

// Implementation of MemorySAModule methods

MemorySAModule::MemorySAModule() : AgentModule() {};

MemoryOpt MemorySAModule::parse(std::unordered_map<std::string, std::string>& options) {
    MemoryOpt opt;

    opt.type = options["task_type"];
    if (opt.type.empty()) {
        throw std::invalid_argument("Missing required parameter: task_type");
    }

    if (opt.type != "metaspace" && opt.type != "heap" && opt.type != "gc" && opt.type != "all") {
        throw std::invalid_argument("Unsupported task_type: " + opt.type +
                                    ". Supported types: metaspace, heap, gc, all");
    }

    opt.interval = std::atoi(options["interval"].c_str());
    opt.duration = std::atoi(options["duration"].c_str());

    return opt;
}

jint MemorySAModule::onAttach(std::unordered_map<std::string, std::string>& options) {
    if (!writer_) {
        writer_ = std::make_unique<MessageWriter>();
        if (!writer_->initialize("/tmp/jvmtool_memory_" + std::to_string(getpid()) + ".sock")) {
            writer_ = nullptr;
            return JNI_ERR;
        }
    }

    if (state_ == ModuleState::ANALYZING) {
        return JNI_ERR;
    }

    try {
        monitor_thread_ = std::thread(&MemorySAModule::monitorMemory, this, parse(options));
        state_ = ModuleState::ANALYZING;
        return JNI_OK;
    } catch (...) {
    }
    return JNI_ERR;
}

MemorySAModule::~MemorySAModule() {
    try {
        if (monitor_thread_.joinable()) {
            monitor_thread_.join();
        }
    } catch (...) {
    }
}

bool MemorySAModule::writeMessage(int fd, Message& msg) {
    if (!writer_->writeMessage(fd, msg)) {
        reset();
        return false;
    }
    return true;
}

void MemorySAModule::writeAndClose(int fd, Message& msg) {
    writer_->writeMessage(fd, msg);
    reset();
}

void MemorySAModule::monitorMemory(const MemoryOpt& opt) {
    try {
        auto fd = ClosableFd(writer_->waitForClient());

        JNIEnv* env = nullptr;
        if (vm_->AttachCurrentThread(reinterpret_cast<void**>(&env), nullptr) != JNI_OK) {
            Message msg(agentType(), ERROR, "[Native SA] Failed to attach monitoring thread");
            writeAndClose(fd, msg);
            return;
        }
        auto start_time = std::chrono::steady_clock::now();
        auto duration_ms = std::chrono::seconds(opt.duration);

        while (analyzeMetaspace(fd)) {
            auto current_time = std::chrono::steady_clock::now();
            if (current_time - start_time >= duration_ms) {
                Message data(agentType(), STATUS, "[Native SA] === End Analysis ===");
                writeAndClose(fd, data);
                break;
            }
            std::this_thread::sleep_for(std::chrono::seconds(opt.interval));
        }
        vm_->DetachCurrentThread();
    } catch (...) {
    }
}

bool MemorySAModule::analyzeMetaspace(int fd) {
    try {
        const Message header(agentType(), DATA, "[Native SA] === Metaspace Analysis ===");
        if (!writer_->writeMessage(fd, header)) {
            return false;
        }

        auto data = collectMetaspaceStatistics();
        if (!writeMessage(fd, data)) {
            return false;
        }
        
        // If metaspace structures are not available, don't continue the loop
        if (!MetaspaceStructs::hasMetaspaceStructs()) {
            Message warning(agentType(), STATUS, 
                "[Native SA] Metaspace analysis not supported on this JVM - ending analysis");
            writeAndClose(fd, warning);
            return false;
        }
        
        return true;
    } catch (const std::exception& e) {
        Message err(agentType(), ERROR,
                    "[Native SA] Error during metaspace analysis: " + std::string(e.what()));
        writeAndClose(fd, err);
    }
    return false;
}

Message MemorySAModule::collectMetaspaceStatistics() const {
    std::stringstream ss;

    ss << "[Native SA] Metaspace Statistics:\n";

    // Check if metaspace structures are available
    if (!MetaspaceStructs::hasMetaspaceStructs()) {
        ss << "  Metaspace structures not available\n";
        return {agentType(), DATA, ss.str()};
    }

    ss << "  Metaspace structures available: Yes\n";

    // Check shared metaspace information
    if (MetaspaceStructs::hasSharedMetaspace()) {
        void* base = MetaspaceStructs::sharedMetaspaceBase();
        void* top = MetaspaceStructs::sharedMetaspaceTop();
        size_t size = MetaspaceStructs::sharedMetaspaceSize();

        ss << "  Shared metaspace available: Yes\n";
        ss << "  Shared metaspace base: 0x" << std::hex << base << "\n";
        ss << "  Shared metaspace top:  0x" << std::hex << top << "\n";
        ss << "  Shared metaspace size: " << std::dec << size << " bytes (" << (size / 1024 / 1024)
           << " MB)\n";

        // Calculate usage percentage (this is a simplified calculation)
        if (size > 0) {
            ss << "  Shared metaspace range: [0x" << std::hex << base << " - 0x" << std::hex << top
               << ")\n";
        }
    } else {
        ss << "  Shared metaspace available: No (CDS may not be enabled)\n";
    }

    return {agentType(), DATA, ss.str()};
}

extern "C" {

// Auto-register when library is loaded (Unix/Linux)
static __attribute__((constructor)) void initModule() {
    try {
        auto memoryModule = new MemorySAModule();
        AgentManager::instance().registerModule(memoryModule);
        std::cerr << "[Native SA] Memory SA module registered successfully" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[Native SA] Failed to register memory SA module: " << e.what() << std::endl;
    }
}

}  // extern "C"

}  // namespace jvmtool
