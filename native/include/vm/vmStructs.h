/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 *
 * Modified by: jvmtool project
 * Modifications (2025-10-23):
 * - Added new offset variables for precise Metaspace memory measurement:
 *   * InstanceKlass: _size_helper, _vtable_len, _itable_len, _fields, _annotations
 *   * ConstantPool: _length, _cache
 *   * ConstMethod: _code_size
 * - Added static accessor methods for new offsets (constMethodCodeSizeOffset, etc.)
 * - Added VMKlass methods for safe memory access:
 *   * instanceKlassSizeInWords(), vtableLength(), itableLength()
 *   * methodsArray(), constantPool()
 * - Modified method signatures for const-correctness (at(), methodCount())
 * - Added SafeAccess integration for pointer dereference protection
 */

#ifndef _VMSTRUCTS_H
#define _VMSTRUCTS_H

#include <jvmti.h>
#include <stdint.h>
#include <string.h>

#include <type_traits>

#include "codeCache.h"
#include "safeAccess.h"

namespace jvmtool {

// Sanity check constants for VMStructs operations
constexpr int32_t MAX_REASONABLE_KLASS_SIZE_WORDS = 2048;       // ~16KB max for InstanceKlass
constexpr int32_t MAX_REASONABLE_VTABLE_LENGTH = 10000;         // Max vtable entries
constexpr int32_t MAX_REASONABLE_ITABLE_LENGTH = 10000;         // Max itable entries
constexpr int32_t MAX_REASONABLE_METHOD_COUNT = 100000;         // Max methods in a class
constexpr int32_t MAX_REASONABLE_CP_LENGTH = 100000;            // Max constant pool entries
constexpr int32_t MAX_REASONABLE_CODE_SIZE = 10 * 1024 * 1024;  // 10MB max bytecode per method

class VMStructs {
  protected:
    enum { MONITOR_BIT = 2 };

    static CodeCache* _libjvm;

    static bool _has_class_names;
    static bool _has_method_structs;
    static bool _has_compiler_structs;
    static bool _has_stack_structs;
    static bool _has_class_loader_data;
    static bool _has_native_thread_id;
    static bool _has_perm_gen;
    static bool _can_dereference_jmethod_id;
    static bool _compact_object_headers;

    static int _klass_name_offset;
    static int _symbol_length_offset;
    static int _symbol_length_and_refcount_offset;
    static int _symbol_body_offset;
    static int _oop_klass_offset;
    static int _class_loader_data_offset;
    static int _class_loader_data_next_offset;
    static int _methods_offset;
    static int _jmethod_ids_offset;
    static int _thread_osthread_offset;
    static int _thread_anchor_offset;
    static int _thread_state_offset;
    static int _thread_vframe_offset;
    static int _thread_exception_offset;
    static int _osthread_id_offset;
    static int _call_wrapper_anchor_offset;
    static int _comp_env_offset;
    static int _comp_task_offset;
    static int _comp_method_offset;
    static int _anchor_sp_offset;
    static int _anchor_pc_offset;
    static int _anchor_fp_offset;
    static int _frame_size_offset;
    static int _frame_complete_offset;
    static int _code_offset;
    static int _data_offset;
    static int _mutable_data_offset;
    static int _relocation_size_offset;
    static int _scopes_pcs_offset;
    static int _scopes_data_offset;
    static int _nmethod_name_offset;
    static int _nmethod_method_offset;
    static int _nmethod_entry_offset;
    static int _nmethod_state_offset;
    static int _nmethod_level_offset;
    static int _nmethod_metadata_offset;
    static int _nmethod_immutable_offset;
    static int _method_constmethod_offset;
    static int _method_code_offset;
    static int _constmethod_constants_offset;
    static int _constmethod_idnum_offset;
    static int _constmethod_size;
    static int _pool_holder_offset;
    static int _array_len_offset;
    static int _array_data_offset;

    // === InstanceKlass offsets for precise Metaspace measurement ===
    static int _instance_klass_size_offset;         // _size_helper
    static int _instance_klass_vtable_len_offset;   // _vtable_len
    static int _instance_klass_itable_len_offset;   // _itable_len
    static int _instance_klass_fields_offset;       // _fields
    static int _instance_klass_annotations_offset;  // _annotations

