#include "metaspace/module.h"

#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <iostream>
#include <sstream>
#include <thread>

#include "common.h"
#include "deadline_scheduler.h"
#include "jni_thread.h"
#include "metaspace/classloader_analyzer.h"
#include "metaspace/classloader_formatter.h"
#include "metaspace/detect_analyzer.h"
#include "metaspace/structs.h"
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
            Message msg(
                agentType(), ERROR,
                std::string("[Native SA] Failed to attach monitoring thread: ") + attach.error());
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
        ss << "- Attempting basic ClassLoader analysis via JVMTI...\n\n";
    } else {
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

        if (MetaspaceStructs::hasClassLoaderDataSupport()) {
            ss << "- ClassLoaderData traversal: supported\n";
        } else {
            ss << "- ClassLoaderData traversal: not supported (will use JVMTI only)\n";
        }

        if (MetaspaceStructs::hasKlassDetails()) {
            ss << "- Klass detail analysis: supported\n";
        } else {
            ss << "- Klass detail analysis: not supported (will use estimates)\n";
        }
        ss << "\n";
    }

    // Perform hybrid ClassLoader analysis using new API
    try {
        // Configure analysis options
        ClassLoaderAnalysisOptions opts = ClassLoaderAnalysisOptions::detailed();
        opts.max_classes_per_loader = 50;  // Analyze top 50 classes per loader
        opts.top_n = 10;                   // Return top 10 loaders

        ClassLoaderSummary summary;
        std::vector<ClassLoaderInfo> loaders =
            ClassLoaderAnalyzer::analyze(jvmti_, env, opts, &summary);

        // Format and output summary report
        std::string report = ClassLoaderReportFormatter::formatSummary(loaders, summary, 10);
        ss << report;

        // Show detailed class breakdown for loaders with detailed info
        ss << "\n";
        size_t detail_count = 0;
        for (const auto& loader : loaders) {
            if (loader.has_detailed_class_info && !loader.classes.empty()) {
                // Show top 20 classes for the first detailed loader, top 10 for others
                size_t num_classes = (detail_count == 0) ? 20 : 10;
                std::string detail_report =
                    ClassLoaderReportFormatter::formatClassDetails(loader, num_classes);
                ss << detail_report << "\n";
                detail_count++;
            }
        }

        // Run pattern analysis on all loaders with detailed class info
        std::vector<ClassLoaderInfo> loaders_with_details;
        for (const auto& loader : loaders) {
            if (loader.has_detailed_class_info) {
                loaders_with_details.push_back(loader);
            }
        }

        if (!loaders_with_details.empty()) {
            std::vector<DetectionResult> patterns = DetectAnalyzer::analyze(loaders_with_details);
            std::string pattern_report = DetectAnalyzer::formatReport(patterns, true);
            ss << pattern_report;
        }

        // Clean up global references
        for (auto& loader : loaders) {
            if (loader.loader_object != nullptr) {
                env->DeleteGlobalRef(loader.loader_object);
            }
            for (auto& class_info : loader.classes) {
                if (class_info.class_ref != nullptr) {
                    env->DeleteGlobalRef(class_info.class_ref);
                }
            }
        }
    } catch (const std::exception& e) {
        ss << "\n[Error] ClassLoader analysis failed: " << e.what() << "\n";
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
}  // namespace

}  // namespace jvmtool
