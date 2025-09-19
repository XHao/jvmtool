#include "metaspace.h"

#include <fcntl.h>      // for open()
#include <sys/types.h>  // for pid_t
#include <unistd.h>     // for getpid()

#include <cerrno>  // for errno (modernize-deprecated-headers)
#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>

#include "common.h"

namespace jvmtool {
// Static member definitions
bool MetaspaceStructs::_has_metaspace_structs = false;
void** MetaspaceStructs::_shared_metaspace_base_addr = nullptr;
void** MetaspaceStructs::_shared_metaspace_top_addr = nullptr;
void* MetaspaceStructs::_shared_metaspace_base = nullptr;
void* MetaspaceStructs::_shared_metaspace_top = nullptr;

// Run at agent load time
void MetaspaceStructs::init(CodeCache* libjvm) {
    if (libjvm != nullptr) {
        initMetaspaceOffsets();
    }
}

// Run when VM is initialized and JNI is available
void MetaspaceStructs::ready() {
    resolveMetaspaceOffsets();
}

void MetaspaceStructs::initMetaspaceOffsets() {
    uintptr_t entry = readSymbol("gHotSpotVMStructs");
    uintptr_t stride = readSymbol("gHotSpotVMStructEntryArrayStride");
    uintptr_t type_offset = readSymbol("gHotSpotVMStructEntryTypeNameOffset");
    uintptr_t field_offset = readSymbol("gHotSpotVMStructEntryFieldNameOffset");
    uintptr_t address_offset = readSymbol("gHotSpotVMStructEntryAddressOffset");

    if (entry != 0 && stride != 0) {
        for (;; entry += stride) {
            const char* type = *(const char**)(entry + type_offset);
            const char* field = *(const char**)(entry + field_offset);
            if (type == nullptr || field == nullptr) {
                break;
            }

            // Look for MetaspaceObj static fields
            if (strcmp(type, "MetaspaceObj") == 0) {
                if (strcmp(field, "_shared_metaspace_base") == 0) {
                    _shared_metaspace_base_addr = *(void***)(entry + address_offset);
                } else if (strcmp(field, "_shared_metaspace_top") == 0) {
                    _shared_metaspace_top_addr = *(void***)(entry + address_offset);
                }
            }
        }
    }
}

void MetaspaceStructs::resolveMetaspaceOffsets() {
    // Resolve shared metaspace boundaries
    if (_shared_metaspace_base_addr != nullptr) {
        _shared_metaspace_base = *_shared_metaspace_base_addr;
    }

    if (_shared_metaspace_top_addr != nullptr) {
        _shared_metaspace_top = *_shared_metaspace_top_addr;
    }

    // Check if we have valid metaspace structures
    _has_metaspace_structs =
        (_shared_metaspace_base_addr != nullptr && _shared_metaspace_top_addr != nullptr);
}

MetaspaceSAModule::MetaspaceSAModule() : AgentModule() {};

MemoryOpt MetaspaceSAModule::parseOptions(std::unordered_map<std::string, std::string>& options) {
    MemoryOpt opt;

    opt.type = options["task_type"];
    if (opt.type.empty()) {
        throw std::invalid_argument("Missing required parameter: task_type");
    }

    if (opt.type != "metaspace" && opt.type != "heap" && opt.type != "gc" && opt.type != "all") {
        throw std::invalid_argument("Unsupported task_type: " + opt.type +
                                    ". Supported types: metaspace, heap, gc, all");
    }

    opt.interval = parseInt(options, "interval", 5);
    opt.duration = parseInt(options, "duration", 30);

    return opt;
}

jvmtiError MetaspaceSAModule::initialize(JavaVM* java_vm, jvmtiEnv* jvmti) {
    jvmtiError init_err = AgentModule::initialize(java_vm, jvmti);
    if (init_err != JVMTI_ERROR_NONE) {
        return init_err;
    }
    MetaspaceStructs::init(VMStructs::libjvm());
    MetaspaceStructs::ready();
}

jint MetaspaceSAModule::onAttach(std::unordered_map<std::string, std::string>& options) {
    if (writeReady() != JNI_OK) {
        return JNI_ERR;
    }

    if (state_ == ModuleState::ANALYZING) {
        return JNI_ERR;
    }

    try {
        monitor_thread_ = std::thread(&MetaspaceSAModule::monitorMemory, this, parseOptions(options));
        state_ = ModuleState::ANALYZING;
        return JNI_OK;
    } catch (...) {
    }
    return JNI_ERR;
}

MetaspaceSAModule::~MetaspaceSAModule() {
    try {
        if (monitor_thread_.joinable()) {
            monitor_thread_.join();
        }
    } catch (...) {
    }
}

void MetaspaceSAModule::monitorMemory(const MemoryOpt& opt) {
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

bool MetaspaceSAModule::analyzeMetaspace(int fd) {
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
            Message warning(
                agentType(), STATUS,
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

Message MetaspaceSAModule::collectMetaspaceStatistics() const {
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
        auto memoryModule = new MetaspaceSAModule();
        AgentManager::instance().registerModule(memoryModule);
        std::cerr << "[Native SA] Memory SA module registered successfully" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[Native SA] Failed to register memory SA module: " << e.what() << std::endl;
    }
}

}  // extern "C"

}  // namespace jvmtool