    // === ConstantPool offsets for precise measurement ===
    static int _constantpool_length_offset;  // _length
    static int _constantpool_cache_offset;   // _cache

    // === ConstMethod offsets for precise measurement ===
    static int _constmethod_code_size_offset;  // _code_size
    static int _code_heap_memory_offset;
    static int _code_heap_segmap_offset;
    static int _code_heap_segment_shift;
    static int _heap_block_used_offset;
    static int _vs_low_bound_offset;
    static int _vs_high_bound_offset;
    static int _vs_low_offset;
    static int _vs_high_offset;
    static int _flag_name_offset;
    static int _flag_addr_offset;
    static int _flag_origin_offset;
    static const char* _flags_addr;
    static int _flag_count;
    static int _flag_size;
    static char* _code_heap[3];
    static const void* _code_heap_low;
    static const void* _code_heap_high;
    static char** _code_heap_addr;
    static const void** _code_heap_low_addr;
    static const void** _code_heap_high_addr;
    static int* _klass_offset_addr;
    static char** _narrow_klass_base_addr;
    static char* _narrow_klass_base;
    static int* _narrow_klass_shift_addr;
    static int _narrow_klass_shift;
    static char** _collected_heap_addr;
    static char* _collected_heap;
    static int _collected_heap_reserved_offset;
    static int _region_start_offset;
    static int _region_size_offset;
    static int _markword_klass_shift;
    static int _markword_monitor_value;
    static int _entry_frame_call_wrapper_offset;
    static int _interpreter_frame_bcp_offset;
    static unsigned char _unsigned5_base;
    static const void** _call_stub_return_addr;
    static const void* _call_stub_return;
    static const void* _interpreted_frame_valid_start;
    static const void* _interpreted_frame_valid_end;

    static jfieldID _eetop;
    static jfieldID _tid;
    static jfieldID _klass;
    static int _tls_index;
    static intptr_t _env_offset;
    static void* _java_thread_vtbl[6];

    typedef void (*LockFunc)(void*);
    static LockFunc _lock_func;
    static LockFunc _unlock_func;

    static uintptr_t readSymbol(const char* symbol_name);
    static void initOffsets();
    static void resolveOffsets();
    static void patchSafeFetch();
    static void initJvmFunctions();
    static void initTLS(void* vm_thread);
    static void initThreadBridge();

    const char* at(int offset) const {
        return (const char*)this + offset;
    }

    static bool goodPtr(const void* ptr) {
        return (uintptr_t)ptr >= 0x1000 && ((uintptr_t)ptr & (sizeof(uintptr_t) - 1)) == 0;
    }

    template <typename T>
    static T align(const void* ptr) {
        static_assert(std::is_pointer<T>::value, "T must be a pointer type");
        return (T)((uintptr_t)ptr & ~(sizeof(T) - 1));
    }

  public:
    static void init(CodeCache* libjvm);
    static void ready();

    static CodeCache* libjvm() {
        return _libjvm;
    }

    static bool hasClassNames() {
        return _has_class_names;
    }

    static bool hasMethodStructs() {
        return _has_method_structs;
    }

    static bool hasCompilerStructs() {
        return _has_compiler_structs;
    }

    static bool hasStackStructs() {
        return _has_stack_structs;
    }

    static bool hasClassLoaderData() {
        return _has_class_loader_data;
    }

    static bool hasNativeThreadId() {
        return _has_native_thread_id;
    }

    static bool hasJavaThreadId() {
        return _tid != NULL;
    }

    static bool isInterpretedFrameValidFunc(const void* pc) {
        return pc >= _interpreted_frame_valid_start && pc < _interpreted_frame_valid_end;
    }

    // Accessor methods for newly added offsets (Phase 1 & 2)
    static int constMethodCodeSizeOffset() {
        return _constmethod_code_size_offset;
    }

    static int constantPoolCacheOffset() {
        return _constantpool_cache_offset;
    }

    static int instanceKlassSizeHelperOffset() {
        return _instance_klass_size_offset;
    }

    static int instanceKlassFieldsOffset() {
        return _instance_klass_fields_offset;
    }

