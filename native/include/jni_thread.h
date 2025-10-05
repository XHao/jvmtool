#pragma once

#include <jni.h>

#include <string>

namespace jvmtool {

class ScopedAttach final {
  public:
    explicit ScopedAttach(JavaVM* java_vm) noexcept;
    ~ScopedAttach();

    ScopedAttach(const ScopedAttach&) = delete;
    ScopedAttach& operator=(const ScopedAttach&) = delete;

    [[nodiscard]] bool ok() const noexcept {
        return attached_;
    }
    [[nodiscard]] JNIEnv* env() const noexcept {
        return env_;
    }
    [[nodiscard]] const std::string& error() const noexcept {
        return error_;
    }

  private:
    JavaVM* vm_{};
    JNIEnv* env_{};
    bool attached_{false};
    std::string error_{};
};

}  // namespace jvmtool
