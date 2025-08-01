# JVMTI Agent Programming Best Practices - AI Coding Prompt

## 📋 Overview

This prompt document is designed to guide AI-assisted JVMTI (JVM Tool Interface) Agent development, providing principles and best practices to ensure generated code meets JVM integration requirements and thread safety standards.

## 🎯 Core Principles

### 1. **Prioritize JVM-provided Capabilities**
- Prioritize using JVMTI/JNI provided functions for operations interacting with the JVM
- Maintain consistency with the JVM to avoid potential conflicts

### 2. **Ensure Thread Safety**
- Understand the JVMTI event callback concurrency model
- Correctly use synchronization mechanisms to protect shared resources

### 3. **Maintain Memory Management Consistency**
- Use JVMTI's memory management functions for JVM-related data

## 🔐 Thread Safety and Synchronization

### RawMonitor Mechanism

**Use Case**: Protect Agent internal global variables and data structures

```c
// Protect Agent internal shared resources and data structures
static jrawMonitorID data_monitor;
static HashTable* method_stats;
static int total_calls = 0;

void JNICALL MethodEntry(jvmtiEnv *jvmti_env, JNIEnv* jni_env, jthread thread, jmethodID method) {
    // Protect our Agent's internal shared resources
    (*jvmti_env)->RawMonitorEnter(jvmti_env, data_monitor);
    
    // Safely access shared data
    update_method_stats(method_stats, method);
    total_calls++;
    
    (*jvmti_env)->RawMonitorExit(jvmti_env, data_monitor);
}
```

**Initialize Monitor**:
```c
JNIEXPORT jint JNICALL Agent_OnLoad(JavaVM *vm, char *options, void *reserved) {
    jvmtiEnv *jvmti;
    (*vm)->GetEnv((void**)&jvmti, JVMTI_VERSION);
    
    // Create monitor for synchronization
    jrawMonitorID monitor;
    jvmtiError err = (*jvmti)->CreateRawMonitor(jvmti, "agent_monitor", &monitor);
    
    return JNI_OK;
}
```

**Synchronization Scope Clarification**:
✅ Agent global variables  
✅ Custom data structures  
✅ Shared file handles and resources  

❌ No need to protect JVM internal mechanisms  
❌ JVMTI function calls themselves  
❌ JVM event dispatching mechanism  

## 💾 Memory Management

### Use JVMTI Memory Management Functions

```c
// Recommended: Use JVMTI memory management
unsigned char* jvmti_buffer;
jvmtiError err = (*jvmti)->Allocate(jvmti, 1024, &jvmti_buffer);
if (err == JVMTI_ERROR_NONE) {
    // Use buffer...
    // Release memory
    (*jvmti)->Deallocate(jvmti, jvmti_buffer);
}
```

### Handle Strings Returned by JVMTI

```c
char* method_name;
char* method_signature;
char* method_generic;

jvmtiError err = (*jvmti)->GetMethodName(jvmti, method_id, 
                                        &method_name,
                                        &method_signature, 
                                        &method_generic);
if (err == JVMTI_ERROR_NONE) {
    // Use method name...
    // Remember to release JVMTI allocated memory
    (*jvmti)->Deallocate(jvmti, (unsigned char*)method_name);
    (*jvmti)->Deallocate(jvmti, (unsigned char*)method_signature);
    (*jvmti)->Deallocate(jvmti, (unsigned char*)method_generic);
}
```

## 🔧 JVMTI Capabilities and Events

### Capability Management

```c
// Set required capabilities during Agent_OnLoad
jvmtiCapabilities capabilities;
memset(&capabilities, 0, sizeof(capabilities));

// Enable specific capabilities
capabilities.can_generate_method_entry_events = 1;
capabilities.can_generate_method_exit_events = 1;
capabilities.can_get_source_file_name = 1;
capabilities.can_get_line_numbers = 1;
capabilities.can_generate_exception_events = 1;

jvmtiError err = (*jvmti)->AddCapabilities(jvmti, &capabilities);
if (err != JVMTI_ERROR_NONE) {
    logJvmtiError(err, "AddCapabilities");
    return JNI_ERR;
}
```

### Event Callbacks Registration