    static int instanceKlassAnnotationsOffset() {
        return _instance_klass_annotations_offset;
    }
};

class MethodList {
  public:
    enum { SIZE = 8 };

  private:
    intptr_t _method[SIZE];
    int _ptr;
    MethodList* _next;
    int _padding;

  public:
    MethodList(MethodList* next) : _ptr(0), _next(next), _padding(0) {
        for (int i = 0; i < SIZE; i++) {
            _method[i] = 0x37;
        }
    }
};

class NMethod;
class VMMethod;

class VMSymbol : VMStructs {
  public:
    unsigned short length() {
        if (_symbol_length_offset >= 0) {
            return *(unsigned short*)at(_symbol_length_offset);
        } else {
            return *(unsigned int*)at(_symbol_length_and_refcount_offset) >> 16;
        }
    }

    const char* body() {
        return at(_symbol_body_offset);
    }
};

class ClassLoaderData : VMStructs {
  private:
    void* mutex() {
        return *(void**)at(sizeof(uintptr_t) * 3);
    }

  public:
    void lock() {
        _lock_func(mutex());
    }

    void unlock() {
        _unlock_func(mutex());
    }

    MethodList** methodList() {
        return (MethodList**)at(sizeof(uintptr_t) * 6 + 8);
    }
};

class VMKlass : VMStructs {
  public:
    static VMKlass* fromJavaClass(JNIEnv* env, jclass cls) {
        if (_has_perm_gen) {
            jobject klassOop = env->GetObjectField(cls, _klass);
            return (VMKlass*)(*(uintptr_t**)klassOop + 2);
        } else if (sizeof(VMKlass*) == 8) {
            return (VMKlass*)(uintptr_t)env->GetLongField(cls, _klass);
        } else {
            return (VMKlass*)(uintptr_t)env->GetIntField(cls, _klass);
        }
    }

    static VMKlass* fromHandle(uintptr_t handle) {
        if (_has_perm_gen) {
            // On JDK 7 KlassHandle is a pointer to klassOop, hence one more indirection
            return (VMKlass*)(*(uintptr_t**)handle + 2);
        } else {
            return (VMKlass*)handle;
        }
    }

    static VMKlass* fromOop(uintptr_t oop) {
        if (_narrow_klass_shift >= 0) {
            uintptr_t narrow_klass;
            if (_compact_object_headers) {
                uintptr_t mark = *(uintptr_t*)oop;
                if (mark & MONITOR_BIT) {
                    mark = *(uintptr_t*)(mark ^ MONITOR_BIT);
                }
                narrow_klass = mark >> _markword_klass_shift;
            } else {
                narrow_klass = *(unsigned int*)(oop + _oop_klass_offset);
            }
            return (VMKlass*)(_narrow_klass_base + (narrow_klass << _narrow_klass_shift));
        } else {
            return *(VMKlass**)(oop + _oop_klass_offset);
        }
    }

    VMSymbol* name() {
        return *(VMSymbol**)at(_klass_name_offset);
    }

    ClassLoaderData* classLoaderData() {
        return *(ClassLoaderData**)at(_class_loader_data_offset);
    }

    int methodCount() const {
        int* methods = *(int**)at(_methods_offset);
        return methods == NULL ? 0 : *methods & 0xffff;
    }

    jmethodID* jmethodIDs() {
        return __atomic_load_n((jmethodID**)at(_jmethod_ids_offset), __ATOMIC_ACQUIRE);
    }

    // === Precise Metaspace Memory Measurement Methods ===

    /**
     * Get InstanceKlass actual size in words (need to multiply by sizeof(uintptr_t) to get bytes)
     * @return size in words, -1 if not supported or unreasonable
     */
    int instanceKlassSizeInWords() const {
        if (_instance_klass_size_offset >= 0) {
            int32_t size = SafeAccess::load32((int32_t*)at(_instance_klass_size_offset), -1);
            // Sanity check: InstanceKlass should be between 64 bytes and 16KB
            if (size > 0 && size <= MAX_REASONABLE_KLASS_SIZE_WORDS) {
                return size;
            }
        }
        return -1;
    }

