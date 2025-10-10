#include "metaspace.h"

#include <fcntl.h>      // for open()
#include <sys/types.h>  // for pid_t
#include <unistd.h>     // for getpid()

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstring>
#include <initializer_list>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <thread>
#include <unordered_map>
#include <vector>

#include "common.h"
#include "deadline_scheduler.h"
#include "jni_thread.h"
#include "scope_guard.h"
#include "vm/vm.h"

namespace jvmtool {
namespace {

template <typename T>
inline T ptrAt(uintptr_t base, uintptr_t offset) {
    return reinterpret_cast<T>(base + offset);  // NOLINT(performance-no-int-to-ptr)
}

TaskOpt normalizeTaskOpt(const TaskOpt& opt) {
    TaskOpt normalized = opt;
    normalized.type = toLowerCopy(opt.type);
    if (normalized.type == "stats") {
        normalized.single_shot = true;
    }
    return normalized;
}

size_t effectiveUsedBytes(const ClassLoaderStats& stats) {
    size_t used = stats.space.totalUsedBytes();
    if (used == 0) {
        used = stats.metadata.total();
    }
    return used;
}

size_t effectiveCapacity(const ClassLoaderStats& stats) {
    size_t capacity = stats.space.totalCommittedBytes();
    if (capacity == 0) {
        capacity = stats.metadata.total();
    }
    return capacity;
}

bool buildStatsFromCommandOutput(JNIEnv* env, jobjectArray lines, jstring response_string,
                                 std::vector<ClassLoaderStats>& out,
                                 ClassLoaderSummary* summary_out) {
    if (lines == nullptr && response_string == nullptr) {
        return false;
    }

    out.clear();

    ClassLoaderSummary local_summary;
    ClassLoaderSummary* summary_ptr = nullptr;
    if (summary_out != nullptr) {
        local_summary = ClassLoaderSummary();
        summary_ptr = &local_summary;
    }

    ClassLoaderStatsBuilder builder(out, summary_ptr);

    if (lines != nullptr) {
        jsize len = env->GetArrayLength(lines);
        for (jsize i = 0; i < len; ++i) {
            jstring line = static_cast<jstring>(env->GetObjectArrayElement(lines, i));
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
    } else if (response_string != nullptr) {
        const char* chars = env->GetStringUTFChars(response_string, nullptr);
        if (chars != nullptr) {
            std::string response(chars);
            env->ReleaseStringUTFChars(response_string, chars);

            size_t start = 0;
            while (start <= response.size()) {
                const size_t end = response.find('\n', start);
                std::string line = response.substr(
                    start, (end == std::string::npos) ? std::string::npos : end - start);
                if (!line.empty() && line.back() == '\r') {
                    line.pop_back();
                }
                if (!builder.addLine(line)) {
                    break;
                }
                if (end == std::string::npos) {
                    break;
                }
                start = end + 1;
            }
        }
        env->DeleteLocalRef(response_string);
    }

    const bool has_data = !out.empty();

    if (has_data && summary_ptr != nullptr) {
        summary_ptr->loader_count = out.size();
        summary_ptr->total_class_count = 0;
        summary_ptr->total_hidden_classes = 0;
        summary_ptr->aggregated_committed_bytes = 0;
        summary_ptr->aggregated_used_bytes = 0;
        summary_ptr->aggregated_hidden_committed_bytes = 0;
        summary_ptr->aggregated_hidden_used_bytes = 0;
        summary_ptr->aggregated_metadata_bytes = 0;

        for (const auto& entry : out) {
            summary_ptr->total_class_count += entry.class_count;
            summary_ptr->total_hidden_classes += entry.hidden_classes;
            summary_ptr->aggregated_committed_bytes += entry.space.committed_bytes;
            summary_ptr->aggregated_used_bytes += entry.space.used_bytes;
            summary_ptr->aggregated_hidden_committed_bytes += entry.space.hidden_committed_bytes;
            summary_ptr->aggregated_hidden_used_bytes += entry.space.hidden_used_bytes;
            summary_ptr->aggregated_metadata_bytes += entry.metadata.total();
        }

        if (summary_ptr->reported_total_classes == 0) {
            summary_ptr->reported_total_classes =
                summary_ptr->total_class_count + summary_ptr->total_hidden_classes;
        }

        *summary_out = *summary_ptr;
    }

    return has_data;
}

}  // namespace

std::array<const char*, 2> SelectClassStatsCommands(int hotspot_version) {
    if (hotspot_version >= 21) {
        return {{"VM.classloader_stats", "GC.class_stats"}};
    }
    return {{"GC.class_stats", "VM.classloader_stats"}};
}

// Static member definitions
bool MetaspaceStructs::_has_metaspace_structs = false;
void** MetaspaceStructs::_shared_metaspace_base_addr = nullptr;
void** MetaspaceStructs::_shared_metaspace_top_addr = nullptr;
void* MetaspaceStructs::_shared_metaspace_base = nullptr;
void* MetaspaceStructs::_shared_metaspace_top = nullptr;
size_t* MetaspaceStructs::_metaspace_capacity_until_gc_addr = nullptr;
size_t* MetaspaceStructs::_metaspace_used_words_addr = nullptr;
size_t* MetaspaceStructs::_metaspace_committed_words_addr = nullptr;
size_t MetaspaceStructs::_metaspace_capacity_until_gc_words = 0;
size_t MetaspaceStructs::_metaspace_used_words = 0;
size_t MetaspaceStructs::_metaspace_committed_words = 0;
void** MetaspaceStructs::_compressed_class_space_virtual_space_addr = nullptr;
void* MetaspaceStructs::_compressed_class_space_virtual_space = nullptr;
void* MetaspaceStructs::_compressed_class_space_low = nullptr;
void* MetaspaceStructs::_compressed_class_space_high = nullptr;
void* MetaspaceStructs::_compressed_class_space_low_boundary = nullptr;
void* MetaspaceStructs::_compressed_class_space_high_boundary = nullptr;
int MetaspaceStructs::_virtual_space_low_offset = -1;
int MetaspaceStructs::_virtual_space_high_offset = -1;
int MetaspaceStructs::_virtual_space_low_boundary_offset = -1;
int MetaspaceStructs::_virtual_space_high_boundary_offset = -1;

ClassLoaderStatsBuilder::ClassLoaderStatsBuilder(std::vector<ClassLoaderStats>& out,
                                                 ClassLoaderSummary* summary)
    : out_(out), summary_(summary) {}

bool ClassLoaderStatsBuilder::startsWith(const std::string& line, const char* prefix) {
    return line.compare(0, std::strlen(prefix), prefix) == 0;
}

bool ClassLoaderStatsBuilder::detectHeader(const std::string& line) {
    const std::string trimmed = trimCopy(line);
    if (trimmed.empty()) {
        return false;
    }

    if (startsWith(trimmed, "Index")) {
        column_index_.clear();
        auto tokens = splitWhitespace(trimmed);
        for (size_t i = 0; i < tokens.size(); ++i) {
            column_index_.emplace(tokens[i], i);
        }
        format_ = StatsTableFormat::Index;
        return true;
    }

    if (trimmed.find("ClassLoader") != std::string::npos &&
        (trimmed.find("Chunk") != std::string::npos ||
         trimmed.find("ChunkSz") != std::string::npos)) {
        format_ = StatsTableFormat::Chunk;
        return true;
    }

    return false;
}

void ClassLoaderStatsBuilder::parseTotals(const std::string& line) {
    if (summary_ == nullptr) {
        return;
    }

    auto tokens = splitWhitespace(line);
    if (tokens.empty()) {
        return;
    }

    size_t start_index = 0;
    if (tokens.size() > 1 && tokens[1] == "=") {
        start_index = 2;
    } else if (tokens[0] == "Total") {
        start_index = 1;
    }

    std::vector<size_t> values;
    for (size_t i = start_index; i < tokens.size(); ++i) {
        if (tokens[i] == "=") {
            continue;
        }
        const size_t value = parseSizeToken(tokens[i]);
        values.push_back(value);
    }

    if (!values.empty()) {
        summary_->reported_loader_count = values[0];
    }
    if (values.size() >= 2) {
        summary_->reported_total_classes = values[1];
    }
    if (values.size() >= 3) {
        summary_->reported_total_chunk_bytes = values[2];
    }
    if (values.size() >= 4) {
        summary_->reported_total_block_bytes = values[3];
    }
}

bool ClassLoaderStatsBuilder::parseHiddenLine(const std::string& line) {
    if (format_ != StatsTableFormat::Chunk) {
        return false;
    }
    if (line.find("+ hidden classes") == std::string::npos) {
        return false;
    }

    auto tokens = splitWhitespace(line);
    if (tokens.size() < 3 || current_ == nullptr) {
        return true;
    }

    size_t idx = 0;
    const size_t hidden_class_count = parseSizeToken(tokens[idx++]);
    const size_t hidden_chunk_bytes = (idx < tokens.size()) ? parseSizeToken(tokens[idx++]) : 0;
    const size_t hidden_block_bytes = (idx < tokens.size()) ? parseSizeToken(tokens[idx++]) : 0;

    current_->hidden_classes = hidden_class_count;
    current_->space.hidden_committed_bytes = hidden_chunk_bytes;
    current_->space.hidden_used_bytes = hidden_block_bytes;
    if (current_->space.fallback_total_bytes == 0) {
        current_->space.fallback_total_bytes = hidden_block_bytes;
    } else {
        current_->space.fallback_total_bytes += hidden_block_bytes;
    }

    return true;
}

bool ClassLoaderStatsBuilder::parseIndexRow(const std::string& line) {
    auto tokens = splitWhitespace(line);
    if (tokens.size() < 2) {
        return true;
    }

    const int loaderIdx = findColumnIndex(column_index_, {"Loader", "ClassLoader"});
    const int moduleIdx = findColumnIndex(column_index_, {"Module"});
    const int metaIdx = findColumnIndex(column_index_, {"Metaspace", "Meta"});
    const int klassIdx = findColumnIndex(column_index_, {"Class", "Klass"});
    const int methodIdx = findColumnIndex(column_index_, {"Method"});
    const int cpIdx = findColumnIndex(column_index_, {"ConstantPool", "CP"});

    if (loaderIdx < 0 || metaIdx < 0) {
        return true;
    }

    ClassLoaderStats stats;
    if (static_cast<size_t>(loaderIdx) < tokens.size()) {
        stats.loader_name = tokens[loaderIdx];
    }
    if (moduleIdx >= 0 && static_cast<size_t>(moduleIdx) < tokens.size()) {
        stats.module_name = tokens[moduleIdx];
    }
    if (static_cast<size_t>(metaIdx) < tokens.size()) {
        const size_t total_bytes = parseSizeToken(tokens[metaIdx]);
        stats.space.fallback_total_bytes = total_bytes;
        stats.space.committed_bytes = total_bytes;
        stats.space.used_bytes = total_bytes;
    }
    if (klassIdx >= 0 && static_cast<size_t>(klassIdx) < tokens.size()) {
        stats.metadata.klass_bytes = parseSizeToken(tokens[klassIdx]);
    }
    if (methodIdx >= 0 && static_cast<size_t>(methodIdx) < tokens.size()) {
        stats.metadata.method_bytes = parseSizeToken(tokens[methodIdx]);
    }
    if (cpIdx >= 0 && static_cast<size_t>(cpIdx) < tokens.size()) {
        stats.metadata.constant_pool_bytes = parseSizeToken(tokens[cpIdx]);
    }

    out_.emplace_back(std::move(stats));
    current_ = &out_.back();

    return true;
}

bool ClassLoaderStatsBuilder::parseChunkRow(const std::string& line) {
    auto tokens = splitWhitespace(line);
    if (tokens.size() < 6) {
        return true;
    }

    ClassLoaderStats stats;
    stats.loader_address = tokens[0];
    if (tokens.size() > 1) {
        stats.parent_address = tokens[1];
    }
    if (tokens.size() > 2) {
        stats.cld_address = tokens[2];
    }

    size_t index = 3;
    if (index < tokens.size()) {
        stats.class_count = parseSizeToken(tokens[index++]);
    }
    if (index < tokens.size()) {
        stats.space.committed_bytes = parseSizeToken(tokens[index++]);
    }
    if (index < tokens.size()) {
        stats.space.used_bytes = parseSizeToken(tokens[index++]);
        stats.space.fallback_total_bytes = stats.space.used_bytes;
    }

    if (stats.space.fallback_total_bytes == 0) {
        stats.space.fallback_total_bytes = stats.space.committed_bytes;
    }

    if (index < tokens.size()) {
        stats.loader_name = tokens[index++];
        for (; index < tokens.size(); ++index) {
            stats.loader_name.append(" ");
            stats.loader_name.append(tokens[index]);
        }
    } else {
        stats.loader_name = "<unknown>";
    }

    out_.emplace_back(std::move(stats));
    current_ = &out_.back();

    return true;
}

bool ClassLoaderStatsBuilder::addLine(const std::string& line) {
    if (!header_parsed_) {
        header_parsed_ = detectHeader(line);
        return true;
    }

    const std::string trimmed = trimCopy(line);
    if (trimmed.empty()) {
        return true;
    }

    if (trimmed.rfind("Total", 0) == 0) {
        parseTotals(trimmed);
        return false;
    }

    if (parseHiddenLine(trimmed)) {
        return true;
    }

    switch (format_) {
        case StatsTableFormat::Index:
            return parseIndexRow(trimmed);
        case StatsTableFormat::Chunk:
            return parseChunkRow(trimmed);
        case StatsTableFormat::Unknown:
        default:
            if (detectHeader(trimmed)) {
                return true;
            }
            return true;
    }
}

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
    const uintptr_t offset_offset = readSymbol("gHotSpotVMStructEntryOffsetOffset");
    const uintptr_t address_offset = readSymbol("gHotSpotVMStructEntryAddressOffset");

