// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_ISOLATE_H_
#define V8_ISOLATE_H_

#include <cstddef>
#include <memory>
#include <queue>
#include <unordered_map>
#include <vector>

#include "include/v8-inspector.h"
#include "include/v8-internal.h"
#include "include/v8.h"
#include "src/allocation.h"
#include "src/base/atomicops.h"
#include "src/base/macros.h"
#include "src/builtins/builtins.h"
#include "src/contexts.h"
#include "src/date.h"
#include "src/debug/debug-interface.h"
#include "src/execution.h"
#include "src/futex-emulation.h"
#include "src/globals.h"
#include "src/handles.h"
#include "src/heap/factory.h"
#include "src/heap/heap.h"
#include "src/messages.h"
#include "src/objects/code.h"
#include "src/objects/debug-objects.h"
#include "src/runtime/runtime.h"
#include "src/unicode.h"

#ifdef V8_INTL_SUPPORT
#include "unicode/uversion.h"  // Define U_ICU_NAMESPACE.
// 'icu' does not work. Use U_ICU_NAMESPACE.
namespace U_ICU_NAMESPACE {

class RegexMatcher;

}  // namespace U_ICU_NAMESPACE
#endif  // V8_INTL_SUPPORT

namespace v8 {

namespace base {
class RandomNumberGenerator;
}

namespace debug {
class ConsoleDelegate;
}

namespace internal {

namespace heap {
class HeapTester;
}  // namespace heap

class AddressToIndexHashMap;
class AstStringConstants;
class Bootstrapper;
class BuiltinsConstantsTableBuilder;
class CancelableTaskManager;
class CodeEventDispatcher;
class CodeTracer;
class CompilationCache;
class CompilationStatistics;
class CompilerDispatcher;
class ContextSlotCache;
class Counters;
class Debug;
class DeoptimizerData;
class DescriptorLookupCache;
class EternalHandles;
class ExternalCallbackScope;
class HandleScopeImplementer;
class HeapObjectToIndexHashMap;
class HeapProfiler;
class InnerPointerToCodeCache;
class Logger;
class MaterializedObjectStore;
class Microtask;
class OptimizingCompileDispatcher;
class PromiseOnStack;
class RegExpStack;
class RootVisitor;
class RuntimeProfiler;
class SaveContext;
class SetupIsolateDelegate;
class Simulator;
class StartupDeserializer;
class StandardFrame;
class StubCache;
class ThreadManager;
class ThreadState;
class ThreadVisitor;  // Defined in v8threads.h
class TracingCpuProfilerImpl;
class UnicodeCache;
struct ManagedPtrDestructor;

template <StateTag Tag> class VMState;

namespace interpreter {
class Interpreter;
}

namespace compiler {
class PerIsolateCompilerCache;
}

namespace wasm {
class WasmEngine;
}

#define RETURN_FAILURE_IF_SCHEDULED_EXCEPTION(isolate) \
  do {                                                 \
    Isolate* __isolate__ = (isolate);                  \
    DCHECK(!__isolate__->has_pending_exception());     \
    if (__isolate__->has_scheduled_exception()) {      \
      return __isolate__->PromoteScheduledException(); \
    }                                                  \
  } while (false)

// Macros for MaybeHandle.

#define RETURN_VALUE_IF_SCHEDULED_EXCEPTION(isolate, value) \
  do {                                                      \
    Isolate* __isolate__ = (isolate);                       \
    DCHECK(!__isolate__->has_pending_exception());          \
    if (__isolate__->has_scheduled_exception()) {           \
      __isolate__->PromoteScheduledException();             \
      return value;                                         \
    }                                                       \
  } while (false)

#define RETURN_EXCEPTION_IF_SCHEDULED_EXCEPTION(isolate, T) \
  RETURN_VALUE_IF_SCHEDULED_EXCEPTION(isolate, MaybeHandle<T>())

#define ASSIGN_RETURN_ON_SCHEDULED_EXCEPTION_VALUE(isolate, dst, call, value) \
  do {                                                                        \
    Isolate* __isolate__ = (isolate);                                         \
    if (!(call).ToLocal(&dst)) {                                              \
      DCHECK(__isolate__->has_scheduled_exception());                         \
      __isolate__->PromoteScheduledException();                               \
      return value;                                                           \
    }                                                                         \
  } while (false)

#define RETURN_ON_SCHEDULED_EXCEPTION_VALUE(isolate, call, value) \
  do {                                                            \
    Isolate* __isolate__ = (isolate);                             \
    if ((call).IsNothing()) {                                     \
      DCHECK(__isolate__->has_scheduled_exception());             \
      __isolate__->PromoteScheduledException();                   \
      return value;                                               \
    }                                                             \
  } while (false)

/**
 * RETURN_RESULT_OR_FAILURE is used in functions with return type Object* (such
 * as "RUNTIME_FUNCTION(...) {...}" or "BUILTIN(...) {...}" ) to return either
 * the contents of a MaybeHandle<X>, or the "exception" sentinel value.
 * Example usage:
 *
 * RUNTIME_FUNCTION(Runtime_Func) {
 *   ...
 *   RETURN_RESULT_OR_FAILURE(
 *       isolate,
 *       FunctionWithReturnTypeMaybeHandleX(...));
 * }
 *
 * If inside a function with return type MaybeHandle<X> use RETURN_ON_EXCEPTION
 * instead.
 * If inside a function with return type Handle<X>, or Maybe<X> use
 * RETURN_ON_EXCEPTION_VALUE instead.
 */
#define RETURN_RESULT_OR_FAILURE(isolate, call)      \
  do {                                               \
    Handle<Object> __result__;                       \
    Isolate* __isolate__ = (isolate);                \
    if (!(call).ToHandle(&__result__)) {             \
      DCHECK(__isolate__->has_pending_exception());  \
      return ReadOnlyRoots(__isolate__).exception(); \
    }                                                \
    DCHECK(!__isolate__->has_pending_exception());   \
    return *__result__;                              \
  } while (false)

#define ASSIGN_RETURN_ON_EXCEPTION_VALUE(isolate, dst, call, value)  \
  do {                                                               \
    if (!(call).ToHandle(&dst)) {                                    \
      DCHECK((isolate)->has_pending_exception());                    \
      return value;                                                  \
    }                                                                \
  } while (false)

#define ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, dst, call)                \
  do {                                                                        \
    Isolate* __isolate__ = (isolate);                                         \
    ASSIGN_RETURN_ON_EXCEPTION_VALUE(__isolate__, dst, call,                  \
                                     ReadOnlyRoots(__isolate__).exception()); \
  } while (false)

#define ASSIGN_RETURN_ON_EXCEPTION(isolate, dst, call, T)  \
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(isolate, dst, call, MaybeHandle<T>())

#define THROW_NEW_ERROR(isolate, call, T)                       \
  do {                                                          \
    Isolate* __isolate__ = (isolate);                           \
    return __isolate__->Throw<T>(__isolate__->factory()->call); \
  } while (false)

#define THROW_NEW_ERROR_RETURN_FAILURE(isolate, call)         \
  do {                                                        \
    Isolate* __isolate__ = (isolate);                         \
    return __isolate__->Throw(*__isolate__->factory()->call); \
  } while (false)

#define THROW_NEW_ERROR_RETURN_VALUE(isolate, call, value) \
  do {                                                     \
    Isolate* __isolate__ = (isolate);                      \
    __isolate__->Throw(*__isolate__->factory()->call);     \
    return value;                                          \
  } while (false)

/**
 * RETURN_ON_EXCEPTION_VALUE conditionally returns the given value when the
 * given MaybeHandle is empty. It is typically used in functions with return
 * type Maybe<X> or Handle<X>. Example usage:
 *
 * Handle<X> Func() {
 *   ...
 *   RETURN_ON_EXCEPTION_VALUE(
 *       isolate,
 *       FunctionWithReturnTypeMaybeHandleX(...),
 *       Handle<X>());
 *   // code to handle non exception
 *   ...
 * }
 *
 * Maybe<bool> Func() {
 *   ..
 *   RETURN_ON_EXCEPTION_VALUE(
 *       isolate,
 *       FunctionWithReturnTypeMaybeHandleX(...),
 *       Nothing<bool>);
 *   // code to handle non exception
 *   return Just(true);
 * }
 *
 * If inside a function with return type MaybeHandle<X>, use RETURN_ON_EXCEPTION
 * instead.
 * If inside a function with return type Object*, use
 * RETURN_FAILURE_ON_EXCEPTION instead.
 */
#define RETURN_ON_EXCEPTION_VALUE(isolate, call, value)            \
  do {                                                             \
    if ((call).is_null()) {                                        \
      DCHECK((isolate)->has_pending_exception());                  \
      return value;                                                \
    }                                                              \
  } while (false)

/**
 * RETURN_FAILURE_ON_EXCEPTION conditionally returns the "exception" sentinel if
 * the given MaybeHandle is empty; so it can only be used in functions with
 * return type Object*, such as RUNTIME_FUNCTION(...) {...} or BUILTIN(...)
 * {...}. Example usage:
 *
 * RUNTIME_FUNCTION(Runtime_Func) {
 *   ...
 *   RETURN_FAILURE_ON_EXCEPTION(
 *       isolate,
 *       FunctionWithReturnTypeMaybeHandleX(...));
 *   // code to handle non exception
 *   ...
 * }
 *
 * If inside a function with return type MaybeHandle<X>, use RETURN_ON_EXCEPTION
 * instead.
 * If inside a function with return type Maybe<X> or Handle<X>, use
 * RETURN_ON_EXCEPTION_VALUE instead.
 */
#define RETURN_FAILURE_ON_EXCEPTION(isolate, call)                     \
  do {                                                                 \
    Isolate* __isolate__ = (isolate);                                  \
    RETURN_ON_EXCEPTION_VALUE(__isolate__, call,                       \
                              ReadOnlyRoots(__isolate__).exception()); \
  } while (false);

/**
 * RETURN_ON_EXCEPTION conditionally returns an empty MaybeHandle<T> if the
 * given MaybeHandle is empty. Use it to return immediately from a function with
 * return type MaybeHandle when an exception was thrown. Example usage:
 *
 * MaybeHandle<X> Func() {
 *   ...
 *   RETURN_ON_EXCEPTION(
 *       isolate,
 *       FunctionWithReturnTypeMaybeHandleY(...),
 *       X);
 *   // code to handle non exception
 *   ...
 * }
 *
 * If inside a function with return type Object*, use
 * RETURN_FAILURE_ON_EXCEPTION instead.
 * If inside a function with return type
 * Maybe<X> or Handle<X>, use RETURN_ON_EXCEPTION_VALUE instead.
 */
#define RETURN_ON_EXCEPTION(isolate, call, T)  \
  RETURN_ON_EXCEPTION_VALUE(isolate, call, MaybeHandle<T>())


#define FOR_WITH_HANDLE_SCOPE(isolate, loop_var_type, init, loop_var,      \
                              limit_check, increment, body)                \
  do {                                                                     \
    loop_var_type init;                                                    \
    loop_var_type for_with_handle_limit = loop_var;                        \
    Isolate* for_with_handle_isolate = isolate;                            \
    while (limit_check) {                                                  \
      for_with_handle_limit += 1024;                                       \
      HandleScope loop_scope(for_with_handle_isolate);                     \
      for (; limit_check && loop_var < for_with_handle_limit; increment) { \
        body                                                               \
      }                                                                    \
    }                                                                      \
  } while (false)

// Platform-independent, reliable thread identifier.
class ThreadId {
 public:
  // Creates an invalid ThreadId.
  ThreadId() { base::Relaxed_Store(&id_, kInvalidId); }