    /**
     * Get virtual method table length
     * @return VTable length, -1 if not supported or unreasonable
     */
    int vtableLength() const {
        if (_instance_klass_vtable_len_offset >= 0) {
            int32_t len = SafeAccess::load32((int32_t*)at(_instance_klass_vtable_len_offset), -1);
            // Sanity check: vtable length should be reasonable
            if (len >= 0 && len <= MAX_REASONABLE_VTABLE_LENGTH) {
                return len;
            }
        }
        return -1;
    }

    /**
     * Get interface method table length
     * @return ITable length, -1 if not supported or unreasonable
     */
    int itableLength() const {
        if (_instance_klass_itable_len_offset >= 0) {
            int32_t len = SafeAccess::load32((int32_t*)at(_instance_klass_itable_len_offset), -1);
            // Sanity check: itable length should be reasonable
            if (len >= 0 && len <= MAX_REASONABLE_ITABLE_LENGTH) {
                return len;
            }
        }
        return -1;
    }

    /**
     * Get Methods array pointer (Array<Method*>*)
     * @return pointer to Methods array, nullptr if not supported
     */
    void* methodsArray() const {
        if (_methods_offset >= 0) {
            return SafeAccess::load((void**)at(_methods_offset), nullptr);
        }
        return nullptr;
    }

    /**
     * Get ConstantPool pointer
     * Note: We get it through Method -> ConstMethod -> ConstantPool path
     * @return pointer to ConstantPool, nullptr if not available
     */
    void* constantPool() const {
        if (_constmethod_constants_offset >= 0) {
            // Get ConstantPool via: Methods[0] -> ConstMethod -> ConstantPool
            void* methods_array = methodsArray();
            if (methods_array && methodCount() > 0) {
                // Get first Method from Array<Method*>
                void** methods_data = (void**)((char*)methods_array + _array_data_offset);
                void* method = SafeAccess::load(methods_data, nullptr);
                if (method && goodPtr(method)) {
                    // Method::_constMethod
                    void* const_method = SafeAccess::load(
                        (void**)((char*)method + _method_constmethod_offset), nullptr);
                    if (const_method && goodPtr(const_method)) {
                        // ConstMethod::_constants
                        return SafeAccess::load(
                            (void**)((char*)const_method + _constmethod_constants_offset), nullptr);
                    }
                }
            }
        }
        return nullptr;
    }
};

class JavaFrameAnchor : VMStructs {
  private:
    enum { MAX_CALL_WRAPPER_DISTANCE = 512 };

  public:
    static JavaFrameAnchor* fromEntryFrame(uintptr_t fp) {
        const char* call_wrapper = *(const char**)(fp + _entry_frame_call_wrapper_offset);
        if (!goodPtr(call_wrapper) || (uintptr_t)call_wrapper - fp > MAX_CALL_WRAPPER_DISTANCE) {
            return NULL;
        }
        return (JavaFrameAnchor*)(call_wrapper + _call_wrapper_anchor_offset);
    }

    uintptr_t lastJavaSP() {
        return *(uintptr_t*)at(_anchor_sp_offset);
    }

    uintptr_t lastJavaFP() {
        return *(uintptr_t*)at(_anchor_fp_offset);
    }

    const void* lastJavaPC() {
        return *(const void**)at(_anchor_pc_offset);
    }

    void setLastJavaPC(const void* pc) {
        *(const void**)at(_anchor_pc_offset) = pc;
    }
};

class VMThread : VMStructs {
  public:
    static VMThread* current();

    static int key() {
        return _tls_index;
    }

    static VMThread* fromJavaThread(JNIEnv* env, jthread thread) {
        return (VMThread*)(uintptr_t)env->GetLongField(thread, _eetop);
    }

    static jlong javaThreadId(JNIEnv* env, jthread thread) {
        return env->GetLongField(thread, _tid);
    }

    static int nativeThreadId(JNIEnv* jni, jthread thread);

    int osThreadId();

    JNIEnv* jni();

    const void** vtable() {
        return *(const void***)this;
    }

    // This thread is considered a JavaThread if at least 2 of the selected 3 vtable entries
    // match those of a known JavaThread (which is either application thread or AttachListener).
    // Indexes were carefully chosen to work on OpenJDK 8 to 25, both product an debug builds.
    bool isJavaThread() {
        const void** vtbl = vtable();
        return (vtbl[1] == _java_thread_vtbl[1]) + (vtbl[3] == _java_thread_vtbl[3]) +
                   (vtbl[5] == _java_thread_vtbl[5]) >=
               2;
    }

