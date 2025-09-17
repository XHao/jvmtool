#pragma once

#include <jni.h>
#include <jvmti.h>
#include <sys/types.h>  // for pid_t

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>

#include "agent.h"
#include "message.h"
#include "writer.h"

namespace jvmtool {

struct MemoryOpt {
    int interval;
    int duration;
    std::string type;
};

/**
 * Memory analysis module for monitoring JVM heap and memory pools
 * Provides real-time memory usage statistics and GC event tracking
 */
class MemorySAModule : public AgentModule {
  private:
    std::thread monitor_thread_;

  public:
    MemorySAModule();
    ~MemorySAModule() override;

    jvmtiError initialize(JavaVM* java_vm, jvmtiEnv* jvmti) override;
    jint onAttach(std::unordered_map<std::string, std::string>& options) override;

    const char* getName() const override {
        return "memory";
    }

    AgentType agentType() const override {
        return AgentType::HEAP;
    }

  private:
    static MemoryOpt parse(std::unordered_map<std::string, std::string>& options);

    void monitorMemory(const MemoryOpt& opt);
    bool analyzeMetaspace(int fd);
    Message collectMetaspaceStatistics() const;

    bool writeMessage(int fd, Message& msg);
    void writeAndClose(int fd, Message& msg);
};

/**
 * Memory usage statistics structure
 */
struct MemoryStats {
    jlong heap_used;
    jlong heap_committed;
    jlong heap_max;
    double heap_usage_percent;
    std::chrono::system_clock::time_point timestamp;

    MemoryStats() : heap_used(0), heap_committed(0), heap_max(0), heap_usage_percent(0.0) {
        timestamp = std::chrono::system_clock::now();
    }
};

/**
 * Memory pool information structure
 */
struct MemoryPoolInfo {
    std::string name;
    jlong used;
    jlong max;
    double usage_percent;

    MemoryPoolInfo() : used(0), max(0), usage_percent(0.0) {}
    MemoryPoolInfo(const std::string& pool_name, jlong pool_used, jlong pool_max)
        : name(pool_name), used(pool_used), max(pool_max) {
        usage_percent = (max > 0) ? (static_cast<double>(used) / max * 100.0) : 0.0;
    }
};

/**
 * GC event information structure
 */
struct GCEvent {
    enum Type { GC_START, GC_FINISH };

    Type type;
    std::chrono::system_clock::time_point timestamp;

    GCEvent(Type event_type) : type(event_type) {
        timestamp = std::chrono::system_clock::now();
    }
};

}  // namespace jvmtool
