#include <errno.h>   // for errno
#include <fcntl.h>   // for open()
#include <unistd.h>  // for getpid()

#include <atomic>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>

#include "../include/agent.h"

class MemorySAModule : public AgentModule {
  private:
    jvmtiEnv* jvmti_;
    JavaVM* vm_;
    std::atomic<bool> monitoring_;
    std::thread monitor_thread_;
    std::string output_file_;
    std::string analysis_type_;
    int duration_;
    std::string temp_output_file_;  // 临时输出文件
    std::string instance_id_;       // 实例标识符
    static std::atomic<int> instance_counter_;

  public:
    MemorySAModule() : jvmti_(nullptr), vm_(nullptr), monitoring_(false), duration_(30) {
        // Create unique instance ID
        int id = instance_counter_.fetch_add(1);
        instance_id_ = "SA_" + std::to_string(getpid()) + "_" + std::to_string(id);
    }

    void onAttach(JavaVM* java_vm, jvmtiEnv* jvmti, const char* options) override {
        // If already monitoring, stop previous monitoring first
        if (monitoring_.load()) {
            writeOutput("[Native SA] Stopping previous monitoring session...");
            monitoring_ = false;
            if (monitor_thread_.joinable()) {
                monitor_thread_.join();
            }
        }

        jvmti_ = jvmti;
        vm_ = java_vm;

        // Parse options
        parseOptions(options);

        // If no output file specified, create a temporary one
        if (output_file_.empty()) {
            temp_output_file_ = "/tmp/jvmtool_sa_" + std::to_string(getpid()) + ".log";
            output_file_ = temp_output_file_;  // Use temp file for all output
        }

        monitoring_ = true;

        // Enable memory-related capabilities - be more conservative
        jvmtiCapabilities caps = {};
        caps.can_generate_garbage_collection_events = 1;
        jvmtiError err = jvmti->AddCapabilities(&caps);
        if (err != JVMTI_ERROR_NONE) {
            writeOutput("[Native SA] Warning: Failed to add GC capabilities: " +
                        std::to_string(err));
        }

        // Set GC event callbacks - only if capabilities were added successfully
        if (err == JVMTI_ERROR_NONE) {
            jvmtiEventCallbacks callbacks = {};
            callbacks.GarbageCollectionStart = &onGCStart;
            callbacks.GarbageCollectionFinish = &onGCFinish;
            jvmti->SetEventCallbacks(&callbacks, sizeof(callbacks));

            // Enable events
            jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_GARBAGE_COLLECTION_START,
                                            nullptr);
            jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_GARBAGE_COLLECTION_FINISH,
                                            nullptr);
        }

        writeOutput("[Native SA] Memory SA Module loaded - JVMTI Agent [" + instance_id_ + "]");
        writeOutput("[Native SA] Output will be written to: " + output_file_);

        // If using temporary file, print the path to stderr so jvmtool can see it
        if (!temp_output_file_.empty()) {
            std::cerr << "JVMTOOL_TEMP_OUTPUT:" << temp_output_file_ << std::endl;
            std::cerr.flush();
        }

        // Start monitoring thread AFTER all setup is complete
        try {
            monitor_thread_ = std::thread(&MemorySAModule::monitorMemory, this);
        } catch (const std::exception& e) {
            writeOutput("[Native SA] Failed to start monitoring thread: " + std::string(e.what()));
        }
    }

    ~MemorySAModule() override {
        cleanup();
    }

    void cleanup() {
        static std::mutex cleanup_mutex;
        std::lock_guard<std::mutex> lock(cleanup_mutex);

        try {
            monitoring_ = false;
            if (monitor_thread_.joinable()) {
                monitor_thread_.join();
            }

            // If we created a temporary file, signal completion
            if (!temp_output_file_.empty()) {
                std::cerr << "JVMTOOL_ANALYSIS_COMPLETE:" << temp_output_file_ << std::endl;
                std::cerr.flush();
            }
        } catch (...) {
            // Suppress all exceptions in destructor
        }
    }

  private:
    void parseOptions(const char* options) {
        if (!options)
            return;

        std::string opts(options);
        size_t pos = 0;

        while (pos < opts.length()) {
            size_t next = opts.find(',', pos);
            if (next == std::string::npos)
                next = opts.length();

            std::string param = opts.substr(pos, next - pos);
            size_t eq = param.find('=');

            if (eq != std::string::npos) {
                std::string key = param.substr(0, eq);
                std::string value = param.substr(eq + 1);

                if (key == "analysis") {
                    analysis_type_ = value;
                } else if (key == "duration") {
                    duration_ = std::stoi(value);
                } else if (key == "output") {
                    output_file_ = value;
                }
            }

            pos = next + 1;
        }
    }

    void writeOutput(const std::string& message) {
        // Always write to file (either specified or temporary)
        std::ofstream file(output_file_, std::ios::app);
        if (file.is_open()) {
            auto now = std::chrono::system_clock::now();
            auto time_t = std::chrono::system_clock::to_time_t(now);
            file << "[" << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S") << "] "
                 << message << std::endl;
            file.close();
        }
    }
    void monitorMemory() {
        JNIEnv* env;
        if (vm_->AttachCurrentThread(reinterpret_cast<void**>(&env), nullptr) != JNI_OK) {
            writeOutput("[Native SA] Failed to attach monitoring thread");
            return;
        }

        auto start_time = std::chrono::steady_clock::now();
        auto duration_ms = std::chrono::seconds(duration_);

        writeOutput("[Native SA] Starting memory analysis for " + std::to_string(duration_) +
                    " seconds...");

        while (monitoring_.load()) {
            auto current_time = std::chrono::steady_clock::now();
            if (current_time - start_time >= duration_ms) {
                writeOutput("[Native SA] Analysis duration completed, stopping monitoring...");
                break;
            }

            if (analysis_type_ == "memory" || analysis_type_ == "all") {
                analyzeHeapMemory(env);
                analyzeMemoryPools(env);
            }

            std::this_thread::sleep_for(std::chrono::seconds(10));
        }

        writeOutput("[Native SA] Memory analysis completed");
        vm_->DetachCurrentThread();
    }

    void analyzeHeapMemory(JNIEnv* env) {
        // Analyze heap memory usage
        jclass memoryMXBeanClass = env->FindClass("java/lang/management/ManagementFactory");
        if (!memoryMXBeanClass) {
            env->ExceptionClear();
            return;
        }

        jmethodID getMemoryMXBean = env->GetStaticMethodID(memoryMXBeanClass, "getMemoryMXBean",
                                                           "()Ljava/lang/management/MemoryMXBean;");
        if (!getMemoryMXBean) {
            env->ExceptionClear();
            return;
        }

        jobject memoryBean = env->CallStaticObjectMethod(memoryMXBeanClass, getMemoryMXBean);
        if (!memoryBean) {
            env->ExceptionClear();
            return;
        }

        // Get heap usage information
        jclass memoryMXBeanInterface = env->FindClass("java/lang/management/MemoryMXBean");
        jmethodID getHeapMemoryUsage = env->GetMethodID(memoryMXBeanInterface, "getHeapMemoryUsage",
                                                        "()Ljava/lang/management/MemoryUsage;");

        if (getHeapMemoryUsage) {
            jobject heapUsage = env->CallObjectMethod(memoryBean, getHeapMemoryUsage);
            if (heapUsage) {
                jclass memoryUsageClass = env->FindClass("java/lang/management/MemoryUsage");
                jmethodID getUsed = env->GetMethodID(memoryUsageClass, "getUsed", "()J");
                jmethodID getMax = env->GetMethodID(memoryUsageClass, "getMax", "()J");
                jmethodID getCommitted = env->GetMethodID(memoryUsageClass, "getCommitted", "()J");

                if (getUsed && getMax && getCommitted) {
                    jlong used = env->CallLongMethod(heapUsage, getUsed);
                    jlong max = env->CallLongMethod(heapUsage, getMax);
                    jlong committed = env->CallLongMethod(heapUsage, getCommitted);

                    double usage_percent = max > 0 ? (double)used / max * 100.0 : 0.0;

                    auto now = std::chrono::system_clock::now();
                    auto time_t = std::chrono::system_clock::to_time_t(now);

                    std::ostringstream oss;
                    oss << "[Native SA] Heap Analysis at "
                        << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
                    writeOutput(oss.str());
                    writeOutput("  Used: " + formatBytes(used));
                    writeOutput("  Committed: " + formatBytes(committed));
                    writeOutput("  Max: " + formatBytes(max));

                    std::ostringstream usage_oss;
                    usage_oss << "  Usage: " << std::fixed << std::setprecision(2) << usage_percent
                              << "%";
                    writeOutput(usage_oss.str());
                }
            }
        }

        env->ExceptionClear();
    }

    void analyzeMemoryPools(JNIEnv* env) {
        // Analyze memory pools
        jclass memoryMXBeanClass = env->FindClass("java/lang/management/ManagementFactory");
        if (!memoryMXBeanClass) {
            env->ExceptionClear();
            return;
        }

        jmethodID getMemoryPoolMXBeans =
            env->GetStaticMethodID(memoryMXBeanClass, "getMemoryPoolMXBeans", "()Ljava/util/List;");
        if (!getMemoryPoolMXBeans) {
            env->ExceptionClear();
            return;
        }

        jobject poolList = env->CallStaticObjectMethod(memoryMXBeanClass, getMemoryPoolMXBeans);
        if (poolList) {
            writeOutput("[Native SA] Memory Pool Analysis:");

            jclass listClass = env->FindClass("java/util/List");
            jmethodID sizeMethod = env->GetMethodID(listClass, "size", "()I");
            jmethodID getMethod = env->GetMethodID(listClass, "get", "(I)Ljava/lang/Object;");

            if (sizeMethod && getMethod) {
                jint poolCount = env->CallIntMethod(poolList, sizeMethod);

                for (jint i = 0; i < poolCount; i++) {
                    jobject pool = env->CallObjectMethod(poolList, getMethod, i);
                    if (pool) {
                        analyzeMemoryPool(env, pool);
                    }
                }
            }
        }

        env->ExceptionClear();
    }

    void analyzeMemoryPool(JNIEnv* env, jobject pool) {
        jclass poolClass = env->FindClass("java/lang/management/MemoryPoolMXBean");
        if (!poolClass) {
            env->ExceptionClear();
            return;
        }

        jmethodID getNameMethod = env->GetMethodID(poolClass, "getName", "()Ljava/lang/String;");
        jmethodID getUsageMethod =
            env->GetMethodID(poolClass, "getUsage", "()Ljava/lang/management/MemoryUsage;");

        if (getNameMethod && getUsageMethod) {
            jstring nameStr = static_cast<jstring>(env->CallObjectMethod(pool, getNameMethod));
            jobject usage = env->CallObjectMethod(pool, getUsageMethod);

            if (nameStr && usage) {
                const char* name = env->GetStringUTFChars(nameStr, nullptr);

                jclass memoryUsageClass = env->FindClass("java/lang/management/MemoryUsage");
                jmethodID getUsed = env->GetMethodID(memoryUsageClass, "getUsed", "()J");
                jmethodID getMax = env->GetMethodID(memoryUsageClass, "getMax", "()J");

                if (getUsed && getMax) {
                    jlong used = env->CallLongMethod(usage, getUsed);
                    jlong max = env->CallLongMethod(usage, getMax);

                    std::ostringstream oss;
                    oss << "  Pool '" << name << "': " << formatBytes(used);
                    if (max > 0) {
                        double usage_percent = (double)used / max * 100.0;
                        oss << " / " << formatBytes(max) << " (" << std::fixed
                            << std::setprecision(1) << usage_percent << "%)";
                    }
                    writeOutput(oss.str());
                }

                env->ReleaseStringUTFChars(nameStr, name);
            }
        }

        env->ExceptionClear();
    }

    std::string formatBytes(jlong bytes) {
        if (bytes < 1024)
            return std::to_string(bytes) + " B";
        if (bytes < 1024 * 1024)
            return std::to_string(bytes / 1024) + " KB";
        if (bytes < 1024 * 1024 * 1024)
            return std::to_string(bytes / 1024 / 1024) + " MB";
        return std::to_string(bytes / 1024 / 1024 / 1024) + " GB";
    }

    static void JNICALL onGCStart(jvmtiEnv* jvmti) {
        // Note: Can't use writeOutput here as it's a static callback without instance access
        // For GC events, we'll log to stderr so it appears in jvmtool console
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::cerr << "[Native SA] GC Started at "
                  << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S") << std::endl;
        std::cerr.flush();
    }

    static void JNICALL onGCFinish(jvmtiEnv* jvmti) {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::cerr << "[Native SA] GC Finished at "
                  << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S") << std::endl;
        std::cerr.flush();
    }
};