```c
// Set up event callbacks
jvmtiEventCallbacks callbacks;
memset(&callbacks, 0, sizeof(callbacks));

// Register callback functions
callbacks.MethodEntry = &MethodEntry;
callbacks.MethodExit = &MethodExit;
callbacks.Exception = &Exception;
callbacks.ClassLoad = &ClassLoad;

err = (*jvmti)->SetEventCallbacks(jvmti, &callbacks, sizeof(callbacks));
if (err != JVMTI_ERROR_NONE) {
    logJvmtiError(err, "SetEventCallbacks");
    return JNI_ERR;
}

// Enable events
err = (*jvmti)->SetEventNotificationMode(jvmti, JVMTI_ENABLE, 
                                        JVMTI_EVENT_METHOD_ENTRY, NULL);
```

### Global References Management

```c
// Create global references for objects that persist across events
jobject global_ref = NULL;

void JNICALL ClassLoad(jvmtiEnv *jvmti_env, JNIEnv* jni_env, 
                      jthread thread, jclass klass) {
    // Create global reference to keep class accessible
    global_ref = (*jni_env)->NewGlobalRef(jni_env, klass);
    if (global_ref == NULL) {
        // Handle out of memory
        return;
    }
}

// Remember to delete global references in Agent_OnUnload
void cleanup() {
    if (global_ref != NULL) {
        (*jni_env)->DeleteGlobalRef(jni_env, global_ref);
        global_ref = NULL;
    }
}
```

## 🚨 Error Handling and Debugging

### Comprehensive Error Checking

```c
#define CHECK_JVMTI_ERROR(err, msg) \
    do { \
        if ((err) != JVMTI_ERROR_NONE) { \
            logJvmtiError((err), (msg)); \
            return (err); \
        } \
    } while(0)

jvmtiError getMethodInfo(jmethodID method) {
    char* method_name = NULL;
    char* method_signature = NULL;
    
    jvmtiError err = (*jvmti)->GetMethodName(jvmti, method, 
                                           &method_name, 
                                           &method_signature, 
                                           NULL);
    CHECK_JVMTI_ERROR(err, "GetMethodName");
    
    // Use method info...
    
    // Cleanup
    if (method_name) (*jvmti)->Deallocate(jvmti, (unsigned char*)method_name);
    if (method_signature) (*jvmti)->Deallocate(jvmti, (unsigned char*)method_signature);
    
    return JVMTI_ERROR_NONE;
}
```

### JNI Exception Handling

```c
void JNICALL MethodEntry(jvmtiEnv *jvmti_env, JNIEnv* jni_env, 
                        jthread thread, jmethodID method) {
    // Check for pending JNI exceptions
    if ((*jni_env)->ExceptionCheck(jni_env)) {
        (*jni_env)->ExceptionClear(jni_env);
        return; // Skip processing if there's a pending exception
    }
    
    // Your processing code here...
    
    // Check again after operations
    if ((*jni_env)->ExceptionCheck(jni_env)) {
        (*jni_env)->ExceptionDescribe(jni_env);
        (*jni_env)->ExceptionClear(jni_env);
    }
}
```

## 🎯 Agent Lifecycle Management

### Proper Agent Initialization

```c
JNIEXPORT jint JNICALL Agent_OnLoad(JavaVM *vm, char *options, void *reserved) {
    jvmtiEnv *jvmti = NULL;
    jvmtiError err;
    
    // Get JVMTI environment
    jint result = (*vm)->GetEnv((void**)&jvmti, JVMTI_VERSION);
    if (result != JNI_OK || jvmti == NULL) {
        std::cerr << "Failed to get JVMTI environment" << std::endl;
        return JNI_ERR;
    }
    
    // Initialize capabilities first
    err = initializeCapabilities(jvmti);
    if (err != JVMTI_ERROR_NONE) {
        return JNI_ERR;
    }
    
    // Set up event callbacks
    err = setupEventCallbacks(jvmti);
    if (err != JVMTI_ERROR_NONE) {
        return JNI_ERR;
    }
    
    // Enable required events
    err = enableEvents(jvmti);
    if (err != JVMTI_ERROR_NONE) {
        return JNI_ERR;
    }
    
    return JNI_OK;
}
```

### Agent Cleanup

