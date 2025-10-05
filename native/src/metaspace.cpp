#include "metaspace.h"

#include <fcntl.h>      // for open()
#include <sys/types.h>  // for pid_t
#include <unistd.h>     // for getpid()

#include <cerrno>  // for errno (modernize-deprecated-headers)
#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstring>
#include <initializer_list>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>
#include <unordered_map>
#include <vector>

#include "common.h"
#include "deadline_scheduler.h"
#include "jni_thread.h"
#include "scope_guard.h"

namespace jvmtool {
namespace {

template <typename T>
inline T ptrAt(uintptr_t base, uintptr_t offset) {
    return reinterpret_cast<T>(base + offset);  // NOLINT(performance-no-int-to-ptr)
}

void joinThread(std::thread& t) {
    try {
        if (t.joinable()) {
            t.join();
        }
    } catch (...) {
    }
}

std::vector<std::string> splitWhitespace(const std::string& line) {
    std::vector<std::string> tokens;
    size_t i = 0;
    const size_t len = line.length();
    while (i < len) {
        while (i < len && std::isspace(static_cast<unsigned char>(line[i]))) {
            ++i;
        }
        if (i >= len) {
            break;
        }
        size_t j = i;
        while (j < len && !std::isspace(static_cast<unsigned char>(line[j]))) {
            ++j;
        }
        tokens.emplace_back(line.substr(i, j - i));
        i = j;
    }
    return tokens;
}

size_t parseSizeToken(const std::string& token) {
    if (token.empty()) {
        return 0;
    }
    try {
        return static_cast<size_t>(std::stoull(token));
    } catch (...) {
        return 0;
    }
}

int findColumnIndex(const std::unordered_map<std::string, size_t>& columns,
                    std::initializer_list<const char*> names) {
    for (const char* name : names) {
        auto it = columns.find(name);
        if (it != columns.end()) {
            return static_cast<int>(it->second);
        }
    }
    return -1;
}

}  // namespace

// Static member definitions
bool MetaspaceStructs::_has_metaspace_structs = false;
void** MetaspaceStructs::_shared_metaspace_base_addr = nullptr;
void** MetaspaceStructs::_shared_metaspace_top_addr = nullptr;
void* MetaspaceStructs::_shared_metaspace_base = nullptr;
void* MetaspaceStructs::_shared_metaspace_top = nullptr;

namespace {

struct ClassLoaderStatsBuilder {
    std::vector<MetaspaceStructs::ClassLoaderStats>& out;
    bool headerParsed{false};
    std::unordered_map<std::string, size_t> columnIndex{};

    explicit ClassLoaderStatsBuilder(std::vector<MetaspaceStructs::ClassLoaderStats>& o)
        : out(o) {}

    static bool startsWith(const std::string& line, const char* prefix) {
        return line.compare(0, std::strlen(prefix), prefix) == 0;
    }

