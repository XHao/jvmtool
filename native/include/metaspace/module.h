#pragma once

#include <jni.h>
#include <jvmti.h>

#include <atomic>
#include <thread>

#include "agent.h"
#include "message.h"
#include "writer.h"

namespace jvmtool {

struct TaskOpt;

class MetaspaceSAModule : public AgentModule {
  private:
    std::thread monitor_thread_;
    std::atomic<bool> stop_{false};

  public:
    MetaspaceSAModule();
    ~MetaspaceSAModule() override;

    jvmtiError initialize(JavaVM* java_vm, jvmtiEnv* jvmti) override;
    jint onAttach(const TaskOpt& opt) override;

    const char* getName() const override {
        return "meta";
    }

    AgentType agentType() const override {
        return AgentType::METASPACE;
    }

  private:
    void monitor(const TaskOpt& opt);
    bool analyzeMetaspace(int fd, JNIEnv* env);
    Message collectMetaspaceStatistics(JNIEnv* env) const;
};

}  // namespace jvmtool
