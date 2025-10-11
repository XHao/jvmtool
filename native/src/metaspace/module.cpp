#include "metaspace/module.h"

#include "metaspace/class_loader_stats.h"
#include "metaspace/structs.h"

#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>
#include <utility>
#include <vector>

#include "common.h"
#include "deadline_scheduler.h"
#include "jni_thread.h"
#include "scope_guard.h"
#include "vm/vm.h"

namespace jvmtool {
namespace {

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

bool writeHeader(MessageWriter* writer, int fd, AgentType agent_type) {
    const Message header(agent_type, DATA, "[Native SA] === Metaspace Analysis ===\n");
    return writer != nullptr && writer->writeMessage(fd, header);
}

}  // namespace

MetaspaceSAModule::MetaspaceSAModule() = default;

MetaspaceSAModule::~MetaspaceSAModule() {
    stop_.store(true, std::memory_order_relaxed);
    joinThread(monitor_thread_);
}

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
            Message msg(agentType(), ERROR,
                        std::string("[Native SA] Failed to attach monitoring thread: ") +
                            attach.error());
            writeAndClose(fd, msg);
            return;
        }

        if (!writeHeader(writer_.get(), static_cast<int>(fd), agentType())) {
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
    (void)env;
    try {
        if (!MetaspaceStructs::hasMetaspaceStructs()) {
            Message warning(agentType(), STATUS,
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
}  // namespace

}  // namespace jvmtool