    bool addLine(const std::string& line) {
        if (!headerParsed) {
            if (startsWith(line, "Index")) {
                auto tokens = splitWhitespace(line);
                for (size_t i = 0; i < tokens.size(); ++i) {
                    columnIndex.emplace(tokens[i], i);
                }
                headerParsed = true;
            }
            return true;
        }

        if (line.empty()) {
            return true;
        }
        if (startsWith(line, "Total")) {
            return false;
        }

        auto tokens = splitWhitespace(line);
        if (tokens.size() < 5) {
            return true;
        }

        const int loaderIdx = findColumnIndex(columnIndex, {"Loader", "ClassLoader"});
        const int moduleIdx = findColumnIndex(columnIndex, {"Module"});
        const int metaIdx = findColumnIndex(columnIndex, {"Metaspace", "Meta"});
        const int klassIdx = findColumnIndex(columnIndex, {"Class", "Klass"});
        const int methodIdx = findColumnIndex(columnIndex, {"Method"});
        const int cpIdx = findColumnIndex(columnIndex, {"ConstantPool", "CP"});

        if (loaderIdx < 0 || metaIdx < 0) {
            return true;
        }

        MetaspaceStructs::ClassLoaderStats stats;
        if (static_cast<size_t>(loaderIdx) < tokens.size()) {
            stats.loader_name = tokens[loaderIdx];
        }
        if (moduleIdx >= 0 && static_cast<size_t>(moduleIdx) < tokens.size()) {
            stats.module_name = tokens[moduleIdx];
        }
        if (static_cast<size_t>(metaIdx) < tokens.size()) {
            stats.total_bytes = parseSizeToken(tokens[metaIdx]);
        }
        if (klassIdx >= 0 && static_cast<size_t>(klassIdx) < tokens.size()) {
            stats.klass_bytes = parseSizeToken(tokens[klassIdx]);
        }
        if (methodIdx >= 0 && static_cast<size_t>(methodIdx) < tokens.size()) {
            stats.method_bytes = parseSizeToken(tokens[methodIdx]);
        }
        if (cpIdx >= 0 && static_cast<size_t>(cpIdx) < tokens.size()) {
            stats.constant_pool_bytes = parseSizeToken(tokens[cpIdx]);
        }

        out.emplace_back(std::move(stats));
        return true;
    }
};

}  // namespace

// Run at agent load time
void MetaspaceStructs::init(CodeCache* libjvm) {
    if (libjvm != nullptr) {
        initMetaspaceOffsets();
    }
}

// Run when VM is initialized and JNI is available
void MetaspaceStructs::ready() {
    resolveMetaspaceOffsets();
}

void MetaspaceStructs::initMetaspaceOffsets() {
    uintptr_t entry = readSymbol("gHotSpotVMStructs");
    const uintptr_t stride = readSymbol("gHotSpotVMStructEntryArrayStride");
    const uintptr_t type_offset = readSymbol("gHotSpotVMStructEntryTypeNameOffset");
    const uintptr_t field_offset = readSymbol("gHotSpotVMStructEntryFieldNameOffset");
    const uintptr_t address_offset = readSymbol("gHotSpotVMStructEntryAddressOffset");

    if (entry != 0 && stride != 0) {
        for (;; entry += stride) {
            const char* type = *ptrAt<const char**>(entry, type_offset);
            const char* field = *ptrAt<const char**>(entry, field_offset);
            if (type == nullptr || field == nullptr) {
                break;
            }

            // Look for MetaspaceObj static fields
            if (strcmp(type, "MetaspaceObj") == 0) {
                if (strcmp(field, "_shared_metaspace_base") == 0) {
                    _shared_metaspace_base_addr = *ptrAt<void***>(entry, address_offset);
                } else if (strcmp(field, "_shared_metaspace_top") == 0) {
                    _shared_metaspace_top_addr = *ptrAt<void***>(entry, address_offset);
                }
            }
        }
    }
}

void MetaspaceStructs::resolveMetaspaceOffsets() {
    // Resolve shared metaspace boundaries
    if (_shared_metaspace_base_addr != nullptr) {
        _shared_metaspace_base = *_shared_metaspace_base_addr;
    }

    if (_shared_metaspace_top_addr != nullptr) {
        _shared_metaspace_top = *_shared_metaspace_top_addr;
    }

    // Check if we have valid metaspace structures
    _has_metaspace_structs =
        (_shared_metaspace_base_addr != nullptr && _shared_metaspace_top_addr != nullptr);
}

bool MetaspaceStructs::collectClassLoaderStats(JNIEnv* env, std::vector<ClassLoaderStats>& out) {
    out.clear();

    if (env == nullptr) {
        return false;
    }

    jclass diagClass = env->FindClass("sun/management/DiagnosticCommandImpl");
    if (diagClass == nullptr) {
        env->ExceptionClear();
        return false;
    }

    jmethodID getMBeanMethod = env->GetStaticMethodID(
        diagClass, "getDiagnosticCommandMBean", "(Ljava/lang/String;)Ljava/lang/Object;");
    if (getMBeanMethod == nullptr) {
        env->ExceptionClear();
        env->DeleteLocalRef(diagClass);
        return false;
    }

    jstring objectName = env->NewStringUTF("com.sun.management:type=DiagnosticCommand");
    jobject mbean = env->CallStaticObjectMethod(diagClass, getMBeanMethod, objectName);
    env->DeleteLocalRef(objectName);
    if (env->ExceptionCheck() || mbean == nullptr) {
        env->ExceptionClear();
        env->DeleteLocalRef(diagClass);
        return false;
    }

    jclass mbeanClass = env->GetObjectClass(mbean);
    jmethodID execMethod = env->GetMethodID(
        mbeanClass, "executeDiagnosticCommand", "(Ljava/lang/String;)[Ljava/lang/String;");
    if (execMethod == nullptr) {
        env->ExceptionClear();
        env->DeleteLocalRef(mbeanClass);
        env->DeleteLocalRef(mbean);
        env->DeleteLocalRef(diagClass);
        return false;
    }

    jstring command = env->NewStringUTF("GC.class_stats");
    jobjectArray lines = (jobjectArray)env->CallObjectMethod(mbean, execMethod, command);
    env->DeleteLocalRef(command);
    env->DeleteLocalRef(mbeanClass);
    env->DeleteLocalRef(mbean);
    env->DeleteLocalRef(diagClass);

    if (env->ExceptionCheck() || lines == nullptr) {
        env->ExceptionClear();
        return false;
    }

    ClassLoaderStatsBuilder builder(out);
    jsize len = env->GetArrayLength(lines);
    for (jsize i = 0; i < len; ++i) {
        jstring line = (jstring)env->GetObjectArrayElement(lines, i);
        if (line == nullptr) {
            continue;
        }
        const char* chars = env->GetStringUTFChars(line, nullptr);
        if (chars != nullptr) {
            std::string lineStr(chars);
            env->ReleaseStringUTFChars(line, chars);
            if (!builder.addLine(lineStr)) {
                env->DeleteLocalRef(line);
                break;
            }
        }
        env->DeleteLocalRef(line);
    }

    env->DeleteLocalRef(lines);
    return !out.empty();
}

MetaspaceSAModule::MetaspaceSAModule() = default;

jvmtiError MetaspaceSAModule::initialize(JavaVM* java_vm, jvmtiEnv* jvmti) {
    const jvmtiError init_err = AgentModule::initialize(java_vm, jvmti);
    if (init_err != JVMTI_ERROR_NONE) {
        return init_err;
    }
    MetaspaceStructs::init(VMStructs::libjvm());
    MetaspaceStructs::ready();
    return JVMTI_ERROR_NONE;
}

jint MetaspaceSAModule::onAttach(const TaskOpt& opt) {
    if (writeReady() != JNI_OK) {
        return JNI_ERR;
    }

    if (state_ == ModuleState::ANALYZING) {
        return JNI_ERR;
    }

    joinThread(monitor_thread_);

    stop_.store(false, std::memory_order_relaxed);

    try {
        monitor_thread_ = std::thread(&MetaspaceSAModule::monitor, this, opt);
        state_ = ModuleState::ANALYZING;
        return JNI_OK;
    } catch (...) {
    }
    return JNI_ERR;
}

MetaspaceSAModule::~MetaspaceSAModule() {
    stop_.store(true, std::memory_order_relaxed);
    joinThread(monitor_thread_);
}

void MetaspaceSAModule::monitor(const TaskOpt& opt) {
    auto state_guard =
        jvmtool::make_scope_exit([this]() noexcept { this->state_ = ModuleState::IDLE; });

    try {
        if (!writer_->setNonBlocking()) {
            return;
        }

        DeadlineRegistration deadline_reg;
        if (opt.duration > 0) {
            deadline_reg = schedule_stop_on_deadline(
                stop_, (std::chrono::steady_clock::now() + std::chrono::seconds(opt.duration)));
        }

        int client_fd = -1;
        int attempts = 0;
        while (!stop_.load(std::memory_order_relaxed) && attempts < 10) {
            const int r = writer_->tryAccept();
            if (r != -2) {
                client_fd = r;
                break;
            }
            attempts++;
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }

        if (client_fd == -1) {
            return;
        }

        auto fd = ClosableFd(client_fd);

        const ScopedAttach attach(vm_);
        if (!attach.ok()) {
            Message msg(
                agentType(), ERROR,
                std::string("[Native SA] Failed to attach monitoring thread: ") + attach.error());
            writeAndClose(fd, msg);
            return;
        }

        const Message header(agentType(), DATA, "[Native SA] === Metaspace Analysis ===\n");
        if (!writer_->writeMessage(fd, header)) {
            return;
        }

        while (!stop_.load(std::memory_order_relaxed) && analyzeMetaspace(fd, attach.env())) {
            std::this_thread::sleep_for(std::chrono::seconds(opt.interval));
        }

        if (stop_.load(std::memory_order_relaxed)) {
            Message data(agentType(), STATUS, "[Native SA] === End Analysis ===\n");
            writeAndClose(fd, data);
        }
    } catch (...) {
    }
}

bool MetaspaceSAModule::analyzeMetaspace(int fd, JNIEnv* env) {
    try {
        auto data = collectMetaspaceStatistics(env);
        if (!writeMessage(fd, data)) {
            return false;
        }

        // If metaspace structures are not available, don't continue the loop
        if (!MetaspaceStructs::hasMetaspaceStructs()) {
            Message warning(
                agentType(), STATUS,
                "[Native SA] Metaspace analysis not supported on this JVM - ending analysis");
            writeAndClose(fd, warning);
            return false;
        }

        return true;
    } catch (const std::exception& e) {
        Message err(agentType(), ERROR,
                    "[Native SA] Error during metaspace analysis: " + std::string(e.what()));
        writeAndClose(fd, err);
    }
    return false;
}

Message MetaspaceSAModule::collectMetaspaceStatistics(JNIEnv* env) const {
    std::stringstream ss;

    ss << "[Native SA] Metaspace Statistics:\n";

    if (!MetaspaceStructs::hasMetaspaceStructs()) {
        ss << "  Metaspace structures not available\n";
        return {agentType(), DATA, ss.str()};
    }

    ss << "  Metaspace structures available: Yes\n";

    if (MetaspaceStructs::hasSharedMetaspace()) {
        const void* base = MetaspaceStructs::sharedMetaspaceBase();
        const void* top = MetaspaceStructs::sharedMetaspaceTop();
        const size_t size = MetaspaceStructs::sharedMetaspaceSize();

        ss << "  Shared metaspace available: Yes\n";
        ss << "  Shared metaspace base: 0x" << std::hex << base << "\n";
        ss << "  Shared metaspace top:  0x" << std::hex << top << "\n";
        ss << "  Shared metaspace size: " << std::dec << size << " bytes (" << (size / 1024 / 1024)
           << " MB)\n";

        // Calculate usage percentage (this is a simplified calculation)
        if (size > 0) {
            ss << "  Shared metaspace range: [0x" << std::hex << base << " - 0x" << std::hex << top
               << ")\n";
        }
    } else {
        ss << "  Shared metaspace available: No (CDS may not be enabled)\n";
    }

    std::vector<MetaspaceStructs::ClassLoaderStats> stats;
    if (MetaspaceStructs::collectClassLoaderStats(env, stats)) {
        ss << "\n  Top ClassLoader Metaspace Usage:\n";

        std::sort(stats.begin(), stats.end(), [](const auto& lhs, const auto& rhs) {
            return lhs.total_bytes > rhs.total_bytes;
        });

        const size_t limit = std::min<size_t>(stats.size(), 10);
        for (size_t i = 0; i < limit; ++i) {
            const auto& entry = stats[i];
            ss << "    [" << (i + 1) << "] " << entry.loader_name
               << " total=" << entry.total_bytes << "B"
               << " (klass=" << entry.klass_bytes << "B"
               << ", method=" << entry.method_bytes << "B"
               << ", cp=" << entry.constant_pool_bytes << "B)";
            if (!entry.module_name.empty()) {
                ss << " module=" << entry.module_name;
            }
            ss << "\n";
        }

        if (stats.size() > limit) {
            size_t remaining_total = 0;
            for (size_t i = limit; i < stats.size(); ++i) {
                remaining_total += stats[i].total_bytes;
            }
            ss << "    ... remaining " << (stats.size() - limit)
               << " loaders total=" << remaining_total << "B\n";
        }
    } else {
        ss << "\n  ClassLoader stats not available on this JVM\n";
    }

    return {agentType(), DATA, ss.str()};
}

namespace {
__attribute__((constructor)) void initModule() {
    try {
        AgentManager::instance().registerModule(std::make_unique<MetaspaceSAModule>());
        std::cerr << "[Native SA] Memory SA module registered successfully\n";
    } catch (const std::exception& e) {
        std::cerr << "[Native SA] Failed to register memory SA module: " << e.what() << "\n";
    }
}
}  // anonymous namespace

}  // namespace jvmtool

