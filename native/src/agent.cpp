#include "agent.h"

#include <unistd.h>

#include <exception>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include "file_protocol.h"

namespace jvmtool {

void logJvmtiError(jvmtiError error, const char* context) {
    if (error == JVMTI_ERROR_NONE) {
        return;  // No error to log
    }

    std::cerr << "[JVMTI Error] ";
    if (context != nullptr) {
        std::cerr << context << ": ";
    }
    std::cerr << "code(" << error << ")" << std::endl;
}

jvmtiError AgentModule::initialize(JavaVM* java_vm, jvmtiEnv* jvmti, const char* options) {
    if (java_vm == nullptr || jvmti == nullptr) {
        return JVMTI_ERROR_NULL_POINTER;
    }

    jvmti_ = jvmti;
    vm_ = java_vm;

    if (!initializeMonitor()) {
        return JVMTI_ERROR_INTERNAL;
    }

    return JVMTI_ERROR_NONE;
}

bool AgentModule::isInitialized() const {
    return jvmti_ != nullptr && vm_ != nullptr && module_monitor_ != nullptr;
}

bool AgentModule::initializeMonitor() {
    if (jvmti_ == nullptr) {
        logJvmtiError(JVMTI_ERROR_WRONG_PHASE,
                      "AgentModule::initializeMonitor - JVMTI not available");
        return false;
    }

    if (module_monitor_ != nullptr) {
        return true;  // Already initialized
    }

    jvmtiError err = jvmti_->CreateRawMonitor(getName(), &module_monitor_);
    if (err != JVMTI_ERROR_NONE) {
        logJvmtiError(err, "AgentModule::initializeMonitor - Failed to create raw monitor");
        return false;
    }
    return true;
}

// AgentModule::MonitorLock implementation
AgentModule::MonitorLock::MonitorLock(AgentModule* module)
    : jvmti_(module->jvmti_), monitor_(module->module_monitor_), is_locked_(false) {
    if (jvmti_ != nullptr && monitor_ != nullptr) {
        jvmtiError err = jvmti_->RawMonitorEnter(monitor_);
        if (err == JVMTI_ERROR_NONE) {
            is_locked_ = true;
        } else {
            logJvmtiError(err, "MonitorLock::MonitorLock - Failed to enter monitor");
        }
    }
}

AgentModule::MonitorLock::~MonitorLock() {
    if (is_locked_ && jvmti_ != nullptr && monitor_ != nullptr) {
        jvmtiError err = jvmti_->RawMonitorExit(monitor_);
        if (err != JVMTI_ERROR_NONE) {
            logJvmtiError(err, "MonitorLock::~MonitorLock - Failed to exit monitor");
        }
    }
}

// AgentManager implementation
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
    } else {
        logJvmtiError(JVMTI_ERROR_DUPLICATE,
                      "AgentManager::registerModule - Duplicate module registered");
    }
}

jint AgentManager::onAttach(JavaVM* java_vm, jvmtiEnv* jvmti, const char* options) {
    try {
        const std::lock_guard<std::mutex> lock(modules_mutex_);
        if (!inited_) {
            for (auto& [name, module] : modules_) {
                jvmtiError init_err = module->initialize(java_vm, jvmti, options);
                if (init_err != JVMTI_ERROR_NONE) {
                    logJvmtiError(init_err, ("AgentManager::onAttach - Module '" + name +
                                             "' initialization failed")
                                                .c_str());
                }
            }
            inited_ = true;
        }

        std::string target_module;

        if (options != nullptr) {
            std::string opts(options);
            size_t analysis_pos = opts.find("analysis=");
            if (analysis_pos != std::string::npos) {
                size_t start = analysis_pos + 9;  // length of "analysis="
                size_t end = opts.find(',', start);
                if (end == std::string::npos) {
                    end = opts.length();
                }
                target_module = opts.substr(start, end - start);
            }
        }

        if (target_module.empty()) {
            logJvmtiError(JVMTI_ERROR_ILLEGAL_ARGUMENT,
                          "AgentManager::onAttach - Module name is empty");
            return JNI_EINVAL;
        }

        auto it = modules_.find(target_module);
        if (it != modules_.end() && it->second != nullptr) {
            AgentModule* module = it->second;
            if (!module->isInitialized()) {
                return JNI_ERR;
            }
            module->onAttach(options);
            return JNI_OK;
        } else {
            logJvmtiError(
                JVMTI_ERROR_ILLEGAL_ARGUMENT,
                ("AgentManager::onAttach - Module '" + target_module + "' is not loaded").c_str());
            return JNI_EINVAL;
        }
    } catch (const std::exception& exception) {
        logJvmtiError(JVMTI_ERROR_INTERNAL, exception.what());
    }
    return JNI_ERR;
}

}  // namespace jvmtool

JNIEXPORT jint JNICALL Agent_OnAttach(JavaVM* java_vm, char* options, void* /*reserved*/) {
    jvmtiEnv* jvmti = nullptr;
    const jint res = java_vm->GetEnv(reinterpret_cast<void**>(&jvmti), JVMTI_VERSION_1_2);
    if (res != JNI_OK || jvmti == nullptr) {
        return JNI_ERR;
    }
    return jvmtool::AgentManager::instance().onAttach(java_vm, jvmti, options);
}

JNIEXPORT void JNICALL Agent_OnUnload(JavaVM* java_vm) {
    static_cast<void>(java_vm);  // Suppress unused parameter warning
}