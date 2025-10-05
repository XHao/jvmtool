#pragma once

#include <jni.h>
#include <jvmti.h>
#include <sys/types.h>  // for pid_t

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <vector>
#include <thread>

#include "agent.h"
#include "message.h"
#include "vm/vmStructs.h"
#include "writer.h"

namespace jvmtool {

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

class MetaspaceStructs : public VMStructs {
    protected:
    // Metaspace-related flags
    static bool _has_metaspace_structs;

    // MetaspaceObj static fields
    static void** _shared_metaspace_base_addr;
    static void** _shared_metaspace_top_addr;
    static void* _shared_metaspace_base;
    static void* _shared_metaspace_top;

    // Initialize metaspace-specific offsets
    static void initMetaspaceOffsets();
    static void resolveMetaspaceOffsets();

  public:
    // Initialization methods
    static void init(CodeCache* libjvm);
    static void ready();

    struct ClassLoaderStats {
        std::string loader_name;
        std::string module_name;
        size_t total_bytes{0};
        size_t klass_bytes{0};
        size_t method_bytes{0};
        size_t constant_pool_bytes{0};
    };

    static bool collectClassLoaderStats(JNIEnv* env, std::vector<ClassLoaderStats>& out);

    // Metaspace functionality checks
    static bool hasMetaspaceStructs() {
        return _has_metaspace_structs;
    }

    // Shared metaspace boundary access
    static void* sharedMetaspaceBase() {
        return _shared_metaspace_base;
    }

    static void* sharedMetaspaceTop() {
        return _shared_metaspace_top;
    }

    // Check if an object is in shared metaspace
    static bool isSharedMetaspaceObject(const void* obj) {
        if (_shared_metaspace_base == nullptr || _shared_metaspace_top == nullptr) {
            return false;
        }
        return obj >= _shared_metaspace_base && obj < _shared_metaspace_top;
    }

    // Get shared metaspace size
    static size_t sharedMetaspaceSize() {
        if (_shared_metaspace_base == nullptr || _shared_metaspace_top == nullptr) {
            return 0;
        }
        return static_cast<char*>(_shared_metaspace_top) -
               static_cast<char*>(_shared_metaspace_base);
    }

    // Check if shared metaspace is available
    static bool hasSharedMetaspace() {
        return _shared_metaspace_base != nullptr && _shared_metaspace_top != nullptr;
    }
};

}  // namespace jvmtool