    int state() {
        return _thread_state_offset >= 0 ? *(int*)at(_thread_state_offset) : 0;
    }

    bool inJava() {
        return state() == 8;
    }

    bool inDeopt() {
        return *(void**)at(_thread_vframe_offset) != NULL;
    }

    void*& exception() {
        return *(void**)at(_thread_exception_offset);
    }

    JavaFrameAnchor* anchor() {
        return (JavaFrameAnchor*)at(_thread_anchor_offset);
    }

    VMMethod* compiledMethod() {
        const char* env = *(const char**)at(_comp_env_offset);
        if (env != NULL) {
            const char* task = *(const char**)(env + _comp_task_offset);
            if (task != NULL) {
                return *(VMMethod**)(task + _comp_method_offset);
            }
        }
        return NULL;
    }
};

class VMMethod : VMStructs {
  public:
    jmethodID id();

    // Performs extra validation when VMMethod comes from incomplete frame
    jmethodID validatedId();

    // Workaround for JDK-8313816
    static bool isStaleMethodId(jmethodID id) {
        if (!_can_dereference_jmethod_id)
            return false;
        VMMethod* vm_method = *(VMMethod**)id;
        return vm_method == NULL || vm_method->id() == NULL;
    }

    const char* bytecode() {
        return *(const char**)at(_method_constmethod_offset) + _constmethod_size;
    }

    NMethod* code() {
        return *(NMethod**)at(_method_code_offset);
    }
};

class NMethod : VMStructs {
  public:
    int frameSize() {
        return *(int*)at(_frame_size_offset);
    }

    short frameCompleteOffset() {
        return *(short*)at(_frame_complete_offset);
    }

    void setFrameCompleteOffset(int offset) {
        if (_nmethod_immutable_offset > 0) {
            // _frame_complete_offset is short on JDK 23+
            *(short*)at(_frame_complete_offset) = offset;
        } else {
            *(int*)at(_frame_complete_offset) = offset;
        }
    }

    const char* immutableDataAt(int offset) {
        if (_nmethod_immutable_offset > 0) {
            return *(const char**)at(_nmethod_immutable_offset) + offset;
        }
        return at(offset);
    }

    const char* code() {
        if (_code_offset > 0) {
            return at(*(int*)at(_code_offset));
        } else {
            return *(const char**)at(-_code_offset);
        }
    }

    const char* scopes() {
        if (_scopes_data_offset > 0) {
            return immutableDataAt(*(int*)at(_scopes_data_offset));
        } else {
            return *(const char**)at(-_scopes_data_offset);
        }
    }

    const void* entry() {
        if (_nmethod_entry_offset > 0) {
            return at(*(int*)at(_code_offset) + *(unsigned short*)at(_nmethod_entry_offset));
        } else {
            return *(void**)at(-_nmethod_entry_offset);
        }
    }

    bool isFrameCompleteAt(const void* pc) {
        return pc >= code() + frameCompleteOffset();
    }

    bool isEntryFrame(const void* pc) {
        return pc == _call_stub_return;
    }

    const char* name() {
        return *(const char**)at(_nmethod_name_offset);
    }

    bool isNMethod() {
        const char* n = name();
        return n != NULL && (strcmp(n, "nmethod") == 0 || strcmp(n, "native nmethod") == 0);
    }

    bool isInterpreter() {
        const char* n = name();
        return n != NULL && strcmp(n, "Interpreter") == 0;
    }

    VMMethod* method() {
        return *(VMMethod**)at(_nmethod_method_offset);
    }

    char state() {
        return *at(_nmethod_state_offset);
    }

    bool isAlive() {
        return state() >= 0 && state() <= 1;
    }

    int level() {
        return _nmethod_level_offset >= 0 ? *(signed char*)at(_nmethod_level_offset) : 0;
    }