```c
JNIEXPORT void JNICALL Agent_OnUnload(JavaVM *vm) {
    // Disable all events
    if (global_jvmti != NULL) {
        (*global_jvmti)->SetEventNotificationMode(global_jvmti, JVMTI_DISABLE, 
                                                  JVMTI_EVENT_METHOD_ENTRY, NULL);
        (*global_jvmti)->SetEventNotificationMode(global_jvmti, JVMTI_DISABLE, 
                                                  JVMTI_EVENT_METHOD_EXIT, NULL);
    }
    
    // Clean up global references
    cleanup_global_references();
    
    // Destroy monitors
    if (global_monitor != NULL && global_jvmti != NULL) {
        (*global_jvmti)->DestroyRawMonitor(global_jvmti, global_monitor);
        global_monitor = NULL;
    }
    
    // Final cleanup
    global_jvmti = NULL;
}
```

## 🔍 Advanced JVMTI Features

### Heap Walking and Object Iteration

```c
// Heap object callback
jvmtiIterationControl JNICALL heap_object_callback(jlong class_tag, 
                                                   jlong size, 
                                                   jlong* tag_ptr, 
                                                   void* user_data) {
    // Process heap object
    HeapStats* stats = (HeapStats*)user_data;
    stats->total_objects++;
    stats->total_size += size;
    
    return JVMTI_ITERATION_CONTINUE;
}

// Iterate over heap
void analyzeHeap(jvmtiEnv *jvmti) {
    HeapStats stats = {0};
    jvmtiError err = (*jvmti)->IterateOverHeap(jvmti, JVMTI_HEAP_OBJECT_EITHER, 
                                              heap_object_callback, &stats);
    CHECK_JVMTI_ERROR(err, "IterateOverHeap");
}
```

### Stack Frame Inspection

```c
void analyzeStackTrace(jvmtiEnv *jvmti, jthread thread) {
    jint frame_count;
    jvmtiFrameInfo* frames;
    
    jvmtiError err = (*jvmti)->GetStackTrace(jvmti, thread, 0, 100, 
                                           &frames, &frame_count);
    CHECK_JVMTI_ERROR(err, "GetStackTrace");
    
    for (int i = 0; i < frame_count; i++) {
        jmethodID method = frames[i].method;
        jlocation location = frames[i].location;
        
        // Get method information
        char* method_name = NULL;
        char* class_name = NULL;
        
        jclass declaring_class;
        err = (*jvmti)->GetMethodDeclaringClass(jvmti, method, &declaring_class);
        if (err == JVMTI_ERROR_NONE) {
            err = (*jvmti)->GetClassSignature(jvmti, declaring_class, &class_name, NULL);
        }
        
        err = (*jvmti)->GetMethodName(jvmti, method, &method_name, NULL, NULL);
        
        // Process frame information...
        
        // Cleanup
        if (method_name) (*jvmti)->Deallocate(jvmti, (unsigned char*)method_name);
        if (class_name) (*jvmti)->Deallocate(jvmti, (unsigned char*)class_name);
    }
    
    (*jvmti)->Deallocate(jvmti, (unsigned char*)frames);
}
```

## 📊 Information Retrieval

### Use JVMTI to Get JVM Information

```c
// Recommended: Use JVMTI to get system properties
char* java_home_prop;
jvmtiError err = (*jvmti)->GetSystemProperty(jvmti, "java.home", &java_home_prop);
if (err == JVMTI_ERROR_NONE) {
    // Use property value...
    (*jvmti)->Deallocate(jvmti, (unsigned char*)java_home_prop);
}
```

## ⚠️ Standard Library Usage to Avoid

### 1. **Thread Synchronization Mechanisms**
```c
// ❌ Avoid using pthread mutex or other standard library synchronization
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

// ✅ Use JVMTI RawMonitor
jrawMonitorID monitor;
(*jvmti)->CreateRawMonitor(jvmti, "my_monitor", &monitor);
```

### 2. **Direct Memory Management**
```c
// ❌ Avoid mixing malloc/free with JVMTI allocated memory
```

## ✅ Standard Library Functions That Are Safe to Use

### 1. **Complex Data Structures**
```c
// ✅ Can use third-party or custom data structure libraries
#include <uthash.h>  // Hash table
#include <queue.h>   // Queue
```

### 2. **Mathematical Operations**
```c
// ✅ Can use standard math library
#include <math.h>
double result = sqrt(value);
```

### 3. **String Processing**
```c
// ✅ Can use standard string functions to process data returned by JVMTI
```

## 🛠️ Core Utility Classes and Functions

