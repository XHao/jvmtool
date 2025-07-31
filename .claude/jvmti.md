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
#define CHECK_JVMTI_ERROR(err, msg) \
    if (err != JVMTI_ERROR_NONE) { \
        fprintf(stderr, "JVMTI Error: %s - %d\n", msg, err); \
        return err; \
    }
```

## 🎯 Considerations When Generating Code

1. **Always Consider Thread Safety**: JVMTI events may execute concurrently
2. **Manage Memory Properly**: Distinguish between JVMTI allocated and standard library allocated memory
3. **Use Appropriate Synchronization**: Prioritize RawMonitor
4. **Follow JVMTI Event Model**: Understand different event triggering timing and constraints
5. **Error Handling**: Check return values of all JVMTI functions
6. **Resource Cleanup**: Ensure resources are released at appropriate times

---

**Version**: 1.0  
**Scope**: JVMTI Agent Development  
**Use Cases**: AI-assisted programming, code review, best practice reference