#pragma once
#include <jni.h>
#include <jvmti.h>

#include <mutex>
#include <unordered_map>
#include <string>

class AgentModule {
  public:
    virtual ~AgentModule() = default;
    virtual void onAttach(JavaVM* java_vm, jvmtiEnv* jvmti, const char* options) = 0;
    virtual const char* getName() const = 0;

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
    std::unordered_map<std::string, AgentModule*> modules_;
    std::mutex modules_mutex_;

    AgentManager() = default;
    ~AgentManager() = default;

  public:
    AgentManager(const AgentManager&) = delete;
    AgentManager& operator=(const AgentManager&) = delete;
    AgentManager(AgentManager&&) = delete;
    AgentManager& operator=(AgentManager&&) = delete;
};