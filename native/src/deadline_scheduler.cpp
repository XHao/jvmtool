#include "deadline_scheduler.h"

#include <cassert>

namespace jvmtool {

DeadlineScheduler& DeadlineScheduler::instance() {
    static DeadlineScheduler inst;
    return inst;
}

DeadlineScheduler::DeadlineScheduler() = default;

DeadlineScheduler::~DeadlineScheduler() {
    {
        std::lock_guard<std::mutex> lk(mtx_);
        stop_.store(true, std::memory_order_relaxed);
    }
    cv_.notify_all();
    if (thread_.joinable()) {
        thread_.join();
    }
}

DeadlineScheduler::TaskId DeadlineScheduler::registerTask(TimePoint deadline,
                                                          std::function<void()> cancel) {
    std::scoped_lock lk(mtx_);
    const TaskId id = next_id_++;
    Task t{id, deadline, std::move(cancel)};
    active_.emplace(id, t);
    pq_.push(t);
    if (!thread_.joinable() && !stop_.load(std::memory_order_relaxed)) {
        // Lazy start worker
        thread_ = std::thread([this] { loop(); });
    }
    cv_.notify_all();
    return id;
}

void DeadlineScheduler::unregisterTask(TaskId id) {
    std::scoped_lock lk(mtx_);
    active_.erase(id);  // lazy removal from pq_
    cv_.notify_all();
}

void DeadlineScheduler::loop() {
    std::unique_lock<std::mutex> lk(mtx_);
    while (!stop_.load(std::memory_order_relaxed)) {
        if (pq_.empty()) {
            // No tasks: exit the worker to avoid an always-on thread.
            // New registrations will lazy-start a fresh worker.
            return;
        }

        // Skip tasks that have been unregistered (lazy removal)
        while (!pq_.empty() && active_.find(pq_.top().id) == active_.end()) {
            pq_.pop();
        }
        if (pq_.empty()) {
            continue;
        }

        const auto next = pq_.top();
        const auto now = Clock::now();
        if (now < next.deadline) {
            // Wait until the current deadline OR until notified (new task/stop/unregister).
            // No predicate here so that newly scheduled earlier deadlines preempt quickly.
            cv_.wait_until(lk, next.deadline);
            continue;  // re-evaluate top/active set
        }

        // Deadline reached; fire cancel outside the lock
        pq_.pop();
        auto it = active_.find(next.id);
        if (it != active_.end()) {
            auto cancel = std::move(it->second.cancel);
            active_.erase(it);
            lk.unlock();
            try {
                if (cancel)
                    cancel();
            } catch (...) {
            }
            lk.lock();
        }
    }
}

}  // namespace jvmtool