  ThreadId& operator=(const ThreadId& other) {
    base::Relaxed_Store(&id_, base::Relaxed_Load(&other.id_));
    return *this;
  }

  bool operator==(const ThreadId& other) const { return Equals(other); }

  // Returns ThreadId for current thread.
  static ThreadId Current() { return ThreadId(GetCurrentThreadId()); }

  // Returns invalid ThreadId (guaranteed not to be equal to any thread).
  static ThreadId Invalid() { return ThreadId(kInvalidId); }

  // Compares ThreadIds for equality.
  V8_INLINE bool Equals(const ThreadId& other) const {
    return base::Relaxed_Load(&id_) == base::Relaxed_Load(&other.id_);
  }

  // Checks whether this ThreadId refers to any thread.
  V8_INLINE bool IsValid() const {
    return base::Relaxed_Load(&id_) != kInvalidId;
  }

  // Converts ThreadId to an integer representation
  // (required for public API: V8::V8::GetCurrentThreadId).
  int ToInteger() const { return static_cast<int>(base::Relaxed_Load(&id_)); }

  // Converts ThreadId to an integer representation
  // (required for public API: V8::V8::TerminateExecution).
  static ThreadId FromInteger(int id) { return ThreadId(id); }

 private:
  static const int kInvalidId = -1;

  explicit ThreadId(int id) { base::Relaxed_Store(&id_, id); }

  static int AllocateThreadId();

  V8_EXPORT_PRIVATE static int GetCurrentThreadId();

  base::Atomic32 id_;

  static base::Atomic32 highest_thread_id_;

