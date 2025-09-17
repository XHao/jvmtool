#include "agent.h"

#include <dlfcn.h>

#include <exception>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include "library_loader.h"
#include "metaspace_structs.h"
#include "vm/codeCache.h"
#include "vm/vm.h"

namespace jvmtool {

constexpr JvmtiErrorInfo getJvmtiErrorInfo(jvmtiError error) {
    switch (error) {
        case JVMTI_ERROR_NULL_POINTER:
            return {"NULL_POINTER", "Pointer parameter is NULL"};
        case JVMTI_ERROR_OUT_OF_MEMORY:
            return {"OUT_OF_MEMORY", "Insufficient memory"};
        case JVMTI_ERROR_ACCESS_DENIED:
            return {"ACCESS_DENIED", "Current thread does not own the monitor"};
        case JVMTI_ERROR_WRONG_PHASE:
            return {"WRONG_PHASE", "Function cannot be called during current phase"};
        case JVMTI_ERROR_INTERNAL:
            return {"INTERNAL", "Internal JVM error"};
        case JVMTI_ERROR_INVALID_ENVIRONMENT:
            return {"INVALID_ENVIRONMENT", "JVMTI environment is invalid"};
        case JVMTI_ERROR_THREAD_NOT_SUSPENDED:
            return {"THREAD_NOT_SUSPENDED", "Thread is not suspended"};
        case JVMTI_ERROR_THREAD_SUSPENDED:
            return {"THREAD_SUSPENDED", "Thread is already suspended"};
        case JVMTI_ERROR_THREAD_NOT_ALIVE:
            return {"THREAD_NOT_ALIVE", "Thread is not alive"};
        case JVMTI_ERROR_CLASS_NOT_PREPARED:
            return {"CLASS_NOT_PREPARED", "Class is not yet prepared"};
        case JVMTI_ERROR_NO_MORE_FRAMES:
            return {"NO_MORE_FRAMES", "No more frames on the call stack"};
        case JVMTI_ERROR_OPAQUE_FRAME:
            return {"OPAQUE_FRAME", "Information about the frame is not available"};
        case JVMTI_ERROR_DUPLICATE:
            return {"DUPLICATE", "Item already exists"};
        case JVMTI_ERROR_NOT_FOUND:
            return {"NOT_FOUND", "Desired element was not found"};
        case JVMTI_ERROR_NOT_MONITOR_OWNER:
            return {"NOT_MONITOR_OWNER", "Current thread does not own the monitor"};
        case JVMTI_ERROR_INTERRUPT:
            return {"INTERRUPT", "Call has been interrupted before completion"};
        case JVMTI_ERROR_UNMODIFIABLE_CLASS:
            return {"UNMODIFIABLE_CLASS", "Class cannot be modified"};
        case JVMTI_ERROR_NOT_AVAILABLE:
            return {"NOT_AVAILABLE", "Functionality is not available in this version"};
        case JVMTI_ERROR_MUST_POSSESS_CAPABILITY:
            return {"MUST_POSSESS_CAPABILITY", "Agent must possess the required capability"};
        case JVMTI_ERROR_INVALID_THREAD:
            return {"INVALID_THREAD", "Thread parameter is invalid"};
        case JVMTI_ERROR_INVALID_FIELDID:
            return {"INVALID_FIELDID", "Field ID is invalid"};
        case JVMTI_ERROR_INVALID_METHODID:
            return {"INVALID_METHODID", "Method ID is invalid"};
        case JVMTI_ERROR_INVALID_LOCATION:
            return {"INVALID_LOCATION", "Location is invalid"};
        case JVMTI_ERROR_INVALID_OBJECT:
            return {"INVALID_OBJECT", "Object parameter is invalid"};
        case JVMTI_ERROR_INVALID_CLASS:
            return {"INVALID_CLASS", "Class parameter is invalid"};
        case JVMTI_ERROR_TYPE_MISMATCH:
            return {"TYPE_MISMATCH", "Variable type does not match requested type"};
        case JVMTI_ERROR_NATIVE_METHOD:
            return {"NATIVE_METHOD", "Requested information is not available for native method"};
        case JVMTI_ERROR_CLASS_LOADER_UNSUPPORTED:
            return {"CLASS_LOADER_UNSUPPORTED", "Class loader does not support this operation"};
        case JVMTI_ERROR_ILLEGAL_ARGUMENT:
            return {"ILLEGAL_ARGUMENT", "Illegal argument"};
        default:
            return {"UNKNOWN_ERROR", "Undefined JVMTI error"};
    }
}

void logJvmtiError(jvmtiError error, const char* context) {
    if (error == JVMTI_ERROR_NONE) {
        return;  // No error to log
    }

    const auto [error_name, error_description] = getJvmtiErrorInfo(error);

    std::cerr << "[JVMTI Error] " << error_name << " in " << (context ? context : "Unknown context")
              << ": " << error_description << " (" << error << ")" << std::endl;
}

AgentModule::~AgentModule() {
    try {
        if (writer_ && writer_->isReady()) {
            writer_->close();
        }

        if (jvmti_ != nullptr && module_monitor_ != nullptr) {
            jvmtiError err = jvmti_->DestroyRawMonitor(module_monitor_);
            if (err != JVMTI_ERROR_NONE) {
                logJvmtiError(err, "AgentModule::~AgentModule - Failed to destroy monitor");
            }
        }
    } catch (...) {
    }
}

jvmtiError AgentModule::initialize(JavaVM* java_vm, jvmtiEnv* jvmti) {
    if (java_vm == nullptr || jvmti == nullptr) {
        return JVMTI_ERROR_NULL_POINTER;
    }

    jvmti_ = jvmti;
    vm_ = java_vm;

    return jvmti_->CreateRawMonitor(getName(), &module_monitor_);
}

bool AgentModule::isInitialized() const {
    return jvmti_ != nullptr && vm_ != nullptr && module_monitor_ != nullptr;
}

std::unordered_map<std::string, std::string> AgentManager::parseOptions(const char* options) {
    std::unordered_map<std::string, std::string> result;

    if (options == nullptr) {
        return result;
    }

    std::string opts(options);
    size_t pos = 0;

    while (pos < opts.length()) {
        size_t next = opts.find(',', pos);
        if (next == std::string::npos) {
            next = opts.length();
        }

        std::string param = opts.substr(pos, next - pos);
        size_t eq = param.find('=');

        if (eq != std::string::npos) {
            std::string key = param.substr(0, eq);
            std::string value = param.substr(eq + 1);
            result[key] = value;
        }

        pos = next + 1;
    }

    int interval = 0;
    auto it = result.find("interval");
    if (it != result.end()) {
        try {
            interval = std::atoi(it->second.c_str());
        } catch (const std::exception& e) {
            throw std::invalid_argument("Invalid interval parameter: " + std::string(e.what()));
        }
        if (interval <= 0) {
            throw std::invalid_argument("Interval must be a positive integer");
        }
    } else {
        interval = 5;  // default
    }

    it = result.find("duration");
    if (it != result.end()) {
        int duration = 0;
        try {
            duration = std::atoi(it->second.c_str());
        } catch (const std::exception& e) {
            throw std::invalid_argument("Invalid duration parameter: " + std::string(e.what()));
        }
        if (duration <= 0) {
            throw std::invalid_argument("Duration must be a positive integer");
        }
        if (duration < interval) {
            throw std::invalid_argument("Duration must be greater than interval");
        }
    } else {
        result["duration"] = "30";  // default
    }

    return result;
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

AgentManager::~AgentManager() {
    cleanup();
}

void AgentManager::cleanup() {
    const std::lock_guard<std::mutex> lock(modules_mutex_);
    for (auto& [name, module] : modules_) {
        delete module;
    }
    modules_.clear();
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
    const std::lock_guard<std::mutex> lock(modules_mutex_);

    if (!inited_) {
        VM::init(java_vm, jvmti);

        CodeCache* libjvm = LibraryLoader::findLibraryByName(
#ifdef __APPLE__
            "libjvm.dylib"
#elif __linux__
            "libjvm.so"
#else
            "libjvm"
#endif
        );
        if (libjvm != nullptr) {
            VMStructs::init(libjvm);
        } else {
            std::cerr << "[Native SA] Warning: failed to construct CodeCache for libjvm; symbol "
                         "lookups may be limited"
                      << std::endl;
        }

        for (auto it = modules_.begin(); it != modules_.end();) {
            const auto& [name, module] = *it;
            jvmtiError init_err = module->initialize(java_vm, jvmti);
            if (init_err != JVMTI_ERROR_NONE) {
                logJvmtiError(init_err, ("AgentModule::initialize - Module '" + name +
                                         "' initialization failed")
                                            .c_str());
                it = modules_.erase(it);
            } else {
                ++it;
            }
        }
        inited_ = true;
    }

    std::unordered_map<std::string, std::string> opts = parseOptions(options);
    std::string target_module = opts["analysis"];

    auto it = modules_.find(target_module);
    if (it != modules_.end() && it->second != nullptr) {
        const auto& [_, module] = *it;
        return module->onAttach(opts);
    }

    logJvmtiError(
        JVMTI_ERROR_ILLEGAL_ARGUMENT,
        ("AgentManager::onAttach - Module '" + target_module + "' is not loaded").c_str());
    return JNI_EINVAL;
}

}  // namespace jvmtool

JNIEXPORT jint JNICALL Agent_OnAttach(JavaVM* java_vm, char* options, void* /*reserved*/) {
    try {
        jvmtiEnv* jvmti = nullptr;
        const jint res = java_vm->GetEnv(reinterpret_cast<void**>(&jvmti), JVMTI_VERSION_1_2);
        if (res != JNI_OK || jvmti == nullptr) {
            return JNI_ERR;
        }

        return jvmtool::AgentManager::instance().onAttach(java_vm, jvmti, options);
    } catch (const std::exception& exception) {
        jvmtool::logJvmtiError(JVMTI_ERROR_INTERNAL, exception.what());
    }
    return JNI_ERR;
}