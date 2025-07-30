#include "agent.h"

#include <unistd.h>

#include <exception>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include "file_protocol.h"

AgentManager& AgentManager::instance() {
    static AgentManager mgr;
    return mgr;
}

void AgentManager::registerModule(AgentModule* module) {
    if (module == nullptr) {
        return;
    }

    const std::lock_guard<std::mutex> lock(modules_mutex_);
    const std::string module_name = module->getName();

    if (modules_.find(module_name) == modules_.end()) {
        modules_[module_name] = module;
    }
}

void AgentManager::onAttach(JavaVM* java_vm, jvmtiEnv* jvmti, const char* options) {
    const std::lock_guard<std::mutex> lock(modules_mutex_);
    std::string target_module;
    std::string base_path;

    if (options != nullptr) {
        std::string opts(options);

        // Parse analysis type
        size_t analysis_pos = opts.find("analysis=");
        if (analysis_pos != std::string::npos) {
            size_t start = analysis_pos + 9;  // length of "analysis="
            size_t end = opts.find(',', start);
            if (end == std::string::npos) {
                end = opts.length();
            }
            target_module = opts.substr(start, end - start);
        }

        // Parse base path for file communication
        size_t path_pos = opts.find("comm_path=");
        if (path_pos != std::string::npos) {
            size_t start = path_pos + 10;  // length of "comm_path="
            size_t end = opts.find(',', start);
            if (end == std::string::npos) {
                end = opts.length();
            }
            base_path = opts.substr(start, end - start);
        }
    }

    // Create file protocol instance for communication
    auto protocol = std::make_unique<jvmtool::FileProtocol>(base_path);

    if (target_module.empty()) {
        protocol->sendError("No analysis type specified in options");
        return;
    }

    // Find and attach only the specified module
    auto it = modules_.find(target_module);
    if (it != modules_.end() && it->second != nullptr) {
        try {
            protocol->sendStatus(jvmtool::StatusCode::RUNNING,
                                 "Attaching module: " + target_module);
            it->second->onAttach(java_vm, jvmti, options);
            protocol->sendStatus(jvmtool::StatusCode::SUCCESS,
                                 "Module '" + target_module + "' attached successfully");
        } catch (const std::exception& exception) {
            protocol->sendError("Module '" + target_module +
                                "' failed to attach: " + exception.what());
        } catch (...) {
            protocol->sendError("Module '" + target_module +
                                "' failed to attach: Unknown exception");
        }
    } else {
        protocol->sendError("Module '" + target_module + "' not found or not registered");
    }
}

JNIEXPORT jint JNICALL Agent_OnAttach(JavaVM* java_vm, char* options, void* /*reserved*/) {
    jvmtiEnv* jvmti = nullptr;
    const jint res = java_vm->GetEnv(reinterpret_cast<void**>(&jvmti), JVMTI_VERSION_1_2);
    if (res != JNI_OK || jvmti == nullptr) {
        return JNI_ERR;
    }
    AgentManager::instance().onAttach(java_vm, jvmti, options);
    return JNI_OK;
}

JNIEXPORT void JNICALL Agent_OnUnload(JavaVM* java_vm) {
    static_cast<void>(java_vm);  // Suppress unused parameter warning
}