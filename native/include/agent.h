#pragma once
#include <jni.h>
#include <jvmti.h>

#include <mutex>
#include <string>
#include <unordered_map>

#include "writer.h"

namespace jvmtool {

void logJvmtiError(jvmtiError error, const char* context = nullptr);

struct JvmtiErrorInfo {
    const char* name;
    const char* description;

    constexpr JvmtiErrorInfo(const char* n, const char* d) : name(n), description(d) {}
};

class AgentModule {
  public:
    virtual ~AgentModule() = default;

    virtual jvmtiError initialize(JavaVM* java_vm, jvmtiEnv* jvmti);
    [[nodiscard]] bool isInitialized() const;

    virtual jint onAttach(const char* options) = 0;

    [[nodiscard]] virtual const char* getName() const = 0;

    AgentModule(const AgentModule&) = delete;
    AgentModule& operator=(const AgentModule&) = delete;
    AgentModule(AgentModule&&) = delete;
    AgentModule& operator=(AgentModule&&) = delete;

  protected:
    AgentModule() = default;

    jvmtiEnv* jvmti_ = nullptr;
    JavaVM* vm_ = nullptr;
    jrawMonitorID module_monitor_ = nullptr;

    // RAII-style monitor lock helper
    class MonitorLock {
      public:
        explicit MonitorLock(AgentModule* module);
        ~MonitorLock();

        [[nodiscard]] bool isLocked() const {
            return is_locked_;
        }

        MonitorLock(const MonitorLock&) = delete;
        MonitorLock& operator=(const MonitorLock&) = delete;

      private:
        jvmtiEnv* jvmti_;
        jrawMonitorID monitor_;
        bool is_locked_;
    };
};

class AgentManager {
  public:
    static AgentManager& instance();
    void registerModule(AgentModule* module);
    jint onAttach(JavaVM* java_vm, jvmtiEnv* jvmti, const char* options);

  private:
    std::unordered_map<std::string, AgentModule*> modules_;
    std::mutex modules_mutex_;
    bool inited_{false};

    AgentManager() = default;
    ~AgentManager() = default;

  public:
    AgentManager(const AgentManager&) = delete;
    AgentManager& operator=(const AgentManager&) = delete;
    AgentManager(AgentManager&&) = delete;
    AgentManager& operator=(AgentManager&&) = delete;
};

}  // namespace jvmtool