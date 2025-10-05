#include "jni_thread.h"

namespace jvmtool {

ScopedAttach::ScopedAttach(JavaVM* java_vm) noexcept : vm_(java_vm) {
    if (vm_ == nullptr) {
        error_ = "JavaVM is null";
        return;
    }
    void* env_void = nullptr;
    const jint attach_rc = vm_->AttachCurrentThread(&env_void, nullptr);
    if (attach_rc == JNI_OK) {
        env_ = static_cast<JNIEnv*>(env_void);
        attached_ = true;
    } else {
        error_ = "AttachCurrentThread failed, rc=" + std::to_string(attach_rc);
    }
}

ScopedAttach::~ScopedAttach() {
    if (attached_ && vm_ != nullptr) {
        vm_->DetachCurrentThread();
    }
}

}  // namespace jvmtool
