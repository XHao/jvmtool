#include "agent.h"

AgentManager& AgentManager::instance() {
    static AgentManager mgr;
    return mgr;
}

void AgentManager::registerModule(AgentModule* module) {
    modules_.push_back(module);
}

void AgentManager::onAttach(JavaVM* vm, jvmtiEnv* jvmti, const char* options) {
    for (auto* m : modules_) m->onAttach(vm, jvmti, options);
}

JNIEXPORT jint JNICALL Agent_OnAttach(JavaVM* vm, char* options, void* reserved) {
    jvmtiEnv* jvmti;
    jint res = vm->GetEnv((void**)&jvmti, JVMTI_VERSION_1_2);
    if (res != JNI_OK || jvmti == nullptr) return JNI_ERR;
    AgentManager::instance().onAttach(vm, jvmti, options);
    return JNI_OK;
}

JNIEXPORT void JNICALL Agent_OnUnload(JavaVM* vm) {
}