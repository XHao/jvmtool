#include "agent.h"

#include <algorithm>
#include <exception>
#include <mutex>

AgentManager& AgentManager::instance() {
    static AgentManager mgr;
    return mgr;
}

void AgentManager::registerModule(AgentModule* module) {
    const std::lock_guard<std::mutex> lock(modules_mutex_);
    // Check if module is already registered to avoid duplicates
    if (std::find(modules_.begin(), modules_.end(), module) == modules_.end()) {
        modules_.push_back(module);
    }
}

void AgentManager::onAttach(JavaVM* java_vm, jvmtiEnv* jvmti, const char* options) {
    const std::lock_guard<std::mutex> lock(modules_mutex_);
    for (auto* module : modules_) {
        try {
            module->onAttach(java_vm, jvmti, options);
        } catch (const std::exception& exception) {
            // Log error but continue with other modules
            // Could add proper logging here
            static_cast<void>(exception);  // Suppress unused variable warning
        } catch (...) {
            // Catch all other exceptions
        }
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