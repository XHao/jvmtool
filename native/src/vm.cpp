/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 *
 * Modified by: shako
 * Modifications:
 * - Simplified VM implementation for jvmtool compatibility
 * - Removed OpenJ9 and Zing JVM support completely, only supports HotSpot JVM
 */

#include "vm.h"

#include <string.h>

namespace jvmtool {
// Static member definitions
JavaVM* VM::_vm = nullptr;
jvmtiEnv* VM::_jvmti = nullptr;
int VM::_hotspot_version = 11;  // Default to JDK 11

void VM::init(JavaVM* vm, jvmtiEnv* jvmti) {
    _vm = vm;
    _jvmti = jvmti;
    detectJVM();
}

JNIEnv* VM::jni() {
    if (_vm == nullptr) {
        return nullptr;
    }

    JNIEnv* jni;
    if (_vm->GetEnv((void**)&jni, JNI_VERSION_1_6) == JNI_OK) {
        return jni;
    }

    // Try to attach current thread
    return attachThread("jvmtool-thread");
}

void VM::detectJVM() {
    if (_vm == nullptr) {
        return;
    }

    JNIEnv* jni = VM::jni();
    if (jni == nullptr) {
        return;
    }

    // Only detect HotSpot version - other JVM implementations removed
    jclass systemClass = jni->FindClass("java/lang/System");
    if (systemClass == nullptr) {
        jni->ExceptionClear();
        return;
    }

    jmethodID getPropertyMethod = jni->GetStaticMethodID(systemClass, "getProperty",
                                                         "(Ljava/lang/String;)Ljava/lang/String;");
    if (getPropertyMethod == nullptr) {
        jni->ExceptionClear();
        jni->DeleteLocalRef(systemClass);
        return;
    }

    // Parse HotSpot Java version only
    jstring versionKey = jni->NewStringUTF("java.version");
    if (versionKey != nullptr) {
        jstring version =
            (jstring)jni->CallStaticObjectMethod(systemClass, getPropertyMethod, versionKey);
        if (version != nullptr && !jni->ExceptionCheck()) {
            const char* versionStr = jni->GetStringUTFChars(version, nullptr);
            if (versionStr != nullptr) {
                // Parse HotSpot version string (e.g., "11.0.1", "1.8.0_271", "17.0.2")
                if (strncmp(versionStr, "1.8", 3) == 0) {
                    _hotspot_version = 8;
                } else {
                    // For Java 9+ format (e.g., "11.0.1", "17.0.2")
                    int majorVersion = 11;  // default
                    sscanf(versionStr, "%d", &majorVersion);
                    if (majorVersion >= 8) {
                        _hotspot_version = majorVersion;
                    }
                }
                jni->ReleaseStringUTFChars(version, versionStr);
            }
            jni->DeleteLocalRef(version);
        }
        jni->DeleteLocalRef(versionKey);
    }

    jni->ExceptionClear();
    jni->DeleteLocalRef(systemClass);
}

JNIEnv* VM::attachThread(const char* name) {
    if (_vm == nullptr) {
        return nullptr;
    }

    JNIEnv* jni;
    JavaVMAttachArgs args;
    args.version = JNI_VERSION_1_6;
    args.name = const_cast<char*>(name);
    args.group = nullptr;

    if (_vm->AttachCurrentThreadAsDaemon((void**)&jni, &args) == JNI_OK) {
        return jni;
    }

    return nullptr;
}

void VM::detachThread() {
    if (_vm != nullptr) {
        _vm->DetachCurrentThread();
    }
}

}  // namespace jvmtool
