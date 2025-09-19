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

/**
 * Module state enumeration - common to all agent modules
 */
enum class ModuleState {
    IDLE,      // Module is idle, ready for new analysis
    ANALYZING  // Module is currently performing analysis
};

class AgentModule {
  public:
    virtual ~AgentModule();

    virtual jvmtiError initialize(JavaVM* java_vm, jvmtiEnv* jvmti);
    [[nodiscard]] bool isInitialized() const;

    virtual jint onAttach(std::unordered_map<std::string, std::string>&) = 0;

    virtual const char* getName() const = 0;

    virtual AgentType agentType() const = 0;

    AgentModule(const AgentModule&) = delete;
    AgentModule& operator=(const AgentModule&) = delete;
    AgentModule(AgentModule&&) = delete;
    AgentModule& operator=(AgentModule&&) = delete;

  protected:
    AgentModule() = default;

    jvmtiEnv* jvmti_ = nullptr;
    JavaVM* vm_ = nullptr;
    jrawMonitorID module_monitor_ = nullptr;

    std::unique_ptr<MessageWriter> writer_ = nullptr;
    ModuleState state_ = ModuleState::IDLE;

    void reset() {
        MonitorLock(this);
        state_ = ModuleState::IDLE;
    }

    jint writeReady();
    bool writeMessage(int fd, Message& msg);
    void writeAndClose(int fd, Message& msg);

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

    void cleanup();
    std::unordered_map<std::string, std::string> parseOptions(const char* options);

    AgentManager() = default;
    ~AgentManager();

  public:
    AgentManager(const AgentManager&) = delete;
    AgentManager& operator=(const AgentManager&) = delete;
    AgentManager(AgentManager&&) = delete;
    AgentManager& operator=(AgentManager&&) = delete;
};

}  // namespace jvmtool