    if (entry != 0 && stride != 0) {
        for (;; entry += stride) {
            const char* type = *ptrAt<const char**>(entry, type_offset);
            const char* field = *ptrAt<const char**>(entry, field_offset);
            if (type == nullptr || field == nullptr) {
                break;
            }

            const int field_offset_value =
                (offset_offset != 0) ? *ptrAt<int*>(entry, offset_offset) : -1;

            // Look for MetaspaceObj static fields
            if (strcmp(type, "MetaspaceObj") == 0) {
                if (strcmp(field, "_shared_metaspace_base") == 0) {
                    _shared_metaspace_base_addr = *ptrAt<void***>(entry, address_offset);
                } else if (strcmp(field, "_shared_metaspace_top") == 0) {
                    _shared_metaspace_top_addr = *ptrAt<void***>(entry, address_offset);
                }
            } else if (strcmp(type, "Metaspace") == 0) {
                if (strcmp(field, "_capacity_until_GC") == 0) {
                    _metaspace_capacity_until_gc_addr = *ptrAt<size_t**>(entry, address_offset);
                } else if (strcmp(field, "_used_words") == 0) {
                    _metaspace_used_words_addr = *ptrAt<size_t**>(entry, address_offset);
                } else if (strcmp(field, "_committed_words") == 0) {
                    _metaspace_committed_words_addr = *ptrAt<size_t**>(entry, address_offset);
                }
            } else if (strcmp(type, "CompressedClassSpace") == 0) {
                if (strcmp(field, "_virtual_space") == 0) {
                    _compressed_class_space_virtual_space_addr =
                        *ptrAt<void***>(entry, address_offset);
                }
            } else if (strcmp(type, "VirtualSpace") == 0) {
                if (strcmp(field, "_low") == 0) {
                    _virtual_space_low_offset = field_offset_value;
                } else if (strcmp(field, "_high") == 0) {
                    _virtual_space_high_offset = field_offset_value;
                } else if (strcmp(field, "_low_boundary") == 0) {
                    _virtual_space_low_boundary_offset = field_offset_value;
                } else if (strcmp(field, "_high_boundary") == 0) {
                    _virtual_space_high_boundary_offset = field_offset_value;
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

    if (_metaspace_capacity_until_gc_addr != nullptr) {
        _metaspace_capacity_until_gc_words = *_metaspace_capacity_until_gc_addr;
    } else {
        _metaspace_capacity_until_gc_words = 0;
    }

    if (_metaspace_used_words_addr != nullptr) {
        _metaspace_used_words = *_metaspace_used_words_addr;
    } else {
        _metaspace_used_words = 0;
    }

    if (_metaspace_committed_words_addr != nullptr) {
        _metaspace_committed_words = *_metaspace_committed_words_addr;
    } else {
        _metaspace_committed_words = 0;
    }

    _compressed_class_space_low = nullptr;
    _compressed_class_space_high = nullptr;
    _compressed_class_space_low_boundary = nullptr;
    _compressed_class_space_high_boundary = nullptr;

    if (_compressed_class_space_virtual_space_addr != nullptr) {
        _compressed_class_space_virtual_space = *_compressed_class_space_virtual_space_addr;
    } else {
        _compressed_class_space_virtual_space = nullptr;
    }

    if (_compressed_class_space_virtual_space != nullptr) {
        const uintptr_t vs_base =
            reinterpret_cast<uintptr_t>(_compressed_class_space_virtual_space);
        auto read_vs_ptr = [vs_base](int offset) -> void* {
            if (offset < 0) {
                return nullptr;
            }
            return *reinterpret_cast<void* const*>(vs_base + static_cast<uintptr_t>(offset));
        };

        _compressed_class_space_low = read_vs_ptr(_virtual_space_low_offset);
        _compressed_class_space_high = read_vs_ptr(_virtual_space_high_offset);
        _compressed_class_space_low_boundary = read_vs_ptr(_virtual_space_low_boundary_offset);
        _compressed_class_space_high_boundary = read_vs_ptr(_virtual_space_high_boundary_offset);
    }

    // Check if we have valid metaspace structures
    _has_metaspace_structs =
        (_shared_metaspace_base_addr != nullptr && _shared_metaspace_top_addr != nullptr);
}

bool MetaspaceStructs::collectClassLoaderStats(JNIEnv* env, std::vector<ClassLoaderStats>& out,
                                               ClassLoaderSummary* summary) {
    out.clear();

    if (summary != nullptr) {
        *summary = ClassLoaderSummary();
    }

    if (env == nullptr) {
        return false;
    }

    jclass diagClass = env->FindClass("com/sun/management/internal/DiagnosticCommandImpl");
    if (diagClass == nullptr) {
        env->ExceptionClear();
        diagClass = env->FindClass("sun/management/DiagnosticCommandImpl");
        if (diagClass == nullptr) {
            env->ExceptionClear();
            return false;
        }
    }

    bool requires_object_name = false;
    jmethodID getMBeanMethod = env->GetStaticMethodID(
        diagClass, "getDiagnosticCommandMBean", "()Lcom/sun/management/DiagnosticCommandMBean;");
    if (getMBeanMethod == nullptr) {
        env->ExceptionClear();
        getMBeanMethod = env->GetStaticMethodID(
            diagClass, "getDiagnosticCommandMBean",
            "(Ljava/lang/String;)Lcom/sun/management/DiagnosticCommandMBean;");
        if (getMBeanMethod != nullptr) {
            requires_object_name = true;
        } else {
            env->ExceptionClear();
            getMBeanMethod = env->GetStaticMethodID(diagClass, "getDiagnosticCommandMBean",
                                                    "(Ljava/lang/String;)Ljava/lang/Object;");
            if (getMBeanMethod == nullptr) {
                env->ExceptionClear();
                env->DeleteLocalRef(diagClass);
                return false;
            }
            requires_object_name = true;
        }
    }

    jobject mbean = nullptr;
    if (requires_object_name) {
        jstring objectName = env->NewStringUTF("com.sun.management:type=DiagnosticCommand");
        if (objectName == nullptr) {
            env->DeleteLocalRef(diagClass);
            return false;
        }
        mbean = env->CallStaticObjectMethod(diagClass, getMBeanMethod, objectName);
        env->DeleteLocalRef(objectName);
    } else {
        mbean = env->CallStaticObjectMethod(diagClass, getMBeanMethod);
    }
    env->DeleteLocalRef(diagClass);

    if (env->ExceptionCheck() || mbean == nullptr) {
        env->ExceptionClear();
        if (mbean != nullptr) {
            env->DeleteLocalRef(mbean);
        }
        return false;
    }

    jclass mbeanClass = env->GetObjectClass(mbean);
    if (mbeanClass == nullptr) {
        env->ExceptionClear();
        env->DeleteLocalRef(mbean);
        return false;
    }

    jmethodID execMethodArray = env->GetMethodID(mbeanClass, "executeDiagnosticCommand",
                                                 "(Ljava/lang/String;)[Ljava/lang/String;");
    if (execMethodArray == nullptr) {
        env->ExceptionClear();
    }

    jmethodID execMethodString = env->GetMethodID(mbeanClass, "executeDiagnosticCommand",
                                                  "(Ljava/lang/String;)Ljava/lang/String;");
    if (execMethodString == nullptr) {
        env->ExceptionClear();
    }

    const auto preferred_commands = SelectClassStatsCommands(VM::hotspot_version());

    bool success = false;

    for (const char* cmd : preferred_commands) {
        if (cmd == nullptr) {
            continue;
        }
        jstring command = env->NewStringUTF(cmd);
        if (command == nullptr) {
            continue;
        }

        jobjectArray candidate_lines = nullptr;
        jstring candidate_response = nullptr;

        if (execMethodArray != nullptr) {
            candidate_lines =
                static_cast<jobjectArray>(env->CallObjectMethod(mbean, execMethodArray, command));
            if (env->ExceptionCheck()) {
                env->ExceptionClear();
                candidate_lines = nullptr;
            }
        }

        if (candidate_lines == nullptr && execMethodString != nullptr) {
            candidate_response =
                static_cast<jstring>(env->CallObjectMethod(mbean, execMethodString, command));
            if (env->ExceptionCheck()) {
                env->ExceptionClear();
                candidate_response = nullptr;
            }
        }

        env->DeleteLocalRef(command);

        if (candidate_lines == nullptr && candidate_response == nullptr) {
            continue;
        }
        if (buildStatsFromCommandOutput(env, candidate_lines, candidate_response, out, summary)) {
            success = true;
            break;
        }
    }

    env->DeleteLocalRef(mbeanClass);
    env->DeleteLocalRef(mbean);

    return success;
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
        TaskOpt normalized_opt = normalizeTaskOpt(opt);
        monitor_thread_ = std::thread(&MetaspaceSAModule::monitor, this, normalized_opt);
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

    DeadlineRegistration deadline_reg;
    if (!opt.single_shot && opt.duration > 0) {
        deadline_reg = schedule_stop_on_deadline(
            stop_, (std::chrono::steady_clock::now() + std::chrono::seconds(opt.duration)));
    }
    try {
        if (!writer_->setNonBlocking()) {
            return;
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

        const auto fd = ClosableFd(client_fd);

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

        if (opt.type == "stats") {
            Message report = collectMetaspaceStatistics(attach.env());
            if (!writeMessage(fd, report)) {
                return;
            }
            Message done(agentType(), STATUS, "[Native SA] === End Analysis ===\n");
            writeAndClose(fd, done);
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

    if (!MetaspaceStructs::hasMetaspaceStructs()) {
        ss << "- Metaspace VMStructs not available; CDS boundaries unknown.\n";
        std::vector<ClassLoaderStats> stats;
        MetaspaceStructs::collectClassLoaderStats(env, stats);
        if (stats.empty()) {
            ss << "- ClassLoader diagnostic command unavailable on this JVM.\n";
        }
        return {agentType(), DATA, ss.str()};
    }

    ss << "- VMStructs access: available\n";
    if (MetaspaceStructs::hasSharedMetaspace()) {
        const void* base = MetaspaceStructs::sharedMetaspaceBase();
        const void* top = MetaspaceStructs::sharedMetaspaceTop();
        const size_t size = MetaspaceStructs::sharedMetaspaceSize();
        ss << "- CDS shared Metaspace: enabled\n";
        ss << "  base: " << base << ", top: " << top << ", size: " << size << " bytes ("
           << formatBytes(size) << ")\n";
    } else {
        ss << "- CDS shared Metaspace: not enabled\n";
    }

    std::vector<ClassLoaderStats> stats;
    ClassLoaderSummary summary;
    if (!MetaspaceStructs::collectClassLoaderStats(env, stats, &summary) || stats.empty()) {
        ss << "- ClassLoader stats unavailable (GC.class_stats not supported or command failed).\n";
        return {agentType(), DATA, ss.str()};
    }

    ss << "\n[ClassLoader Metaspace Summary]\n";
    ss << "- Loaders reported: " << summary.loader_count;
    if (summary.reported_loader_count > 0 &&
        summary.reported_loader_count != summary.loader_count) {
        ss << " (command reported " << summary.reported_loader_count << ")";
    }
    ss << "\n";

    const size_t aggregated_committed = summary.totalCommittedBytes();
    const size_t aggregated_used = summary.totalUsedBytes();
    const size_t total_chunk_bytes = summary.reported_total_chunk_bytes > 0
                                         ? summary.reported_total_chunk_bytes
                                         : aggregated_committed;
    const size_t total_block_bytes = summary.reported_total_block_bytes > 0
                                         ? summary.reported_total_block_bytes
                                         : aggregated_used;

    const size_t hidden_committed = summary.aggregated_hidden_committed_bytes;
    const size_t hidden_used = summary.aggregated_hidden_used_bytes;
    const size_t metadata_total = summary.aggregated_metadata_bytes;

    const size_t combined_classes = summary.total_class_count + summary.total_hidden_classes;
    ss << "- Classes observed: " << combined_classes;
    if (summary.reported_total_classes > 0 && summary.reported_total_classes != combined_classes) {
        ss << " (command reported " << summary.reported_total_classes << ")";
    }
    ss << "\n";

    ss << "- Total committed chunks: " << formatBytes(total_chunk_bytes) << " ("
       << total_chunk_bytes << " bytes)\n";
    ss << "- Total used blocks:     " << formatBytes(total_block_bytes) << " (" << total_block_bytes
       << " bytes)\n";
    ss << "- MetaChunk insight: chunk vs block usage available; detailed per-chunk mapping "
          "requires CLD walk (pending).\n";

    if (hidden_committed > 0 || hidden_used > 0 || summary.total_hidden_classes > 0) {
        ss << "- Hidden classes: " << summary.total_hidden_classes << " consuming "
           << formatBytes(hidden_used) << " in blocks (" << hidden_used << " bytes)";
        if (hidden_committed > hidden_used) {
            ss << ", committed " << formatBytes(hidden_committed) << " total";
        }
        ss << "\n";
    }

    if (metadata_total > 0) {
        ss << "- Metadata footprint: " << formatBytes(metadata_total) << " (" << metadata_total
           << " bytes across klass/method/CP)\n";
    }

    struct RankedEntry {
        size_t index;
        size_t used_bytes;
    };

    std::vector<RankedEntry> ranking;
    ranking.reserve(stats.size());
    for (size_t i = 0; i < stats.size(); ++i) {
        const auto& entry = stats[i];
        const size_t used_bytes = effectiveUsedBytes(entry);
        if (used_bytes > 0) {
            ranking.push_back({i, used_bytes});
        }
    }

    std::sort(ranking.begin(), ranking.end(), [](const RankedEntry& lhs, const RankedEntry& rhs) {
        return lhs.used_bytes > rhs.used_bytes;
    });

    const size_t limit = std::min<size_t>(ranking.size(), 10);
    if (limit == 0) {
        ss << "\n- No ClassLoader usage data returned.\n";
        return {agentType(), DATA, ss.str()};
    }

    ss << "\n[Top ClassLoader Metaspace Usage]\n";
    ss << "Rank  Used      Percent  Loader / Module\n";
    size_t suspicious_count = 0;
    for (size_t i = 0; i < limit; ++i) {
        const auto& entry = stats[ranking[i].index];
        const size_t used_bytes = ranking[i].used_bytes;
        const size_t capacity = effectiveCapacity(entry);
        double percent = 0.0;
        if (total_block_bytes > 0) {
            percent =
                (static_cast<double>(used_bytes) / static_cast<double>(total_block_bytes)) * 100.0;
        }

        const bool suspicious = percent >= 30.0 || used_bytes >= (50ULL * 1024 * 1024);
        if (suspicious) {
            ++suspicious_count;
        }

        ss << std::setw(4) << (i + 1) << "  " << std::setw(10) << formatBytes(used_bytes) << "  "
           << std::setw(7) << formatPercentage(percent) << "  " << entry.loader_name;
        if (!entry.module_name.empty()) {
            ss << " (module=" << entry.module_name << ")";
        }

        if (!entry.cld_address.empty()) {
            ss << "\n      CLD=" << entry.cld_address;
        }
        if (!entry.loader_address.empty()) {
            ss << " loader=" << entry.loader_address;
        }
        if (capacity > 0) {
            ss << " capacity=" << formatBytes(capacity);
        }
        if (suspicious) {
            ss << " [!! suspect]";
        }
        ss << "\n";
    }

    if (ranking.size() > limit) {
        size_t remaining_total = 0;
        for (size_t i = limit; i < ranking.size(); ++i) {
            remaining_total += ranking[i].used_bytes;
        }
        ss << "... remaining " << (ranking.size() - limit) << " loaders hold "
           << formatBytes(remaining_total) << "\n";
    }

    if (suspicious_count > 0) {
        ss << "\n[Alert] " << suspicious_count
           << " loader(s) flagged for heavy Metaspace usage. Investigate potential leaks or class "
              "redefinition loops.\n";
    }

    ss << "\n[Next steps]\n";
    ss << "- Use jcmd GC.class_stats or VM.metaspace to cross-validate chunk consumption.\n";
    ss << "- Inspect highlighted loaders for excessive dynamic class generation, redefining or "
          "ClassLoader leaks.\n";

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
