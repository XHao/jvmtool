#pragma once

#include <jni.h>
#include <jvmti.h>
#include <sys/types.h>  // for pid_t

#include <array>
#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "agent.h"
#include "message.h"
#include "vm/vmStructs.h"
#include "writer.h"

namespace jvmtool {

struct ClassLoaderSpaceUsage {
    size_t committed_bytes{0};
    size_t used_bytes{0};
    size_t hidden_committed_bytes{0};
    size_t hidden_used_bytes{0};
    size_t fallback_total_bytes{0};

    size_t totalCommittedBytes() const {
        size_t total = committed_bytes + hidden_committed_bytes;
        if (total == 0) {
            total = fallback_total_bytes;
        }
        return total;
    }

    size_t totalUsedBytes() const {
        size_t total = used_bytes + hidden_used_bytes;
        if (total == 0) {
            total = fallback_total_bytes;
        }
        return total;
    }
};

struct ClassLoaderMetadataUsage {
    size_t klass_bytes{0};
    size_t method_bytes{0};
    size_t constant_pool_bytes{0};

    size_t total() const {
        return klass_bytes + method_bytes + constant_pool_bytes;
    }
};

struct ClassLoaderStats {
    std::string loader_name;
    std::string module_name;
    std::string loader_address;
    std::string parent_address;
    std::string cld_address;
    size_t class_count{0};
    size_t hidden_classes{0};
    ClassLoaderSpaceUsage space;
    ClassLoaderMetadataUsage metadata;

    size_t usedBytes() const {
        size_t used = space.totalUsedBytes();
        if (used == 0) {
            used = metadata.total();
        }
        return used;
    }

    size_t committedBytes() const {
        size_t committed = space.totalCommittedBytes();
        if (committed == 0) {
            committed = usedBytes();
        }
        return committed;
    }
};

struct ClassLoaderSummary {
    size_t loader_count{0};
    size_t total_class_count{0};
    size_t total_hidden_classes{0};
    size_t aggregated_committed_bytes{0};
    size_t aggregated_used_bytes{0};
    size_t aggregated_hidden_committed_bytes{0};
    size_t aggregated_hidden_used_bytes{0};
    size_t aggregated_metadata_bytes{0};
    size_t reported_loader_count{0};
    size_t reported_total_classes{0};
    size_t reported_total_chunk_bytes{0};
    size_t reported_total_block_bytes{0};

    size_t totalCommittedBytes() const {
        return aggregated_committed_bytes + aggregated_hidden_committed_bytes;
    }

    size_t totalUsedBytes() const {
        return aggregated_used_bytes + aggregated_hidden_used_bytes;
    }
};

enum class StatsTableFormat { Unknown, Index, Chunk };

class ClassLoaderStatsBuilder {
  public:
    ClassLoaderStatsBuilder(std::vector<ClassLoaderStats>& out, ClassLoaderSummary* summary);

    bool addLine(const std::string& line);

  private:
    static bool startsWith(const std::string& line, const char* prefix);

    bool detectHeader(const std::string& line);
    void parseTotals(const std::string& line);
    bool parseHiddenLine(const std::string& line);
    bool parseIndexRow(const std::string& line);
    bool parseChunkRow(const std::string& line);

    std::vector<ClassLoaderStats>& out_;
    ClassLoaderSummary* summary_;
    bool header_parsed_{false};
    StatsTableFormat format_{StatsTableFormat::Unknown};
    std::unordered_map<std::string, size_t> column_index_{};
    ClassLoaderStats* current_{nullptr};
};

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
    // Mirrors MetaspaceShared::_shared_metaspace_base; resolved once via VMStructs to obtain the
    // CDS shared Metaspace start address from the target JVM.
    static void** _shared_metaspace_base_addr;
    // Mirrors MetaspaceShared::_shared_metaspace_top; provides the end of the CDS shared region.
    static void** _shared_metaspace_top_addr;
    // Cached value fetched from *_addr to avoid repeat lookups; points to the CDS base.
    static void* _shared_metaspace_base;
    // Cached CDS region top address derived from *_addr for fast range checks and size math.
    static void* _shared_metaspace_top;

    // Global metaspace accounting fields (word counters in HotSpot).
    static size_t* _metaspace_capacity_until_gc_addr;
    static size_t* _metaspace_used_words_addr;
    static size_t* _metaspace_committed_words_addr;
    static size_t _metaspace_capacity_until_gc_words;
    static size_t _metaspace_used_words;
    static size_t _metaspace_committed_words;

    // Compressed class space virtual space descriptors.
    static void** _compressed_class_space_virtual_space_addr;
    static void* _compressed_class_space_virtual_space;
    static void* _compressed_class_space_low;
    static void* _compressed_class_space_high;
    static void* _compressed_class_space_low_boundary;
    static void* _compressed_class_space_high_boundary;

    // Cached offsets within VirtualSpace for boundary extraction.
    static int _virtual_space_low_offset;
    static int _virtual_space_high_offset;
    static int _virtual_space_low_boundary_offset;
    static int _virtual_space_high_boundary_offset;

    // Initialize metaspace-specific offsets
    static void initMetaspaceOffsets();
    static void resolveMetaspaceOffsets();

  public:
    // Initialization methods
    static void init(CodeCache* libjvm);
    static void ready();

    static bool collectClassLoaderStats(JNIEnv* env, std::vector<ClassLoaderStats>& out,
                                        ClassLoaderSummary* summary = nullptr);

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

    static size_t capacityUntilGCWords() {
        return _metaspace_capacity_until_gc_words;
    }

    static size_t committedWords() {
        return _metaspace_committed_words;
    }

    static size_t usedWords() {
        return _metaspace_used_words;
    }

    static void* compressedClassSpaceLow() {
        return _compressed_class_space_low;
    }

    static void* compressedClassSpaceHigh() {
        return _compressed_class_space_high;
    }

    static void* compressedClassSpaceLowBoundary() {
        return _compressed_class_space_low_boundary;
    }

    static void* compressedClassSpaceHighBoundary() {
        return _compressed_class_space_high_boundary;
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

std::array<const char*, 2> SelectClassStatsCommands(int hotspot_version);

}  // namespace jvmtool