  friend class Isolate;
};

#define FIELD_ACCESSOR(type, name)                 \
  inline void set_##name(type v) { name##_ = v; }  \
  inline type name() const { return name##_; }

class ThreadLocalTop {
 public:
  // Does early low-level initialization that does not depend on the
  // isolate being present.
  ThreadLocalTop() = default;

  // Initialize the thread data.
  void Initialize(Isolate*);

  // Get the top C++ try catch handler or nullptr if none are registered.
  //
  // This method is not guaranteed to return an address that can be
  // used for comparison with addresses into the JS stack.  If such an
  // address is needed, use try_catch_handler_address.
  FIELD_ACCESSOR(v8::TryCatch*, try_catch_handler)

  // Get the address of the top C++ try catch handler or nullptr if
  // none are registered.
  //
  // This method always returns an address that can be compared to
  // pointers into the JavaScript stack.  When running on actual
  // hardware, try_catch_handler_address and TryCatchHandler return
  // the same pointer.  When running on a simulator with a separate JS
  // stack, try_catch_handler_address returns a JS stack address that
  // corresponds to the place on the JS stack where the C++ handler
  // would have been if the stack were not separate.
  Address try_catch_handler_address() {
    return reinterpret_cast<Address>(
        v8::TryCatch::JSStackComparableAddress(try_catch_handler()));
  }

  void Free();

  Isolate* isolate_ = nullptr;
  // The context where the current execution method is created and for variable
  // lookups.
  Context* context_ = nullptr;
  ThreadId thread_id_ = ThreadId::Invalid();
  Object* pending_exception_ = nullptr;

  // Communication channel between Isolate::FindHandler and the CEntry.
  Context* pending_handler_context_ = nullptr;
  Address pending_handler_entrypoint_ = kNullAddress;
  Address pending_handler_constant_pool_ = kNullAddress;
  Address pending_handler_fp_ = kNullAddress;
  Address pending_handler_sp_ = kNullAddress;

  // Communication channel between Isolate::Throw and message consumers.
  bool rethrowing_message_ = false;
  Object* pending_message_obj_ = nullptr;

  // Use a separate value for scheduled exceptions to preserve the
  // invariants that hold about pending_exception.  We may want to
  // unify them later.
  Object* scheduled_exception_ = nullptr;
  bool external_caught_exception_ = false;
  SaveContext* save_context_ = nullptr;

  // Stack.
  // The frame pointer of the top c entry frame.
  Address c_entry_fp_ = kNullAddress;
  // Try-blocks are chained through the stack.
  Address handler_ = kNullAddress;
  // C function that was called at c entry.
  Address c_function_ = kNullAddress;

  // Throwing an exception may cause a Promise rejection.  For this purpose
  // we keep track of a stack of nested promises and the corresponding
  // try-catch handlers.
  PromiseOnStack* promise_on_stack_ = nullptr;

#ifdef USE_SIMULATOR
  Simulator* simulator_ = nullptr;
#endif

  // The stack pointer of the bottom JS entry frame.
  Address js_entry_sp_ = kNullAddress;
  // The external callback we're currently in.
  ExternalCallbackScope* external_callback_scope_ = nullptr;
  StateTag current_vm_state_ = EXTERNAL;

  // Call back function to report unsafe JS accesses.
  v8::FailedAccessCheckCallback failed_access_check_callback_ = nullptr;

  // Address of the thread-local "thread in wasm" flag.
  Address thread_in_wasm_flag_address_ = kNullAddress;

 private:
  v8::TryCatch* try_catch_handler_ = nullptr;
};

#ifdef DEBUG

#define ISOLATE_INIT_DEBUG_ARRAY_LIST(V)               \
  V(CommentStatistic, paged_space_comments_statistics, \
    CommentStatistic::kMaxComments + 1)                \
  V(int, code_kind_statistics, AbstractCode::NUMBER_OF_KINDS)
#else

#define ISOLATE_INIT_DEBUG_ARRAY_LIST(V)

#endif

#define ISOLATE_INIT_ARRAY_LIST(V)                                             \
  /* SerializerDeserializer state. */                                          \
  V(int32_t, jsregexp_static_offsets_vector, kJSRegexpStaticOffsetsVectorSize) \
  V(int, bad_char_shift_table, kUC16AlphabetSize)                              \
  V(int, good_suffix_shift_table, (kBMMaxShift + 1))                           \
  V(int, suffix_table, (kBMMaxShift + 1))                                      \
  ISOLATE_INIT_DEBUG_ARRAY_LIST(V)

typedef std::vector<HeapObject*> DebugObjectCache;

#define ISOLATE_INIT_LIST(V)                                                  \
  /* Assembler state. */                                                      \
  V(FatalErrorCallback, exception_behavior, nullptr)                          \
  V(OOMErrorCallback, oom_behavior, nullptr)                                  \
  V(LogEventCallback, event_logger, nullptr)                                  \
  V(AllowCodeGenerationFromStringsCallback, allow_code_gen_callback, nullptr) \
  V(AllowWasmCodeGenerationCallback, allow_wasm_code_gen_callback, nullptr)   \
  V(ExtensionCallback, wasm_module_callback, &NoExtension)                    \
  V(ExtensionCallback, wasm_instance_callback, &NoExtension)                  \
  V(ApiImplementationCallback, wasm_compile_streaming_callback, nullptr)      \
  V(WasmStreamingCallback, wasm_streaming_callback, nullptr)                  \
  V(WasmThreadsEnabledCallback, wasm_threads_enabled_callback, nullptr)       \
  /* State for Relocatable. */                                                \
  V(Relocatable*, relocatable_top, nullptr)                                   \
  V(DebugObjectCache*, string_stream_debug_object_cache, nullptr)             \
  V(Object*, string_stream_current_security_token, nullptr)                   \
  V(const intptr_t*, api_external_references, nullptr)                        \
  V(AddressToIndexHashMap*, external_reference_map, nullptr)                  \
  V(HeapObjectToIndexHashMap*, root_index_map, nullptr)                       \
  V(CompilationStatistics*, turbo_statistics, nullptr)                        \
  V(CodeTracer*, code_tracer, nullptr)                                        \
  V(uint32_t, per_isolate_assert_data, 0xFFFFFFFFu)                           \
  V(PromiseRejectCallback, promise_reject_callback, nullptr)                  \
  V(const v8::StartupData*, snapshot_blob, nullptr)                           \
  V(int, code_and_metadata_size, 0)                                           \
  V(int, bytecode_and_metadata_size, 0)                                       \
  V(int, external_script_source_size, 0)                                      \
  /* true if being profiled. Causes collection of extra compile info. */      \
  V(bool, is_profiling, false)                                                \
  /* true if a trace is being formatted through Error.prepareStackTrace. */   \
  V(bool, formatting_stack_trace, false)                                      \
  /* Perform side effect checks on function call and API callbacks. */        \
  V(DebugInfo::ExecutionMode, debug_execution_mode, DebugInfo::kBreakpoints)  \
  /* Current code coverage mode */                                            \
  V(debug::Coverage::Mode, code_coverage_mode, debug::Coverage::kBestEffort)  \
  V(debug::TypeProfile::Mode, type_profile_mode, debug::TypeProfile::kNone)   \
  V(int, last_stack_frame_info_id, 0)                                         \
  V(int, last_console_context_id, 0)                                          \
  V(v8_inspector::V8Inspector*, inspector, nullptr)                           \
  V(bool, next_v8_call_is_safe_for_termination, false)                        \
  V(bool, only_terminate_in_safe_scope, false)                                \
  V(bool, detailed_source_positions_for_profiling, FLAG_detailed_line_info)

#define THREAD_LOCAL_TOP_ACCESSOR(type, name)                        \
  inline void set_##name(type v) { thread_local_top_.name##_ = v; }  \
  inline type name() const { return thread_local_top_.name##_; }

#define THREAD_LOCAL_TOP_ADDRESS(type, name) \
  type* name##_address() { return &thread_local_top_.name##_; }

// HiddenFactory exists so Isolate can privately inherit from it without making
// Factory's members available to Isolate directly.
class V8_EXPORT_PRIVATE HiddenFactory : private Factory {};

class Isolate : private HiddenFactory {
  // These forward declarations are required to make the friend declarations in
  // PerIsolateThreadData work on some older versions of gcc.
  class ThreadDataTable;
  class EntryStackItem;
 public:
  ~Isolate();

  // A thread has a PerIsolateThreadData instance for each isolate that it has
  // entered. That instance is allocated when the isolate is initially entered
  // and reused on subsequent entries.
  class PerIsolateThreadData {
   public:
    PerIsolateThreadData(Isolate* isolate, ThreadId thread_id)
        : isolate_(isolate),
          thread_id_(thread_id),
          stack_limit_(0),
          thread_state_(nullptr),
#if USE_SIMULATOR
          simulator_(nullptr),
#endif
          next_(nullptr),
          prev_(nullptr) {
    }
    ~PerIsolateThreadData();
    Isolate* isolate() const { return isolate_; }
    ThreadId thread_id() const { return thread_id_; }

    FIELD_ACCESSOR(uintptr_t, stack_limit)
    FIELD_ACCESSOR(ThreadState*, thread_state)

#if USE_SIMULATOR
    FIELD_ACCESSOR(Simulator*, simulator)
#endif

    bool Matches(Isolate* isolate, ThreadId thread_id) const {
      return isolate_ == isolate && thread_id_.Equals(thread_id);
    }

   private:
    Isolate* isolate_;
    ThreadId thread_id_;
    uintptr_t stack_limit_;
    ThreadState* thread_state_;

#if USE_SIMULATOR
    Simulator* simulator_;
#endif

    PerIsolateThreadData* next_;
    PerIsolateThreadData* prev_;

    friend class Isolate;
    friend class ThreadDataTable;
    friend class EntryStackItem;

    DISALLOW_COPY_AND_ASSIGN(PerIsolateThreadData);
  };

  static void InitializeOncePerProcess();

  // Returns the PerIsolateThreadData for the current thread (or nullptr if one
  // is not currently set).
  static PerIsolateThreadData* CurrentPerIsolateThreadData() {
    return reinterpret_cast<PerIsolateThreadData*>(
        base::Thread::GetThreadLocal(per_isolate_thread_data_key_));
  }

  // Returns the isolate inside which the current thread is running.
  V8_INLINE static Isolate* Current() {
    DCHECK_EQ(base::Relaxed_Load(&isolate_key_created_), 1);
    Isolate* isolate = reinterpret_cast<Isolate*>(
        base::Thread::GetExistingThreadLocal(isolate_key_));
    DCHECK_NOT_NULL(isolate);
    return isolate;
  }

  // Get the isolate that the given HeapObject lives in, returning true on
  // success. If the object is not writable (i.e. lives in read-only space),
  // return false.
  inline static bool FromWritableHeapObject(HeapObject* obj, Isolate** isolate);

  // Usually called by Init(), but can be called early e.g. to allow
  // testing components that require logging but not the whole
  // isolate.
  //
  // Safe to call more than once.
  void InitializeLoggingAndCounters();
  bool InitializeCounters();  // Returns false if already initialized.

  bool Init(StartupDeserializer* des);

  // True if at least one thread Enter'ed this isolate.
  bool IsInUse() { return entry_stack_ != nullptr; }

  // Destroys the non-default isolates.
  // Sets default isolate into "has_been_disposed" state rather then destroying,
  // for legacy API reasons.
  void TearDown();

  void ReleaseSharedPtrs();

  void ClearSerializerData();

  bool LogObjectRelocation();

  // Find the PerThread for this particular (isolate, thread) combination
  // If one does not yet exist, return null.
  PerIsolateThreadData* FindPerThreadDataForThisThread();

  // Find the PerThread for given (isolate, thread) combination
  // If one does not yet exist, return null.
  PerIsolateThreadData* FindPerThreadDataForThread(ThreadId thread_id);

  // Discard the PerThread for this particular (isolate, thread) combination
  // If one does not yet exist, no-op.
  void DiscardPerThreadDataForThisThread();

  // Returns the key used to store the pointer to the current isolate.
  // Used internally for V8 threads that do not execute JavaScript but still
  // are part of the domain of an isolate (like the context switcher).
  static base::Thread::LocalStorageKey isolate_key() {
    return isolate_key_;
  }

  // Returns the key used to store process-wide thread IDs.
  static base::Thread::LocalStorageKey thread_id_key() {
    return thread_id_key_;
  }

  static base::Thread::LocalStorageKey per_isolate_thread_data_key();

  // Mutex for serializing access to break control structures.
  base::RecursiveMutex* break_access() { return &break_access_; }

  Address get_address_from_id(IsolateAddressId id);

  // Access to top context (where the current function object was created).
  Context* context() { return thread_local_top_.context_; }
  inline void set_context(Context* context);
  Context** context_address() { return &thread_local_top_.context_; }

  THREAD_LOCAL_TOP_ACCESSOR(SaveContext*, save_context)

  // Access to current thread id.
  THREAD_LOCAL_TOP_ACCESSOR(ThreadId, thread_id)

  // Interface to pending exception.
  inline Object* pending_exception();
  inline void set_pending_exception(Object* exception_obj);
  inline void clear_pending_exception();

  bool AreWasmThreadsEnabled(Handle<Context> context);

  THREAD_LOCAL_TOP_ADDRESS(Object*, pending_exception)

  inline bool has_pending_exception();

  THREAD_LOCAL_TOP_ADDRESS(Context*, pending_handler_context)
  THREAD_LOCAL_TOP_ADDRESS(Address, pending_handler_entrypoint)
  THREAD_LOCAL_TOP_ADDRESS(Address, pending_handler_constant_pool)
  THREAD_LOCAL_TOP_ADDRESS(Address, pending_handler_fp)
  THREAD_LOCAL_TOP_ADDRESS(Address, pending_handler_sp)

  THREAD_LOCAL_TOP_ACCESSOR(bool, external_caught_exception)

  v8::TryCatch* try_catch_handler() {
    return thread_local_top_.try_catch_handler();
  }
  bool* external_caught_exception_address() {
    return &thread_local_top_.external_caught_exception_;
  }

  THREAD_LOCAL_TOP_ADDRESS(Object*, scheduled_exception)

  inline void clear_pending_message();
  Address pending_message_obj_address() {
    return reinterpret_cast<Address>(&thread_local_top_.pending_message_obj_);
  }

  inline Object* scheduled_exception();
  inline bool has_scheduled_exception();
  inline void clear_scheduled_exception();

  bool IsJavaScriptHandlerOnTop(Object* exception);
  bool IsExternalHandlerOnTop(Object* exception);

  inline bool is_catchable_by_javascript(Object* exception);

  // JS execution stack (see frames.h).
  static Address c_entry_fp(ThreadLocalTop* thread) {
    return thread->c_entry_fp_;
  }
  static Address handler(ThreadLocalTop* thread) { return thread->handler_; }
  Address c_function() { return thread_local_top_.c_function_; }

  inline Address* c_entry_fp_address() {
    return &thread_local_top_.c_entry_fp_;
  }
  inline Address* handler_address() { return &thread_local_top_.handler_; }
  inline Address* c_function_address() {
    return &thread_local_top_.c_function_;
  }

  // Bottom JS entry.
  Address js_entry_sp() {
    return thread_local_top_.js_entry_sp_;
  }
  inline Address* js_entry_sp_address() {
    return &thread_local_top_.js_entry_sp_;
  }

  // Returns the global object of the current context. It could be
  // a builtin object, or a JS global object.
  inline Handle<JSGlobalObject> global_object();

  // Returns the global proxy object of the current context.
  inline Handle<JSObject> global_proxy();

  static int ArchiveSpacePerThread() { return sizeof(ThreadLocalTop); }
  void FreeThreadResources() { thread_local_top_.Free(); }

  // This method is called by the api after operations that may throw
  // exceptions.  If an exception was thrown and not handled by an external
  // handler the exception is scheduled to be rethrown when we return to running
  // JavaScript code.  If an exception is scheduled true is returned.
  V8_EXPORT_PRIVATE bool OptionalRescheduleException(bool is_bottom_call);

  // Push and pop a promise and the current try-catch handler.
  void PushPromise(Handle<JSObject> promise);
  void PopPromise();

  // Return the relevant Promise that a throw/rejection pertains to, based
  // on the contents of the Promise stack
  Handle<Object> GetPromiseOnStackOnThrow();

  // Heuristically guess whether a Promise is handled by user catch handler
  bool PromiseHasUserDefinedRejectHandler(Handle<Object> promise);

  class ExceptionScope {
   public:
    // Scope currently can only be used for regular exceptions,
    // not termination exception.
    inline explicit ExceptionScope(Isolate* isolate);
    inline ~ExceptionScope();

   private:
    Isolate* isolate_;
    Handle<Object> pending_exception_;
  };

  void SetCaptureStackTraceForUncaughtExceptions(
      bool capture,
      int frame_limit,
      StackTrace::StackTraceOptions options);

  void SetAbortOnUncaughtExceptionCallback(
      v8::Isolate::AbortOnUncaughtExceptionCallback callback);

  enum PrintStackMode { kPrintStackConcise, kPrintStackVerbose };
  void PrintCurrentStackTrace(FILE* out);
  void PrintStack(StringStream* accumulator,
                  PrintStackMode mode = kPrintStackVerbose);
  V8_EXPORT_PRIVATE void PrintStack(FILE* out,
                                    PrintStackMode mode = kPrintStackVerbose);
  Handle<String> StackTraceString();
  // Stores a stack trace in a stack-allocated temporary buffer which will
  // end up in the minidump for debugging purposes.
  V8_NOINLINE void PushStackTraceAndDie(void* ptr1 = nullptr,
                                        void* ptr2 = nullptr,
                                        void* ptr3 = nullptr,
                                        void* ptr4 = nullptr);
  Handle<FixedArray> CaptureCurrentStackTrace(
      int frame_limit, StackTrace::StackTraceOptions options);
  Handle<Object> CaptureSimpleStackTrace(Handle<JSReceiver> error_object,
                                         FrameSkipMode mode,
                                         Handle<Object> caller);
  MaybeHandle<JSReceiver> CaptureAndSetDetailedStackTrace(
      Handle<JSReceiver> error_object);
  MaybeHandle<JSReceiver> CaptureAndSetSimpleStackTrace(
      Handle<JSReceiver> error_object, FrameSkipMode mode,
      Handle<Object> caller);
  Handle<FixedArray> GetDetailedStackTrace(Handle<JSObject> error_object);

  Address GetAbstractPC(int* line, int* column);

  // Returns if the given context may access the given global object. If
  // the result is false, the pending exception is guaranteed to be
  // set.
  bool MayAccess(Handle<Context> accessing_context, Handle<JSObject> receiver);

  void SetFailedAccessCheckCallback(v8::FailedAccessCheckCallback callback);
  void ReportFailedAccessCheck(Handle<JSObject> receiver);

  // Exception throwing support. The caller should use the result
  // of Throw() as its return value.
  Object* Throw(Object* exception, MessageLocation* location = nullptr);
  Object* ThrowIllegalOperation();

  template <typename T>
  V8_WARN_UNUSED_RESULT MaybeHandle<T> Throw(
      Handle<Object> exception, MessageLocation* location = nullptr) {
    Throw(*exception, location);
    return MaybeHandle<T>();
  }

  void set_console_delegate(debug::ConsoleDelegate* delegate) {
    console_delegate_ = delegate;
  }
  debug::ConsoleDelegate* console_delegate() { return console_delegate_; }

  void set_async_event_delegate(debug::AsyncEventDelegate* delegate) {
    async_event_delegate_ = delegate;
    PromiseHookStateUpdated();
  }
  void OnAsyncFunctionStateChanged(Handle<JSPromise> promise,
                                   debug::DebugAsyncActionType);

  // Re-throw an exception.  This involves no error reporting since error
  // reporting was handled when the exception was thrown originally.
  Object* ReThrow(Object* exception);

  // Find the correct handler for the current pending exception. This also
  // clears and returns the current pending exception.
  Object* UnwindAndFindHandler();

  // Tries to predict whether an exception will be caught. Note that this can
  // only produce an estimate, because it is undecidable whether a finally
  // clause will consume or re-throw an exception.
  enum CatchType {
    NOT_CAUGHT,
    CAUGHT_BY_JAVASCRIPT,
    CAUGHT_BY_EXTERNAL,
    CAUGHT_BY_DESUGARING,
    CAUGHT_BY_PROMISE,
    CAUGHT_BY_ASYNC_AWAIT
  };
  CatchType PredictExceptionCatcher();

  V8_EXPORT_PRIVATE void ScheduleThrow(Object* exception);
  // Re-set pending message, script and positions reported to the TryCatch
  // back to the TLS for re-use when rethrowing.
  void RestorePendingMessageFromTryCatch(v8::TryCatch* handler);
  // Un-schedule an exception that was caught by a TryCatch handler.
  void CancelScheduledExceptionFromTryCatch(v8::TryCatch* handler);
  void ReportPendingMessages();
  void ReportPendingMessagesFromJavaScript();

  // Implements code shared between the two above methods
  void ReportPendingMessagesImpl(bool report_externally);

  // Return pending location if any or unfilled structure.
  MessageLocation GetMessageLocation();

  // Promote a scheduled exception to pending. Asserts has_scheduled_exception.
  Object* PromoteScheduledException();

  // Attempts to compute the current source location, storing the
  // result in the target out parameter. The source location is attached to a
  // Message object as the location which should be shown to the user. It's
  // typically the top-most meaningful location on the stack.
  bool ComputeLocation(MessageLocation* target);
  bool ComputeLocationFromException(MessageLocation* target,
                                    Handle<Object> exception);
  bool ComputeLocationFromStackTrace(MessageLocation* target,
                                     Handle<Object> exception);

  Handle<JSMessageObject> CreateMessage(Handle<Object> exception,
                                        MessageLocation* location);

  // Out of resource exception helpers.
  Object* StackOverflow();
  Object* TerminateExecution();
  void CancelTerminateExecution();

  void RequestInterrupt(InterruptCallback callback, void* data);
  void InvokeApiInterruptCallbacks();

  // Administration
  void Iterate(RootVisitor* v);
  void Iterate(RootVisitor* v, ThreadLocalTop* t);
  char* Iterate(RootVisitor* v, char* t);
  void IterateThread(ThreadVisitor* v, char* t);

  // Returns the current native context.
  inline Handle<NativeContext> native_context();
  inline NativeContext* raw_native_context();

  Handle<Context> GetIncumbentContext();

  void RegisterTryCatchHandler(v8::TryCatch* that);
  void UnregisterTryCatchHandler(v8::TryCatch* that);

  char* ArchiveThread(char* to);
  char* RestoreThread(char* from);

  static const int kUC16AlphabetSize = 256;  // See StringSearchBase.
  static const int kBMMaxShift = 250;        // See StringSearchBase.

  // Accessors.
#define GLOBAL_ACCESSOR(type, name, initialvalue)                       \
  inline type name() const {                                            \
    DCHECK(OFFSET_OF(Isolate, name##_) == name##_debug_offset_);        \
    return name##_;                                                     \
  }                                                                     \
  inline void set_##name(type value) {                                  \
    DCHECK(OFFSET_OF(Isolate, name##_) == name##_debug_offset_);        \
    name##_ = value;                                                    \
  }
  ISOLATE_INIT_LIST(GLOBAL_ACCESSOR)
#undef GLOBAL_ACCESSOR

#define GLOBAL_ARRAY_ACCESSOR(type, name, length)                       \
  inline type* name() {                                                 \
    DCHECK(OFFSET_OF(Isolate, name##_) == name##_debug_offset_);        \
    return &(name##_)[0];                                               \
  }
  ISOLATE_INIT_ARRAY_LIST(GLOBAL_ARRAY_ACCESSOR)
#undef GLOBAL_ARRAY_ACCESSOR

#define NATIVE_CONTEXT_FIELD_ACCESSOR(index, type, name) \
  inline Handle<type> name();                            \
  inline bool is_##name(type* value);
  NATIVE_CONTEXT_FIELDS(NATIVE_CONTEXT_FIELD_ACCESSOR)
#undef NATIVE_CONTEXT_FIELD_ACCESSOR

  Bootstrapper* bootstrapper() { return bootstrapper_; }
  // Use for updating counters on a foreground thread.
  Counters* counters() { return async_counters().get(); }
  // Use for updating counters on a background thread.
  const std::shared_ptr<Counters>& async_counters() {
    // Make sure InitializeCounters() has been called.
    DCHECK_NOT_NULL(async_counters_.get());
    return async_counters_;
  }
  RuntimeProfiler* runtime_profiler() { return runtime_profiler_; }
  CompilationCache* compilation_cache() { return compilation_cache_; }
  Logger* logger() {
    // Call InitializeLoggingAndCounters() if logging is needed before
    // the isolate is fully initialized.
    DCHECK_NOT_NULL(logger_);
    return logger_;
  }
  StackGuard* stack_guard() { return &stack_guard_; }
  Heap* heap() { return &heap_; }

  // kRootRegister may be used to address any location that falls into this
  // region. Fields outside this region are not guaranteed to live at a static
  // offset from kRootRegister.
  inline base::AddressRegion root_register_addressable_region();

  StubCache* load_stub_cache() { return load_stub_cache_; }
  StubCache* store_stub_cache() { return store_stub_cache_; }
  DeoptimizerData* deoptimizer_data() { return deoptimizer_data_; }
  bool deoptimizer_lazy_throw() const { return deoptimizer_lazy_throw_; }
  void set_deoptimizer_lazy_throw(bool value) {
    deoptimizer_lazy_throw_ = value;
  }
  ThreadLocalTop* thread_local_top() { return &thread_local_top_; }
  MaterializedObjectStore* materialized_object_store() {
    return materialized_object_store_;
  }

  ContextSlotCache* context_slot_cache() {
    return context_slot_cache_;
  }

  DescriptorLookupCache* descriptor_lookup_cache() {
    return descriptor_lookup_cache_;
  }

  HandleScopeData* handle_scope_data() { return &handle_scope_data_; }

  HandleScopeImplementer* handle_scope_implementer() {
    DCHECK(handle_scope_implementer_);
    return handle_scope_implementer_;
  }

  UnicodeCache* unicode_cache() {
    return unicode_cache_;
  }

  InnerPointerToCodeCache* inner_pointer_to_code_cache() {
    return inner_pointer_to_code_cache_;
  }

  GlobalHandles* global_handles() { return global_handles_; }

  EternalHandles* eternal_handles() { return eternal_handles_; }

  ThreadManager* thread_manager() { return thread_manager_; }

  unibrow::Mapping<unibrow::Ecma262UnCanonicalize>* jsregexp_uncanonicalize() {
    return &jsregexp_uncanonicalize_;
  }

  unibrow::Mapping<unibrow::CanonicalizationRange>* jsregexp_canonrange() {
    return &jsregexp_canonrange_;
  }

  RuntimeState* runtime_state() { return &runtime_state_; }

  Builtins* builtins() { return &builtins_; }

  unibrow::Mapping<unibrow::Ecma262Canonicalize>*
      regexp_macro_assembler_canonicalize() {
    return &regexp_macro_assembler_canonicalize_;
  }

  RegExpStack* regexp_stack() { return regexp_stack_; }

  size_t total_regexp_code_generated() { return total_regexp_code_generated_; }
  void IncreaseTotalRegexpCodeGenerated(int size) {
    total_regexp_code_generated_ += size;
  }

  std::vector<int>* regexp_indices() { return &regexp_indices_; }

  unibrow::Mapping<unibrow::Ecma262Canonicalize>*
      interp_canonicalize_mapping() {
    return &regexp_macro_assembler_canonicalize_;
  }

  Debug* debug() { return debug_; }

  bool* is_profiling_address() { return &is_profiling_; }
  CodeEventDispatcher* code_event_dispatcher() const {
    return code_event_dispatcher_.get();
  }
  HeapProfiler* heap_profiler() const { return heap_profiler_; }

#ifdef DEBUG
  static size_t non_disposed_isolates() { return non_disposed_isolates_; }
#endif

  v8::internal::Factory* factory() {
    // Upcast to the privately inherited base-class using c-style casts to avoid
    // undefined behavior (as static_cast cannot cast across private bases).
    // NOLINTNEXTLINE (google-readability-casting)
    return (v8::internal::Factory*)this;  // NOLINT(readability/casting)
  }

  static const int kJSRegexpStaticOffsetsVectorSize = 128;

  THREAD_LOCAL_TOP_ACCESSOR(ExternalCallbackScope*, external_callback_scope)

  THREAD_LOCAL_TOP_ACCESSOR(StateTag, current_vm_state)

  void SetData(uint32_t slot, void* data) {
    DCHECK_LT(slot, Internals::kNumIsolateDataSlots);
    embedder_data_[slot] = data;
  }
  void* GetData(uint32_t slot) {
    DCHECK_LT(slot, Internals::kNumIsolateDataSlots);
    return embedder_data_[slot];
  }

  bool serializer_enabled() const { return serializer_enabled_; }

  void enable_serializer() { serializer_enabled_ = true; }

  bool snapshot_available() const {
    return snapshot_blob_ != nullptr && snapshot_blob_->raw_size != 0;
  }

  bool IsDead() { return has_fatal_error_; }
  void SignalFatalError() { has_fatal_error_ = true; }

  bool use_optimizer();

  bool initialized_from_snapshot() { return initialized_from_snapshot_; }

  bool NeedsSourcePositionsForProfiling() const;

  bool NeedsDetailedOptimizedCodeLineInfo() const;

  bool is_best_effort_code_coverage() const {
    return code_coverage_mode() == debug::Coverage::kBestEffort;
  }

  bool is_precise_count_code_coverage() const {
    return code_coverage_mode() == debug::Coverage::kPreciseCount;
  }

  bool is_precise_binary_code_coverage() const {
    return code_coverage_mode() == debug::Coverage::kPreciseBinary;
  }

  bool is_block_count_code_coverage() const {
    return code_coverage_mode() == debug::Coverage::kBlockCount;
  }

  bool is_block_binary_code_coverage() const {
    return code_coverage_mode() == debug::Coverage::kBlockBinary;
  }

  bool is_block_code_coverage() const {
    return is_block_count_code_coverage() || is_block_binary_code_coverage();
  }

  bool is_collecting_type_profile() const {
    return type_profile_mode() == debug::TypeProfile::kCollect;
  }

  // Collect feedback vectors with data for code coverage or type profile.
  // Reset the list, when both code coverage and type profile are not
  // needed anymore. This keeps many feedback vectors alive, but code
  // coverage or type profile are used for debugging only and increase in
  // memory usage is expected.
  void SetFeedbackVectorsForProfilingTools(Object* value);

  void MaybeInitializeVectorListFromHeap();

  double time_millis_since_init() {
    return heap_.MonotonicallyIncreasingTimeInMs() - time_millis_at_init_;
  }

  DateCache* date_cache() {
    return date_cache_;
  }

  void set_date_cache(DateCache* date_cache) {
    if (date_cache != date_cache_) {
      delete date_cache_;
    }
    date_cache_ = date_cache;
  }

#ifdef V8_INTL_SUPPORT
#if USE_CHROMIUM_ICU == 0 && U_ICU_VERSION_MAJOR_NUM < 63
  icu::RegexMatcher* language_singleton_regexp_matcher() {
    return language_singleton_regexp_matcher_;
  }

  icu::RegexMatcher* language_tag_regexp_matcher() {
    return language_tag_regexp_matcher_;
  }

  icu::RegexMatcher* language_variant_regexp_matcher() {
    return language_variant_regexp_matcher_;
  }
#endif  // USE_CHROMIUM_ICU == 0 && U_ICU_VERSION_MAJOR_NUM < 63

  const std::string& default_locale() { return default_locale_; }

  void set_default_locale(const std::string& locale) {
    DCHECK_EQ(default_locale_.length(), 0);
    default_locale_ = locale;
  }

#if USE_CHROMIUM_ICU == 0 && U_ICU_VERSION_MAJOR_NUM < 63
  void set_language_tag_regexp_matchers(
      icu::RegexMatcher* language_singleton_regexp_matcher,
      icu::RegexMatcher* language_tag_regexp_matcher,
      icu::RegexMatcher* language_variant_regexp_matcher) {
    DCHECK_NULL(language_singleton_regexp_matcher_);
    DCHECK_NULL(language_tag_regexp_matcher_);
    DCHECK_NULL(language_variant_regexp_matcher_);
    language_singleton_regexp_matcher_ = language_singleton_regexp_matcher;
    language_tag_regexp_matcher_ = language_tag_regexp_matcher;
    language_variant_regexp_matcher_ = language_variant_regexp_matcher;
  }
#endif  // USE_CHROMIUM_ICU == 0 && U_ICU_VERSION_MAJOR_NUM < 63
#endif  // V8_INTL_SUPPORT

  static const int kProtectorValid = 1;
  static const int kProtectorInvalid = 0;

  inline bool IsArrayConstructorIntact();

  // The version with an explicit context parameter can be used when
  // Isolate::context is not set up, e.g. when calling directly into C++ from
  // CSA.
  bool IsNoElementsProtectorIntact(Context* context);
  bool IsNoElementsProtectorIntact();

  inline bool IsArraySpeciesLookupChainIntact();
  inline bool IsTypedArraySpeciesLookupChainIntact();
  inline bool IsPromiseSpeciesLookupChainIntact();
  bool IsIsConcatSpreadableLookupChainIntact();
  bool IsIsConcatSpreadableLookupChainIntact(JSReceiver* receiver);
  inline bool IsStringLengthOverflowIntact();
  inline bool IsArrayIteratorLookupChainIntact();

  // The StringIteratorProtector protects the original string iterating behavior
  // for primitive strings. As long as the StringIteratorProtector is valid,
  // iterating over a primitive string is guaranteed to be unobservable from
  // user code and can thus be cut short. More specifically, the protector gets
  // invalidated as soon as either String.prototype[Symbol.iterator] or
  // String.prototype[Symbol.iterator]().next is modified. This guarantee does
  // not apply to string objects (as opposed to primitives), since they could
  // define their own Symbol.iterator.
  // String.prototype itself does not need to be protected, since it is
  // non-configurable and non-writable.
  inline bool IsStringIteratorLookupChainIntact();

  // Make sure we do check for neutered array buffers.
  inline bool IsArrayBufferNeuteringIntact();

  // Disable promise optimizations if promise (debug) hooks have ever been
  // active.
  bool IsPromiseHookProtectorIntact();

  // Make sure a lookup of "resolve" on the %Promise% intrinsic object
  // yeidls the initial Promise.resolve method.
  bool IsPromiseResolveLookupChainIntact();

  // Make sure a lookup of "then" on any JSPromise whose [[Prototype]] is the
  // initial %PromisePrototype% yields the initial method. In addition this
  // protector also guards the negative lookup of "then" on the intrinsic
  // %ObjectPrototype%, meaning that such lookups are guaranteed to yield
  // undefined without triggering any side-effects.
  bool IsPromiseThenLookupChainIntact();
  bool IsPromiseThenLookupChainIntact(Handle<JSReceiver> receiver);

  // On intent to set an element in object, make sure that appropriate
  // notifications occur if the set is on the elements of the array or
  // object prototype. Also ensure that changes to prototype chain between
  // Array and Object fire notifications.
  void UpdateNoElementsProtectorOnSetElement(Handle<JSObject> object);
  void UpdateNoElementsProtectorOnSetLength(Handle<JSObject> object) {
    UpdateNoElementsProtectorOnSetElement(object);
  }
  void UpdateNoElementsProtectorOnSetPrototype(Handle<JSObject> object) {
    UpdateNoElementsProtectorOnSetElement(object);
  }
  void UpdateNoElementsProtectorOnNormalizeElements(Handle<JSObject> object) {
    UpdateNoElementsProtectorOnSetElement(object);
  }
  void InvalidateArrayConstructorProtector();
  void InvalidateArraySpeciesProtector();
  void InvalidateTypedArraySpeciesProtector();
  void InvalidatePromiseSpeciesProtector();
  void InvalidateIsConcatSpreadableProtector();
  void InvalidateStringLengthOverflowProtector();
  void InvalidateArrayIteratorProtector();
  void InvalidateStringIteratorProtector();
  void InvalidateArrayBufferNeuteringProtector();
  V8_EXPORT_PRIVATE void InvalidatePromiseHookProtector();
  void InvalidatePromiseResolveProtector();
  void InvalidatePromiseThenProtector();

  // Returns true if array is the initial array prototype in any native context.
  bool IsAnyInitialArrayPrototype(Handle<JSArray> array);

  void IterateDeferredHandles(RootVisitor* visitor);
  void LinkDeferredHandles(DeferredHandles* deferred_handles);
  void UnlinkDeferredHandles(DeferredHandles* deferred_handles);

#ifdef DEBUG
  bool IsDeferredHandle(Object** location);
#endif  // DEBUG

  bool concurrent_recompilation_enabled() {
    // Thread is only available with flag enabled.
    DCHECK(optimizing_compile_dispatcher_ == nullptr ||
           FLAG_concurrent_recompilation);
    return optimizing_compile_dispatcher_ != nullptr;
  }

  OptimizingCompileDispatcher* optimizing_compile_dispatcher() {
    return optimizing_compile_dispatcher_;
  }
  // Flushes all pending concurrent optimzation jobs from the optimizing
  // compile dispatcher's queue.
  void AbortConcurrentOptimization(BlockingBehavior blocking_behavior);

  int id() const { return static_cast<int>(id_); }

  CompilationStatistics* GetTurboStatistics();
  CodeTracer* GetCodeTracer();

  void DumpAndResetStats();

  FunctionEntryHook function_entry_hook() { return function_entry_hook_; }
  void set_function_entry_hook(FunctionEntryHook function_entry_hook) {
    function_entry_hook_ = function_entry_hook;
  }

  void* stress_deopt_count_address() { return &stress_deopt_count_; }

  void set_force_slow_path(bool v) { force_slow_path_ = v; }
  bool force_slow_path() const { return force_slow_path_; }
  bool* force_slow_path_address() { return &force_slow_path_; }

  DebugInfo::ExecutionMode* debug_execution_mode_address() {
    return &debug_execution_mode_;
  }

  V8_EXPORT_PRIVATE base::RandomNumberGenerator* random_number_generator();

  V8_EXPORT_PRIVATE base::RandomNumberGenerator* fuzzer_rng();

  // Generates a random number that is non-zero when masked
  // with the provided mask.
  int GenerateIdentityHash(uint32_t mask);

  // Given an address occupied by a live code object, return that object.
  Code* FindCodeObject(Address a);

  int NextOptimizationId() {
    int id = next_optimization_id_++;
    if (!Smi::IsValid(next_optimization_id_)) {
      next_optimization_id_ = 0;
    }
    return id;
  }

  void AddNearHeapLimitCallback(v8::NearHeapLimitCallback, void* data);
  void RemoveNearHeapLimitCallback(v8::NearHeapLimitCallback callback,
                                   size_t heap_limit);
  void AddCallCompletedCallback(CallCompletedCallback callback);
  void RemoveCallCompletedCallback(CallCompletedCallback callback);
  void FireCallCompletedCallback();

  void AddBeforeCallEnteredCallback(BeforeCallEnteredCallback callback);
  void RemoveBeforeCallEnteredCallback(BeforeCallEnteredCallback callback);
  inline void FireBeforeCallEnteredCallback();

  void AddMicrotasksCompletedCallback(MicrotasksCompletedCallback callback);
  void RemoveMicrotasksCompletedCallback(MicrotasksCompletedCallback callback);
  inline void FireMicrotasksCompletedCallback();

  void SetPromiseRejectCallback(PromiseRejectCallback callback);
  void ReportPromiseReject(Handle<JSPromise> promise, Handle<Object> value,
                           v8::PromiseRejectEvent event);

  void EnqueueMicrotask(Handle<Microtask> microtask);
  void RunMicrotasks();
  bool IsRunningMicrotasks() const { return is_running_microtasks_; }

  Handle<Symbol> SymbolFor(RootIndex dictionary_index, Handle<String> name,
                           bool private_symbol);

  void SetUseCounterCallback(v8::Isolate::UseCounterCallback callback);
  void CountUsage(v8::Isolate::UseCounterFeature feature);

  static std::string GetTurboCfgFileName(Isolate* isolate);

#if V8_SFI_HAS_UNIQUE_ID
  int GetNextUniqueSharedFunctionInfoId() { return next_unique_sfi_id_++; }
#endif

  Address promise_hook_address() {
    return reinterpret_cast<Address>(&promise_hook_);
  }

  Address async_event_delegate_address() {
    return reinterpret_cast<Address>(&async_event_delegate_);
  }

  Address promise_hook_or_async_event_delegate_address() {
    return reinterpret_cast<Address>(&promise_hook_or_async_event_delegate_);
  }

  Address handle_scope_implementer_address() {
    return reinterpret_cast<Address>(&handle_scope_implementer_);
  }

  void SetAtomicsWaitCallback(v8::Isolate::AtomicsWaitCallback callback,
                              void* data);
  void RunAtomicsWaitCallback(v8::Isolate::AtomicsWaitEvent event,
                              Handle<JSArrayBuffer> array_buffer,
                              size_t offset_in_bytes, int32_t value,
                              double timeout_in_ms,
                              AtomicsWaitWakeHandle* stop_handle);

  void SetPromiseHook(PromiseHook hook);
  void RunPromiseHook(PromiseHookType type, Handle<JSPromise> promise,
                      Handle<Object> parent);

  void AddDetachedContext(Handle<Context> context);
  void CheckDetachedContextsAfterGC();

  std::vector<Object*>* partial_snapshot_cache() {
    return &partial_snapshot_cache_;
  }

  // Off-heap builtins cannot embed constants within the code object itself,
  // and thus need to load them from the root list.
  bool ShouldLoadConstantsFromRootList() const {
    if (FLAG_embedded_builtins) {
      return (serializer_enabled() &&
              builtins_constants_table_builder() != nullptr);
    } else {
      return false;
    }
  }

  // Called only prior to serialization.
  // This function copies off-heap-safe builtins off the heap, creates off-heap
  // trampolines, and sets up this isolate's embedded blob.
  void PrepareEmbeddedBlobForSerialization();

  BuiltinsConstantsTableBuilder* builtins_constants_table_builder() const {
    return builtins_constants_table_builder_;
  }

  static const uint8_t* CurrentEmbeddedBlob();
  static uint32_t CurrentEmbeddedBlobSize();

  // These always return the same result as static methods above, but don't
  // access the global atomic variable (and thus *might be* slightly faster).
  const uint8_t* embedded_blob() const;
  uint32_t embedded_blob_size() const;

  void set_array_buffer_allocator(v8::ArrayBuffer::Allocator* allocator) {
    array_buffer_allocator_ = allocator;
  }
  v8::ArrayBuffer::Allocator* array_buffer_allocator() const {
    return array_buffer_allocator_;
  }

  FutexWaitListNode* futex_wait_list_node() { return &futex_wait_list_node_; }

  CancelableTaskManager* cancelable_task_manager() {
    return cancelable_task_manager_;
  }

  const AstStringConstants* ast_string_constants() const {
    return ast_string_constants_;
  }

  interpreter::Interpreter* interpreter() const { return interpreter_; }

  compiler::PerIsolateCompilerCache* compiler_cache() const {
    return compiler_cache_;
  }
  void set_compiler_utils(compiler::PerIsolateCompilerCache* cache,
                          Zone* zone) {
    compiler_cache_ = cache;
    compiler_zone_ = zone;
  }

  AccountingAllocator* allocator() { return allocator_; }

  CompilerDispatcher* compiler_dispatcher() const {
    return compiler_dispatcher_;
  }

  bool IsInAnyContext(Object* object, uint32_t index);

  void SetHostImportModuleDynamicallyCallback(
      HostImportModuleDynamicallyCallback callback);
  MaybeHandle<JSPromise> RunHostImportModuleDynamicallyCallback(
      Handle<Script> referrer, Handle<Object> specifier);

  void SetHostInitializeImportMetaObjectCallback(
      HostInitializeImportMetaObjectCallback callback);
  Handle<JSObject> RunHostInitializeImportMetaObjectCallback(
      Handle<Module> module);

  void SetPrepareStackTraceCallback(PrepareStackTraceCallback callback);
  MaybeHandle<Object> RunPrepareStackTraceCallback(Handle<Context>,
                                                   Handle<JSObject> Error);
  bool HasPrepareStackTraceCallback() const;

  void SetRAILMode(RAILMode rail_mode);

  RAILMode rail_mode() { return rail_mode_.Value(); }

  double LoadStartTimeMs();

  void IsolateInForegroundNotification();

  void IsolateInBackgroundNotification();

  bool IsIsolateInBackground() { return is_isolate_in_background_; }

  void EnableMemorySavingsMode() { memory_savings_mode_active_ = true; }

  void DisableMemorySavingsMode() { memory_savings_mode_active_ = false; }

  bool IsMemorySavingsModeActive() { return memory_savings_mode_active_; }

  PRINTF_FORMAT(2, 3) void PrintWithTimestamp(const char* format, ...);

  void set_allow_atomics_wait(bool set) { allow_atomics_wait_ = set; }
  bool allow_atomics_wait() { return allow_atomics_wait_; }

  // Register a finalizer to be called at isolate teardown.
  void RegisterManagedPtrDestructor(ManagedPtrDestructor* finalizer);

  // Removes a previously-registered shared object finalizer.
  void UnregisterManagedPtrDestructor(ManagedPtrDestructor* finalizer);

  size_t elements_deletion_counter() { return elements_deletion_counter_; }
  void set_elements_deletion_counter(size_t value) {
    elements_deletion_counter_ = value;
  }

  wasm::WasmEngine* wasm_engine() const { return wasm_engine_.get(); }
  void SetWasmEngine(std::shared_ptr<wasm::WasmEngine> engine);

  const v8::Context::BackupIncumbentScope* top_backup_incumbent_scope() const {
    return top_backup_incumbent_scope_;
  }
  void set_top_backup_incumbent_scope(
      const v8::Context::BackupIncumbentScope* top_backup_incumbent_scope) {
    top_backup_incumbent_scope_ = top_backup_incumbent_scope;
  }

  void SetIdle(bool is_idle);

 protected:
  Isolate();
  bool IsArrayOrObjectOrStringPrototype(Object* object);

 private:
  friend struct GlobalState;
  friend struct InitializeGlobalState;

  // These fields are accessed through the API, offsets must be kept in sync
  // with v8::internal::Internals (in include/v8.h) constants. This is also
  // verified in Isolate::Init() using runtime checks.
  void* embedder_data_[Internals::kNumIsolateDataSlots];
  Heap heap_;

  class ThreadDataTable {
   public:
    ThreadDataTable() = default;

    PerIsolateThreadData* Lookup(ThreadId thread_id);
    void Insert(PerIsolateThreadData* data);
    void Remove(PerIsolateThreadData* data);
    void RemoveAllThreads();

   private:
    struct Hasher {
      std::size_t operator()(const ThreadId& t) const {
        return std::hash<int>()(t.ToInteger());
      }
    };

    std::unordered_map<ThreadId, PerIsolateThreadData*, Hasher> table_;
  };

  // These items form a stack synchronously with threads Enter'ing and Exit'ing
  // the Isolate. The top of the stack points to a thread which is currently
  // running the Isolate. When the stack is empty, the Isolate is considered
  // not entered by any thread and can be Disposed.
  // If the same thread enters the Isolate more than once, the entry_count_
  // is incremented rather then a new item pushed to the stack.
  class EntryStackItem {
   public:
    EntryStackItem(PerIsolateThreadData* previous_thread_data,
                   Isolate* previous_isolate,
                   EntryStackItem* previous_item)
        : entry_count(1),
          previous_thread_data(previous_thread_data),
          previous_isolate(previous_isolate),
          previous_item(previous_item) { }

    int entry_count;
    PerIsolateThreadData* previous_thread_data;
    Isolate* previous_isolate;
    EntryStackItem* previous_item;

   private:
    DISALLOW_COPY_AND_ASSIGN(EntryStackItem);
  };

  static base::Thread::LocalStorageKey per_isolate_thread_data_key_;
  static base::Thread::LocalStorageKey isolate_key_;
  static base::Thread::LocalStorageKey thread_id_key_;

  // A global counter for all generated Isolates, might overflow.
  static base::Atomic32 isolate_counter_;

#if DEBUG
  static base::Atomic32 isolate_key_created_;
#endif

  void Deinit();

  static void SetIsolateThreadLocals(Isolate* isolate,
                                     PerIsolateThreadData* data);

  // Find the PerThread for this particular (isolate, thread) combination.
  // If one does not yet exist, allocate a new one.
  PerIsolateThreadData* FindOrAllocatePerThreadDataForThisThread();

  // Initializes the current thread to run this Isolate.
  // Not thread-safe. Multiple threads should not Enter/Exit the same isolate
  // at the same time, this should be prevented using external locking.
  void Enter();

  // Exits the current thread. The previosuly entered Isolate is restored
  // for the thread.
  // Not thread-safe. Multiple threads should not Enter/Exit the same isolate
  // at the same time, this should be prevented using external locking.
  void Exit();

  void InitializeThreadLocal();

  void MarkCompactPrologue(bool is_compacting,
                           ThreadLocalTop* archived_thread_data);
  void MarkCompactEpilogue(bool is_compacting,
                           ThreadLocalTop* archived_thread_data);

  void FillCache();

  // Propagate pending exception message to the v8::TryCatch.
  // If there is no external try-catch or message was successfully propagated,
  // then return true.
  bool PropagatePendingExceptionToExternalTryCatch();

  void SetTerminationOnExternalTryCatch();

  void PromiseHookStateUpdated();
  void RunPromiseHookForAsyncEventDelegate(PromiseHookType type,
                                           Handle<JSPromise> promise);

  const char* RAILModeName(RAILMode rail_mode) const {
    switch (rail_mode) {
      case PERFORMANCE_RESPONSE:
        return "RESPONSE";
      case PERFORMANCE_ANIMATION:
        return "ANIMATION";
      case PERFORMANCE_IDLE:
        return "IDLE";
      case PERFORMANCE_LOAD:
        return "LOAD";
    }
    return "";
  }

  base::Atomic32 id_;
  EntryStackItem* entry_stack_;
  int stack_trace_nesting_level_;
  StringStream* incomplete_message_;
  Address isolate_addresses_[kIsolateAddressCount + 1];  // NOLINT
  Bootstrapper* bootstrapper_;
  RuntimeProfiler* runtime_profiler_;
  CompilationCache* compilation_cache_;
  std::shared_ptr<Counters> async_counters_;
  base::RecursiveMutex break_access_;
  Logger* logger_;
  StackGuard stack_guard_;
  StubCache* load_stub_cache_;
  StubCache* store_stub_cache_;
  DeoptimizerData* deoptimizer_data_;
  bool deoptimizer_lazy_throw_;
  MaterializedObjectStore* materialized_object_store_;
  ThreadLocalTop thread_local_top_;
  bool capture_stack_trace_for_uncaught_exceptions_;
  int stack_trace_for_uncaught_exceptions_frame_limit_;
  StackTrace::StackTraceOptions stack_trace_for_uncaught_exceptions_options_;
  ContextSlotCache* context_slot_cache_;
  DescriptorLookupCache* descriptor_lookup_cache_;
  HandleScopeData handle_scope_data_;
  HandleScopeImplementer* handle_scope_implementer_;
  UnicodeCache* unicode_cache_;
  AccountingAllocator* allocator_;
  InnerPointerToCodeCache* inner_pointer_to_code_cache_;
  GlobalHandles* global_handles_;
  EternalHandles* eternal_handles_;
  ThreadManager* thread_manager_;
  RuntimeState runtime_state_;
  Builtins builtins_;
  SetupIsolateDelegate* setup_delegate_;
  unibrow::Mapping<unibrow::Ecma262UnCanonicalize> jsregexp_uncanonicalize_;
  unibrow::Mapping<unibrow::CanonicalizationRange> jsregexp_canonrange_;
  unibrow::Mapping<unibrow::Ecma262Canonicalize>
      regexp_macro_assembler_canonicalize_;
  RegExpStack* regexp_stack_;
  std::vector<int> regexp_indices_;
  DateCache* date_cache_;
  base::RandomNumberGenerator* random_number_generator_;
  base::RandomNumberGenerator* fuzzer_rng_;
  base::AtomicValue<RAILMode> rail_mode_;
  v8::Isolate::AtomicsWaitCallback atomics_wait_callback_;
  void* atomics_wait_callback_data_;
  PromiseHook promise_hook_;
  HostImportModuleDynamicallyCallback host_import_module_dynamically_callback_;
  HostInitializeImportMetaObjectCallback
      host_initialize_import_meta_object_callback_;
  base::Mutex rail_mutex_;
  double load_start_time_ms_;

#ifdef V8_INTL_SUPPORT
#if USE_CHROMIUM_ICU == 0 && U_ICU_VERSION_MAJOR_NUM < 63
  icu::RegexMatcher* language_singleton_regexp_matcher_;
  icu::RegexMatcher* language_tag_regexp_matcher_;
  icu::RegexMatcher* language_variant_regexp_matcher_;
#endif  // USE_CHROMIUM_ICU == 0 && U_ICU_VERSION_MAJOR_NUM < 63
  std::string default_locale_;
#endif  // V8_INTL_SUPPORT

  // Whether the isolate has been created for snapshotting.
  bool serializer_enabled_;

  // True if fatal error has been signaled for this isolate.
  bool has_fatal_error_;

  // True if this isolate was initialized from a snapshot.
  bool initialized_from_snapshot_;

  // True if ES2015 tail call elimination feature is enabled.
  bool is_tail_call_elimination_enabled_;

  // True if the isolate is in background. This flag is used
  // to prioritize between memory usage and latency.
  bool is_isolate_in_background_;

  // True if the isolate is in memory savings mode. This flag is used to
  // favor memory over runtime performance.
  bool memory_savings_mode_active_;

  // Time stamp at initialization.
  double time_millis_at_init_;

#ifdef DEBUG
  static std::atomic<size_t> non_disposed_isolates_;

  JSObject::SpillInformation js_spill_information_;
#endif

  Debug* debug_;
  HeapProfiler* heap_profiler_;
  std::unique_ptr<CodeEventDispatcher> code_event_dispatcher_;
  FunctionEntryHook function_entry_hook_;

  const AstStringConstants* ast_string_constants_;

  interpreter::Interpreter* interpreter_;

  compiler::PerIsolateCompilerCache* compiler_cache_ = nullptr;
  Zone* compiler_zone_ = nullptr;

  CompilerDispatcher* compiler_dispatcher_;

  typedef std::pair<InterruptCallback, void*> InterruptEntry;
  std::queue<InterruptEntry> api_interrupts_queue_;

#define GLOBAL_BACKING_STORE(type, name, initialvalue)                         \
  type name##_;
  ISOLATE_INIT_LIST(GLOBAL_BACKING_STORE)
#undef GLOBAL_BACKING_STORE

#define GLOBAL_ARRAY_BACKING_STORE(type, name, length)                         \
  type name##_[length];
  ISOLATE_INIT_ARRAY_LIST(GLOBAL_ARRAY_BACKING_STORE)
#undef GLOBAL_ARRAY_BACKING_STORE

#ifdef DEBUG
  // This class is huge and has a number of fields controlled by
  // preprocessor defines. Make sure the offsets of these fields agree
  // between compilation units.
#define ISOLATE_FIELD_OFFSET(type, name, ignored)                              \
  static const intptr_t name##_debug_offset_;
  ISOLATE_INIT_LIST(ISOLATE_FIELD_OFFSET)
  ISOLATE_INIT_ARRAY_LIST(ISOLATE_FIELD_OFFSET)
#undef ISOLATE_FIELD_OFFSET
#endif

  DeferredHandles* deferred_handles_head_;
  OptimizingCompileDispatcher* optimizing_compile_dispatcher_;

  // Counts deopt points if deopt_every_n_times is enabled.
  unsigned int stress_deopt_count_;

  bool force_slow_path_;

  int next_optimization_id_;

#if V8_SFI_HAS_UNIQUE_ID
  int next_unique_sfi_id_;
#endif

  // Vector of callbacks before a Call starts execution.
  std::vector<BeforeCallEnteredCallback> before_call_entered_callbacks_;

  // Vector of callbacks when a Call completes.
  std::vector<CallCompletedCallback> call_completed_callbacks_;

  // Vector of callbacks after microtasks were run.
  std::vector<MicrotasksCompletedCallback> microtasks_completed_callbacks_;
  bool is_running_microtasks_;

  v8::Isolate::UseCounterCallback use_counter_callback_;

  std::vector<Object*> partial_snapshot_cache_;

  // Used during builtins compilation to build the builtins constants table,
  // which is stored on the root list prior to serialization.
  BuiltinsConstantsTableBuilder* builtins_constants_table_builder_ = nullptr;

  void SetEmbeddedBlob(const uint8_t* blob, uint32_t blob_size);

  const uint8_t* embedded_blob_ = nullptr;
  uint32_t embedded_blob_size_ = 0;

  v8::ArrayBuffer::Allocator* array_buffer_allocator_;

  FutexWaitListNode futex_wait_list_node_;

  CancelableTaskManager* cancelable_task_manager_;

  debug::ConsoleDelegate* console_delegate_ = nullptr;

  debug::AsyncEventDelegate* async_event_delegate_ = nullptr;
  bool promise_hook_or_async_event_delegate_ = false;
  int async_task_count_ = 0;

  v8::Isolate::AbortOnUncaughtExceptionCallback
      abort_on_uncaught_exception_callback_;

  bool allow_atomics_wait_;

  ManagedPtrDestructor* managed_ptr_destructors_head_ = nullptr;

  size_t total_regexp_code_generated_;

  size_t elements_deletion_counter_ = 0;

  std::shared_ptr<wasm::WasmEngine> wasm_engine_;

  std::unique_ptr<TracingCpuProfilerImpl> tracing_cpu_profiler_;

  // The top entry of the v8::Context::BackupIncumbentScope stack.
  const v8::Context::BackupIncumbentScope* top_backup_incumbent_scope_ =
      nullptr;

  PrepareStackTraceCallback prepare_stack_trace_callback_ = nullptr;

  // TODO(kenton@cloudflare.com): This mutex can be removed if
  // thread_data_table_ is always accessed under the isolate lock. I do not
  // know if this is the case, so I'm preserving it for now.
  base::Mutex thread_data_table_mutex_;
  ThreadDataTable thread_data_table_;

  friend class ExecutionAccess;
  friend class HandleScopeImplementer;
  friend class heap::HeapTester;
  friend class OptimizingCompileDispatcher;
  friend class Simulator;
  friend class StackGuard;
  friend class SweeperThread;
  friend class TestIsolate;
  friend class ThreadId;
  friend class ThreadManager;
  friend class v8::Isolate;
  friend class v8::Locker;
  friend class v8::SnapshotCreator;
  friend class v8::Unlocker;

  DISALLOW_COPY_AND_ASSIGN(Isolate);
};


#undef FIELD_ACCESSOR
#undef THREAD_LOCAL_TOP_ACCESSOR


class PromiseOnStack {
 public:
  PromiseOnStack(Handle<JSObject> promise, PromiseOnStack* prev)
      : promise_(promise), prev_(prev) {}
  Handle<JSObject> promise() { return promise_; }
  PromiseOnStack* prev() { return prev_; }

 private:
  Handle<JSObject> promise_;
  PromiseOnStack* prev_;
};


// If the GCC version is 4.1.x or 4.2.x an additional field is added to the
// class as a work around for a bug in the generated code found with these
// versions of GCC. See V8 issue 122 for details.
class V8_EXPORT_PRIVATE SaveContext {
 public:
  explicit SaveContext(Isolate* isolate);
  ~SaveContext();

  Handle<Context> context() { return context_; }
  SaveContext* prev() { return prev_; }

  // Returns true if this save context is below a given JavaScript frame.
  bool IsBelowFrame(StandardFrame* frame);

 private:
  Isolate* const isolate_;
  Handle<Context> context_;
  SaveContext* const prev_;
  Address c_entry_fp_;
};

class AssertNoContextChange {
#ifdef DEBUG
 public:
  explicit AssertNoContextChange(Isolate* isolate);
  ~AssertNoContextChange() {
    DCHECK(isolate_->context() == *context_);
  }

 private:
  Isolate* isolate_;
  Handle<Context> context_;
#else
 public:
  explicit AssertNoContextChange(Isolate* isolate) { }
#endif
};

class ExecutionAccess {
 public:
  explicit ExecutionAccess(Isolate* isolate) : isolate_(isolate) {
    Lock(isolate);
  }
  ~ExecutionAccess() { Unlock(isolate_); }

  static void Lock(Isolate* isolate) { isolate->break_access()->Lock(); }
  static void Unlock(Isolate* isolate) { isolate->break_access()->Unlock(); }

  static bool TryLock(Isolate* isolate) {
    return isolate->break_access()->TryLock();
  }

 private:
  Isolate* isolate_;
};


// Support for checking for stack-overflows.
class StackLimitCheck {
 public:
  explicit StackLimitCheck(Isolate* isolate) : isolate_(isolate) { }

  // Use this to check for stack-overflows in C++ code.
  bool HasOverflowed() const {
    StackGuard* stack_guard = isolate_->stack_guard();
    return GetCurrentStackPosition() < stack_guard->real_climit();
  }

  // Use this to check for interrupt request in C++ code.
  bool InterruptRequested() {
    StackGuard* stack_guard = isolate_->stack_guard();
    return GetCurrentStackPosition() < stack_guard->climit();
  }

  // Use this to check for stack-overflow when entering runtime from JS code.
  bool JsHasOverflowed(uintptr_t gap = 0) const;

 private:
  Isolate* isolate_;
};

#define STACK_CHECK(isolate, result_value) \
  do {                                     \
    StackLimitCheck stack_check(isolate);  \
    if (stack_check.HasOverflowed()) {     \
      isolate->StackOverflow();            \
      return result_value;                 \
    }                                      \
  } while (false)

// Scope intercepts only interrupt which is part of its interrupt_mask and does
// not affect other interrupts.
class InterruptsScope {
 public:
  enum Mode { kPostponeInterrupts, kRunInterrupts, kNoop };

  virtual ~InterruptsScope() {
    if (mode_ != kNoop) stack_guard_->PopInterruptsScope();
  }

  // Find the scope that intercepts this interrupt.
  // It may be outermost PostponeInterruptsScope or innermost
  // SafeForInterruptsScope if any.
  // Return whether the interrupt has been intercepted.
  bool Intercept(StackGuard::InterruptFlag flag);

  InterruptsScope(Isolate* isolate, int intercept_mask, Mode mode)
      : stack_guard_(isolate->stack_guard()),
        intercept_mask_(intercept_mask),
        intercepted_flags_(0),
        mode_(mode) {
    if (mode_ != kNoop) stack_guard_->PushInterruptsScope(this);
  }

 private:
  StackGuard* stack_guard_;
  int intercept_mask_;
  int intercepted_flags_;
  Mode mode_;
  InterruptsScope* prev_;

  friend class StackGuard;
};

// Support for temporarily postponing interrupts. When the outermost
// postpone scope is left the interrupts will be re-enabled and any
// interrupts that occurred while in the scope will be taken into
// account.
class PostponeInterruptsScope : public InterruptsScope {
 public:
  PostponeInterruptsScope(Isolate* isolate,
                          int intercept_mask = StackGuard::ALL_INTERRUPTS)
      : InterruptsScope(isolate, intercept_mask,
                        InterruptsScope::kPostponeInterrupts) {}
  ~PostponeInterruptsScope() override = default;
};

// Support for overriding PostponeInterruptsScope. Interrupt is not ignored if
// innermost scope is SafeForInterruptsScope ignoring any outer
// PostponeInterruptsScopes.
class SafeForInterruptsScope : public InterruptsScope {
 public:
  SafeForInterruptsScope(Isolate* isolate,
                         int intercept_mask = StackGuard::ALL_INTERRUPTS)
      : InterruptsScope(isolate, intercept_mask,
                        InterruptsScope::kRunInterrupts) {}
  ~SafeForInterruptsScope() override = default;
};

class StackTraceFailureMessage {
 public:
  explicit StackTraceFailureMessage(Isolate* isolate, void* ptr1 = nullptr,
                                    void* ptr2 = nullptr, void* ptr3 = nullptr,
                                    void* ptr4 = nullptr);

  V8_NOINLINE void Print() volatile;

  static const uintptr_t kStartMarker = 0xdecade30;
  static const uintptr_t kEndMarker = 0xdecade31;
  static const int kStacktraceBufferSize = 32 * KB;

  uintptr_t start_marker_ = kStartMarker;
  void* isolate_;
  void* ptr1_;
  void* ptr2_;
  void* ptr3_;
  void* ptr4_;
  void* code_objects_[4];
  char js_stack_trace_[kStacktraceBufferSize];
  uintptr_t end_marker_ = kEndMarker;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_ISOLATE_H_