// Static member initialization
std::atomic<int> MemorySAModule::instance_counter_{0};

// Register module - use a safer approach with process-level singleton
extern "C" {
// Use a global flag file to prevent multiple instances
static const char* INSTANCE_LOCK_FILE = "/tmp/jvmtool_memory_sa_lock";
static MemorySAModule* memoryModule = nullptr;
static std::mutex module_mutex;
static bool module_registered = false;
static int lock_fd = -1;

bool acquireInstanceLock() {
    lock_fd = open(INSTANCE_LOCK_FILE, O_CREAT | O_EXCL | O_WRONLY, 0644);
    if (lock_fd == -1) {
        if (errno == EEXIST) {
            // Another instance is already running
            return false;
        }
        // Other error, try to continue anyway
        return true;
    }
    // Write our PID to the lock file
    std::string pid_str = std::to_string(getpid());
    write(lock_fd, pid_str.c_str(), pid_str.length());
    return true;
}

void releaseInstanceLock() {
    if (lock_fd != -1) {
        close(lock_fd);
        unlink(INSTANCE_LOCK_FILE);
        lock_fd = -1;
    }
}

void registerMemoryModule() {
    std::lock_guard<std::mutex> lock(module_mutex);
    if (!module_registered) {
        // Check if another instance is already running
        if (!acquireInstanceLock()) {
            std::cerr << "[Native SA] Another memory SA instance is already running, skipping..."
                      << std::endl;
            return;
        }

        if (!memoryModule) {
            memoryModule = new MemorySAModule();
        }
        AgentManager::instance().registerModule(memoryModule);
        module_registered = true;
        std::cerr << "[Native SA] Memory SA module registered successfully" << std::endl;
    }
}

// Auto-register when library is loaded
__attribute__((constructor)) void initModule() {
    registerMemoryModule();
}

// Cleanup on unload
__attribute__((destructor)) void cleanupModule() {
    std::lock_guard<std::mutex> lock(module_mutex);
    if (memoryModule) {
        delete memoryModule;
        memoryModule = nullptr;
    }
    releaseInstanceLock();
    module_registered = false;
}
}
