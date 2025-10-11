#include "metaspace/structs.h"

#include <cstring>

#include "common.h"
#include "vm/vm.h"

namespace jvmtool {
namespace {

template <typename T>
inline T ptrAt(uintptr_t base, uintptr_t offset) {
    return reinterpret_cast<T>(base + offset);  // NOLINT(performance-no-int-to-ptr)
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

    ClassLoaderStatsParser parser(out, summary_ptr);

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
                if (!parser.addLine(lineStr)) {
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
                if (!parser.addLine(line)) {
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

void MetaspaceStructs::init(CodeCache* libjvm) {
    if (libjvm != nullptr) {
        initMetaspaceOffsets();
    }
}

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

    _has_metaspace_structs =
        (_shared_metaspace_base_addr != nullptr && _shared_metaspace_top_addr != nullptr);
}

bool MetaspaceStructs::collectClassLoaderStats(JNIEnv* env, std::vector<ClassLoaderStats>& out,
                                               ClassLoaderSummary* summary,
                                               std::vector<std::string>* debug) {
    out.clear();

    if (summary != nullptr) {
        *summary = ClassLoaderSummary();
    }

    if (env == nullptr) {
        return false;
    }

    if (debug != nullptr) {
        debug->clear();
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

}  // namespace jvmtool