    VMMethod** metadata() {
        if (_mutable_data_offset >= 0) {
            // Since JDK 25
            return (VMMethod**)(*(char**)at(_mutable_data_offset) +
                                *(int*)at(_relocation_size_offset));
        } else if (_data_offset > 0) {
            // since JDK 23
            return (VMMethod**)at(*(int*)at(_data_offset) +
                                  *(unsigned short*)at(_nmethod_metadata_offset));
        }
        return (VMMethod**)at(*(int*)at(_nmethod_metadata_offset));
    }

    int findScopeOffset(const void* pc);
};

class CodeHeap : VMStructs {
  private:
    static bool contains(char* heap, const void* pc) {
        return heap != NULL &&
               pc >= *(const void**)(heap + _code_heap_memory_offset + _vs_low_offset) &&
               pc < *(const void**)(heap + _code_heap_memory_offset + _vs_high_offset);
    }

    static NMethod* findNMethod(char* heap, const void* pc);

  public:
    static bool available() {
        return _code_heap_addr != NULL;
    }

    static bool contains(const void* pc) {
        return _code_heap_low <= pc && pc < _code_heap_high;
    }

    static void updateBounds(const void* start, const void* end) {
        for (const void* low = _code_heap_low;
             start < low && !__sync_bool_compare_and_swap(&_code_heap_low, low, start);
             low = _code_heap_low)
            ;
        for (const void* high = _code_heap_high;
             end > high && !__sync_bool_compare_and_swap(&_code_heap_high, high, end);
             high = _code_heap_high)
            ;
    }

    static NMethod* findNMethod(const void* pc) {
        if (contains(_code_heap[0], pc))
            return findNMethod(_code_heap[0], pc);
        if (contains(_code_heap[1], pc))
            return findNMethod(_code_heap[1], pc);
        if (contains(_code_heap[2], pc))
            return findNMethod(_code_heap[2], pc);
        return NULL;
    }
};

class CollectedHeap : VMStructs {
  public:
    static bool created() {
        return _collected_heap_addr != NULL && *_collected_heap_addr != NULL;
    }

    static CollectedHeap* heap() {
        return (CollectedHeap*)_collected_heap;
    }

    uintptr_t start() {
        return *(uintptr_t*)at(_region_start_offset);
    }

    uintptr_t size() {
        return (*(uintptr_t*)at(_region_size_offset)) * sizeof(uintptr_t);
    }
};

class JVMFlag : VMStructs {
  private:
    enum { ORIGIN_DEFAULT = 0, ORIGIN_MASK = 15, SET_ON_CMDLINE = 1 << 17 };

  public:
    static JVMFlag* find(const char* name);

    const char* name() {
        return *(const char**)at(_flag_name_offset);
    }

    char* addr() {
        return *(char**)at(_flag_addr_offset);
    }

    bool isDefault() {
        return _flag_origin_offset < 0 ||
               (*(int*)at(_flag_origin_offset) & ORIGIN_MASK) == ORIGIN_DEFAULT;
    }

    void setCmdline() {
        if (_flag_origin_offset >= 0) {
            *(int*)at(_flag_origin_offset) |= SET_ON_CMDLINE;
        }
    }

    char get() {
        return *addr();
    }

    void set(char value) {
        *addr() = value;
    }
};

class PcDesc {
  public:
    int _pc;
    int _scope_offset;
    int _obj_offset;
    int _flags;
};

class ScopeDesc : VMStructs {
  private:
    const unsigned char* _scopes;
    VMMethod** _metadata;
    const unsigned char* _stream;
    int _method_offset;
    int _bci;

    int readInt();

  public:
    ScopeDesc(NMethod* nm) {
        _scopes = (const unsigned char*)nm->scopes();
        _metadata = nm->metadata();
    }

    int decode(int offset) {
        _stream = _scopes + offset;
        int sender_offset = readInt();
        _method_offset = readInt();
        _bci = readInt() - 1;
        return sender_offset;
    }

    VMMethod* method() {
        return _method_offset > 0 ? _metadata[_method_offset - 1] : NULL;
    }

    int bci() {
        return _bci;
    }
};

class InterpreterFrame : VMStructs {
  public:
    enum { sender_sp_offset = -1, method_offset = -3 };

    static int bcp_offset() {
        return _interpreter_frame_bcp_offset;
    }
};

}  // namespace jvmtool

#endif  // _VMSTRUCTS_H
