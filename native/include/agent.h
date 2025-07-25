#pragma once
#include <jni.h>
#include <jvmti.h>

#include <mutex>
#include <vector>

class AgentModule {
  public:
    virtual ~AgentModule() = default;
    virtual void onAttach(JavaVM* java_vm, jvmtiEnv* jvmti, const char* options) = 0;

    AgentModule(const AgentModule&) = delete;
    AgentModule& operator=(const AgentModule&) = delete;
    AgentModule(AgentModule&&) = delete;
    AgentModule& operator=(AgentModule&&) = delete;

  protected:
    AgentModule() = default;
};

class AgentManager {
  public:
    static AgentManager& instance();
    void registerModule(AgentModule* module);
    void onAttach(JavaVM* java_vm, jvmtiEnv* jvmti, const char* options);

  private:
    std::vector<AgentModule*> modules_;
    std::mutex modules_mutex_;

    AgentManager() = default;
    ~AgentManager() = default;

  public:
    AgentManager(const AgentManager&) = delete;
    AgentManager& operator=(const AgentManager&) = delete;
    AgentManager(AgentManager&&) = delete;
    AgentManager& operator=(AgentManager&&) = delete;
};