#pragma once
#include <jni.h>
#include <jvmti.h>

#include <mutex>
#include <vector>

class AgentModule {
  public:
    virtual ~AgentModule() = default;
    virtual void onAttach(JavaVM* java_vm, jvmtiEnv* jvmti, const char* options) = 0;

    // Non-copyable and non-movable
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

    // Make it a singleton
    AgentManager() = default;
    ~AgentManager() = default;

  public:
    // Explicitly delete copy/move constructors and assignment operators
    AgentManager(const AgentManager&) = delete;
    AgentManager& operator=(const AgentManager&) = delete;
    AgentManager(AgentManager&&) = delete;
    AgentManager& operator=(AgentManager&&) = delete;
};