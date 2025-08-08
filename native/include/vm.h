/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 * 
 * Modified by: shako
 * Modifications: 
 * - Simplified VM implementation for jvmtool compatibility
 * - Removed OpenJ9 and Zing JVM support completely, only supports HotSpot JVM
 */

#ifndef _VM_H
#define _VM_H

#include <jni.h>
#include <jvmti.h>

/**
 * Simplified VM interface for jvmtool
 * This provides only HotSpot JVM support for jvmtool compatibility
 */
class VM {
public:
    static JavaVM* _vm;
    static jvmtiEnv* _jvmti;
    static int _hotspot_version;

public:
    /**
     * Initialize the VM interface
     */
    static void init(JavaVM* vm, jvmtiEnv* jvmti);
    
    /**
     * Get the JavaVM instance
     */
    static JavaVM* vm() {
        return _vm;
    }
    
    /**
     * Get the JVMTI environment
     */
    static jvmtiEnv* jvmti() {
        return _jvmti;
    }
    
    /**
     * Get JNI environment for current thread
     */
    static JNIEnv* jni();
    
    /**
     * Get HotSpot version
     */
    static int hotspot_version() {
        return _hotspot_version;
    }
    
    /**
     * Detect and set JVM version and type
     */
    static void detectJVM();
    
    /**
     * Attach current thread to JVM
     */
    static JNIEnv* attachThread(const char* name);
    
    /**
     * Detach current thread from JVM
     */
    static void detachThread();
};

// Define WX_MEMORY for compatibility with original code
#ifndef WX_MEMORY
#define WX_MEMORY false
#endif

#endif // _VM_H
