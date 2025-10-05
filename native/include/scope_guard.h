#pragma once

#include <utility>

// A tiny, reusable RAII scope-exit guard.
// Usage:
//   auto _guard = jvmtool::make_scope_exit([&]{ cleanup(); });
// The lambda runs when the guard goes out of scope, regardless of return/exception.
namespace jvmtool {

template <typename F>
class ScopeExit {
  public:
    explicit ScopeExit(F&& f) noexcept : fn_(std::forward<F>(f)), active_(true) {}
    ScopeExit(ScopeExit&& other) noexcept : fn_(std::move(other.fn_)), active_(other.active_) {
        other.active_ = false;
    }
    ScopeExit(const ScopeExit&) = delete;
    ScopeExit& operator=(const ScopeExit&) = delete;
    ~ScopeExit() noexcept {
        if (active_) {
            try {
                fn_();
            } catch (...) {
            }
        }
    }

    void dismiss() noexcept {
        active_ = false;
    }

  private:
    F fn_;
    bool active_;
};

template <typename F>
inline ScopeExit<F> make_scope_exit(F&& f) {
    return ScopeExit<F>(std::forward<F>(f));
}

}  // namespace jvmtool