### 1. **Memory Management Tools**
```c
jvmtiError Allocate(jvmtiEnv* env, jlong size, unsigned char** mem_ptr);
jvmtiError Deallocate(jvmtiEnv* env, unsigned char* mem);
```

### 2. **Thread Synchronization Tools**
```c
jvmtiError CreateRawMonitor(jvmtiEnv* env, const char* name, jrawMonitorID* monitor_ptr);
jvmtiError RawMonitorEnter(jvmtiEnv* env, jrawMonitorID monitor);
jvmtiError RawMonitorExit(jvmtiEnv* env, jrawMonitorID monitor);
```

### 3. **Object and Class Information Tools**
```c
jvmtiError GetClassSignature(jvmtiEnv* env, jclass klass, char** signature_ptr, char** generic_ptr);
jvmtiError GetObjectClass(jvmtiEnv* env, jobject object, jclass* class_ptr);
```

### 4. **Method Information Tools**
```c
jvmtiError GetMethodName(jvmtiEnv* env, jmethodID method, char** name_ptr, char** signature_ptr, char** generic_ptr);
jvmtiError GetMethodDeclaringClass(jvmtiEnv* env, jmethodID method, jclass* declaring_class_ptr);
```

## 📚 Error Handling Best Practices

```c
// Centralized error logging function
void logJvmtiError(jvmtiError error, const char* context) {
    if (error == JVMTI_ERROR_NONE) return;
    
    const char* error_name = "UNKNOWN_ERROR";
    switch (error) {
        case JVMTI_ERROR_NULL_POINTER: error_name = "NULL_POINTER"; break;
        case JVMTI_ERROR_OUT_OF_MEMORY: error_name = "OUT_OF_MEMORY"; break;
        case JVMTI_ERROR_ACCESS_DENIED: error_name = "ACCESS_DENIED"; break;
        case JVMTI_ERROR_WRONG_PHASE: error_name = "WRONG_PHASE"; break;
        case JVMTI_ERROR_INTERNAL: error_name = "INTERNAL"; break;
        // Add more error codes as needed
    }
    
    fprintf(stderr, "JVMTI Error [%s]: %s (code: %d)\n", 
            context ? context : "Unknown", error_name, error);
}
```

## 🎯 Considerations When Generating Code

1. **Always Consider Thread Safety**: JVMTI events may execute concurrently
2. **Manage Memory Properly**: Distinguish between JVMTI allocated and standard library allocated memory
3. **Use Appropriate Synchronization**: Prioritize RawMonitor
4. **Follow JVMTI Event Model**: Understand different event triggering timing and constraints
5. **Error Handling**: Check return values of all JVMTI functions
6. **Resource Cleanup**: Ensure resources are released at appropriate times

## 🔒 Additional JVMTI Specifications and Constraints

### Agent Library Loading Rules
- **Agent_OnLoad**: Called during JVM startup, before main() execution
- **Agent_OnAttach**: Called when agent is dynamically attached to running JVM
- **Agent_OnUnload**: Called during JVM shutdown, cleanup opportunity
- **Library Threading**: Agent functions may be called from any JVM thread

### Event Callback Restrictions
- **No JVM State Modification**: Avoid modifying JVM state in certain callbacks
- **Deadlock Prevention**: Never acquire locks in order that conflicts with JVM internal locks
- **Performance Impact**: Minimize processing time in high-frequency events
- **Exception Safety**: Never let C++ exceptions propagate to JVM

### JVMTI Environment Thread Safety
- **Per-Thread Access**: JNIEnv is thread-local, jvmtiEnv is thread-safe
- **Capability Requirements**: Some functions require specific capabilities
- **Phase Restrictions**: Certain functions only available in specific phases

### Memory and Resource Constraints
- **Heap Walking Limitations**: Some heap iteration may suspend all threads
- **String Encoding**: JVMTI strings are Modified UTF-8 encoded
- **Reference Counting**: JNI local references have limited lifetime
- **Native Memory**: Agent is responsible for its own native memory management

### Platform Considerations
- **Dynamic Library**: Agent must be compiled as shared library (.so/.dll/.dylib)
- **JNI Version Compatibility**: Ensure compatibility with target JVM version
- **Symbol Visibility**: Properly export required agent entry points
- **Calling Conventions**: Follow platform-specific calling conventions

---

**Version**: 1.1  
**Scope**: JVMTI Agent Development  
**Use Cases**: AI-assisted programming, code review, best practice reference