#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_map>

namespace jvmtool {

class DeadlineScheduler {
  public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    using TaskId = std::uint64_t;

    static DeadlineScheduler& instance();

    TaskId registerTask(TimePoint deadline, std::function<void()> cancel);

    void unregisterTask(TaskId id);

  private:
    DeadlineScheduler();
    ~DeadlineScheduler();
    DeadlineScheduler(const DeadlineScheduler&) = delete;
    DeadlineScheduler& operator=(const DeadlineScheduler&) = delete;

    void loop();

    struct Task {
        TaskId id;
        TimePoint deadline;
        std::function<void()> cancel;
    };

    struct EarlierDeadline {
        bool operator()(const Task& a, const Task& b) const noexcept {
            return a.deadline > b.deadline;
        }
    };

    std::mutex mtx_;
    std::condition_variable cv_;
    std::atomic<bool> stop_{false};
    std::thread thread_;
    TaskId next_id_{1};

    std::priority_queue<Task, std::vector<Task>, EarlierDeadline> pq_;
    std::unordered_map<TaskId, Task> active_;
};

class DeadlineRegistration {
  public:
    DeadlineRegistration() = default;
    explicit DeadlineRegistration(DeadlineScheduler::TaskId id) : id_(id) {}
    DeadlineRegistration(const DeadlineRegistration&) = delete;
    DeadlineRegistration& operator=(const DeadlineRegistration&) = delete;
    DeadlineRegistration(DeadlineRegistration&& other) noexcept {
        id_ = other.id_;
        other.id_ = 0;
    }
    DeadlineRegistration& operator=(DeadlineRegistration&& other) noexcept {
        if (this != &other) {
            reset();
            id_ = other.id_;
            other.id_ = 0;
        }
        return *this;
    }
    ~DeadlineRegistration() {
        reset();
    }

    void dismiss() noexcept {
        id_ = 0;
    }
    bool valid() const noexcept {
        return id_ != 0;
    }

  private:
    void reset() noexcept {
        if (id_ != 0) {
            DeadlineScheduler::instance().unregisterTask(id_);
            id_ = 0;
        }
    }

    DeadlineScheduler::TaskId id_{0};
};

inline DeadlineRegistration schedule_stop_on_deadline(std::atomic<bool>& stop_flag,
                                                      DeadlineScheduler::TimePoint deadline) {
    auto id = DeadlineScheduler::instance().registerTask(
        deadline, [&stop_flag]() noexcept { stop_flag.store(true, std::memory_order_relaxed); });
    return DeadlineRegistration{id};
}
}  // namespace jvmtool
