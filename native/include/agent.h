#pragma once
#include <jvmti.h>
#include <jni.h>
#include <vector>

class AgentModule {
public:
    virtual ~AgentModule() {}
    virtual void onAttach(JavaVM* vm, jvmtiEnv* jvmti, const char* options) = 0;
};

class AgentManager {
public:
    static AgentManager& instance();
    void registerModule(AgentModule* module);
    void onAttach(JavaVM* vm, jvmtiEnv* jvmti, const char* options);
private:
    std::vector<AgentModule*> modules_;
};