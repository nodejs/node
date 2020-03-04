// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXECUTION_ISOLATE_H_
#define V8_EXECUTION_ISOLATE_H_

#include <cstddef>
#include <functional>
#include <memory>
#include <queue>
#include <unordered_map>
#include <vector>

#include "include/v8-inspector.h"
#include "include/v8-internal.h"
#include "include/v8.h"
#include "src/base/macros.h"
#include "src/builtins/builtins.h"
#include "src/common/globals.h"
#include "src/debug/interface-types.h"
#include "src/execution/execution.h"
#include "src/execution/futex-emulation.h"
#include "src/execution/isolate-data.h"
#include "src/execution/messages.h"
#include "src/execution/stack-guard.h"
#include "src/handles/handles.h"
#include "src/heap/factory.h"
#include "src/heap/heap.h"
#include "src/heap/read-only-heap.h"
#include "src/init/isolate-allocator.h"
#include "src/objects/code.h"
#include "src/objects/contexts.h"
#include "src/objects/debug-objects.h"
#include "src/runtime/runtime.h"
#include "src/strings/unicode.h"
#include "src/utils/allocation.h"

#ifdef V8_INTL_SUPPORT
#include "unicode/uversion.h"  // Define U_ICU_NAMESPACE.
namespace U_ICU_NAMESPACE {
class UMemory;
}  // namespace U_ICU_NAMESPACE
#endif  // V8_INTL_SUPPORT

namespace v8 {

namespace base {
class RandomNumberGenerator;
}

namespace debug {
class ConsoleDelegate;
class AsyncEventDelegate;
}  // namespace debug

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
class Counters;
class Debug;
class DeoptimizerData;
class DescriptorLookupCache;
class EmbeddedFileWriterInterface;
class EternalHandles;
class HandleScopeImplementer;
class HeapObjectToIndexHashMap;
class HeapProfiler;
class InnerPointerToCodeCache;
class Logger;
class MaterializedObjectStore;
class Microtask;
class MicrotaskQueue;
class OptimizingCompileDispatcher;
class ReadOnlyDeserializer;
class RegExpStack;
class RootVisitor;
class RuntimeProfiler;
class SetupIsolateDelegate;
class Simulator;
class StandardFrame;
class StartupDeserializer;
class StubCache;
class ThreadManager;
class ThreadState;
class ThreadVisitor;  // Defined in v8threads.h
class TracingCpuProfilerImpl;
class UnicodeCache;
struct ManagedPtrDestructor;

template <StateTag Tag>
class VMState;

namespace interpreter {
class Interpreter;
}

namespace compiler {
class PerIsolateCompilerCache;
}

namespace wasm {
class WasmEngine;
}

namespace win64_unwindinfo {
class BuiltinUnwindInfo;
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
 * RETURN_RESULT_OR_FAILURE is used in functions with return type Object (such
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

#define ASSIGN_RETURN_ON_EXCEPTION_VALUE(isolate, dst, call, value) \
  do {                                                              \
    if (!(call).ToHandle(&dst)) {                                   \
      DCHECK((isolate)->has_pending_exception());                   \
      return value;                                                 \
    }                                                               \
  } while (false)

#define ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, dst, call)                \
  do {                                                                        \
    Isolate* __isolate__ = (isolate);                                         \
    ASSIGN_RETURN_ON_EXCEPTION_VALUE(__isolate__, dst, call,                  \
                                     ReadOnlyRoots(__isolate__).exception()); \
  } while (false)

#define ASSIGN_RETURN_ON_EXCEPTION(isolate, dst, call, T) \
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
 * If inside a function with return type Object, use
 * RETURN_FAILURE_ON_EXCEPTION instead.
 */
#define RETURN_ON_EXCEPTION_VALUE(isolate, call, value) \
  do {                                                  \
    if ((call).is_null()) {                             \
      DCHECK((isolate)->has_pending_exception());       \
      return value;                                     \
    }                                                   \
  } while (false)

/**
 * RETURN_FAILURE_ON_EXCEPTION conditionally returns the "exception" sentinel if
 * the given MaybeHandle is empty; so it can only be used in functions with
 * return type Object, such as RUNTIME_FUNCTION(...) {...} or BUILTIN(...)
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
 * If inside a function with return type Object, use
 * RETURN_FAILURE_ON_EXCEPTION instead.
 * If inside a function with return type
 * Maybe<X> or Handle<X>, use RETURN_ON_EXCEPTION_VALUE instead.
 */
#define RETURN_ON_EXCEPTION(isolate, call, T) \
  RETURN_ON_EXCEPTION_VALUE(isolate, call, MaybeHandle<T>())

#define RETURN_FAILURE(isolate, should_throw, call) \
  do {                                              \
    if ((should_throw) == kDontThrow) {             \
      return Just(false);                           \
    } else {                                        \
      isolate->Throw(*isolate->factory()->call);    \
      return Nothing<bool>();                       \
    }                                               \
  } while (false)

#define MAYBE_RETURN(call, value)         \
  do {                                    \
    if ((call).IsNothing()) return value; \
  } while (false)

#define MAYBE_RETURN_NULL(call) MAYBE_RETURN(call, MaybeHandle<Object>())

#define MAYBE_ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, dst, call) \
  do {                                                               \
    Isolate* __isolate__ = (isolate);                                \
    if (!(call).To(&dst)) {                                          \
      DCHECK(__isolate__->has_pending_exception());                  \
      return ReadOnlyRoots(__isolate__).exception();                 \
    }                                                                \
  } while (false)

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

#define FIELD_ACCESSOR(type, name)                \
  inline void set_##name(type v) { name##_ = v; } \
  inline type name() const { return name##_; }

// Controls for manual embedded blob lifecycle management, used by tests and
// mksnapshot.
V8_EXPORT_PRIVATE void DisableEmbeddedBlobRefcounting();
V8_EXPORT_PRIVATE void FreeCurrentEmbeddedBlob();

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

using DebugObjectCache = std::vector<Handle<HeapObject>>;

#define ISOLATE_INIT_LIST(V)                                                   \
  /* Assembler state. */                                                       \
  V(FatalErrorCallback, exception_behavior, nullptr)                           \
  V(OOMErrorCallback, oom_behavior, nullptr)                                   \
  V(LogEventCallback, event_logger, nullptr)                                   \
  V(AllowCodeGenerationFromStringsCallback, allow_code_gen_callback, nullptr)  \
  V(ModifyCodeGenerationFromStringsCallback, modify_code_gen_callback,         \
    nullptr)                                                                   \
  V(AllowWasmCodeGenerationCallback, allow_wasm_code_gen_callback, nullptr)    \
  V(ExtensionCallback, wasm_module_callback, &NoExtension)                     \
  V(ExtensionCallback, wasm_instance_callback, &NoExtension)                   \
  V(WasmStreamingCallback, wasm_streaming_callback, nullptr)                   \
  V(WasmThreadsEnabledCallback, wasm_threads_enabled_callback, nullptr)        \
  V(WasmLoadSourceMapCallback, wasm_load_source_map_callback, nullptr)         \
  /* State for Relocatable. */                                                 \
  V(Relocatable*, relocatable_top, nullptr)                                    \
  V(DebugObjectCache*, string_stream_debug_object_cache, nullptr)              \
  V(Object, string_stream_current_security_token, Object())                    \
  V(const intptr_t*, api_external_references, nullptr)                         \
  V(AddressToIndexHashMap*, external_reference_map, nullptr)                   \
  V(HeapObjectToIndexHashMap*, root_index_map, nullptr)                        \
  V(MicrotaskQueue*, default_microtask_queue, nullptr)                         \
  V(CompilationStatistics*, turbo_statistics, nullptr)                         \
  V(CodeTracer*, code_tracer, nullptr)                                         \
  V(uint32_t, per_isolate_assert_data, 0xFFFFFFFFu)                            \
  V(PromiseRejectCallback, promise_reject_callback, nullptr)                   \
  V(const v8::StartupData*, snapshot_blob, nullptr)                            \
  V(int, code_and_metadata_size, 0)                                            \
  V(int, bytecode_and_metadata_size, 0)                                        \
  V(int, external_script_source_size, 0)                                       \
  /* true if being profiled. Causes collection of extra compile info. */       \
  V(bool, is_profiling, false)                                                 \
  /* Number of CPU profilers running on the isolate. */                        \
  V(size_t, num_cpu_profilers, 0)                                              \
  /* true if a trace is being formatted through Error.prepareStackTrace. */    \
  V(bool, formatting_stack_trace, false)                                       \
  /* Perform side effect checks on function call and API callbacks. */         \
  V(DebugInfo::ExecutionMode, debug_execution_mode, DebugInfo::kBreakpoints)   \
  /* Current code coverage mode */                                             \
  V(debug::CoverageMode, code_coverage_mode, debug::CoverageMode::kBestEffort) \
  V(debug::TypeProfileMode, type_profile_mode, debug::TypeProfileMode::kNone)  \
  V(int, last_stack_frame_info_id, 0)                                          \
  V(int, last_console_context_id, 0)                                           \
  V(v8_inspector::V8Inspector*, inspector, nullptr)                            \
  V(bool, next_v8_call_is_safe_for_termination, false)                         \
  V(bool, only_terminate_in_safe_scope, false)                                 \
  V(bool, detailed_source_positions_for_profiling, FLAG_detailed_line_info)

#define THREAD_LOCAL_TOP_ACCESSOR(type, name)                         \
  inline void set_##name(type v) { thread_local_top()->name##_ = v; } \
  inline type name() const { return thread_local_top()->name##_; }

#define THREAD_LOCAL_TOP_ADDRESS(type, name) \
  type* name##_address() { return &thread_local_top()->name##_; }

// HiddenFactory exists so Isolate can privately inherit from it without making
// Factory's members available to Isolate directly.
class V8_EXPORT_PRIVATE HiddenFactory : private Factory {};

class Isolate final : private HiddenFactory {
  // These forward declarations are required to make the friend declarations in
  // PerIsolateThreadData work on some older versions of gcc.
  class ThreadDataTable;
  class EntryStackItem;

 public:
  // A thread has a PerIsolateThreadData instance for each isolate that it has
  // entered. That instance is allocated when the isolate is initially entered
  // and reused on subsequent entries.
  class PerIsolateThreadData {
   public:
    PerIsolateThreadData(Isolate* isolate, ThreadId thread_id)
        : isolate_(isolate),
          thread_id_(thread_id),
          stack_limit_(0),
          thread_state_(nullptr)
#if USE_SIMULATOR
          ,
          simulator_(nullptr)
#endif
    {
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
      return isolate_ == isolate && thread_id_ == thread_id;
    }

   private:
    Isolate* isolate_;
    ThreadId thread_id_;
    uintptr_t stack_limit_;
    ThreadState* thread_state_;

#if USE_SIMULATOR
    Simulator* simulator_;
#endif

    friend class Isolate;
    friend class ThreadDataTable;
    friend class EntryStackItem;

    DISALLOW_COPY_AND_ASSIGN(PerIsolateThreadData);
  };

  static void InitializeOncePerProcess();

  // Creates Isolate object. Must be used instead of constructing Isolate with
  // new operator.
  static V8_EXPORT_PRIVATE Isolate* New(
      IsolateAllocationMode mode = IsolateAllocationMode::kDefault);

  // Deletes Isolate object. Must be used instead of delete operator.
  // Destroys the non-default isolates.
  // Sets default isolate into "has_been_disposed" state rather then destroying,
  // for legacy API reasons.
  static void Delete(Isolate* isolate);

  void SetUpFromReadOnlyHeap(ReadOnlyHeap* ro_heap);

  // Returns allocation mode of this isolate.
  V8_INLINE IsolateAllocationMode isolate_allocation_mode();

  // Page allocator that must be used for allocating V8 heap pages.
  v8::PageAllocator* page_allocator();

  // Returns the PerIsolateThreadData for the current thread (or nullptr if one
  // is not currently set).
  static PerIsolateThreadData* CurrentPerIsolateThreadData() {
    return reinterpret_cast<PerIsolateThreadData*>(
        base::Thread::GetThreadLocal(per_isolate_thread_data_key_));
  }

  // Returns the isolate inside which the current thread is running or nullptr.
  V8_INLINE static Isolate* TryGetCurrent() {
    DCHECK_EQ(true, isolate_key_created_.load(std::memory_order_relaxed));
    return reinterpret_cast<Isolate*>(
        base::Thread::GetExistingThreadLocal(isolate_key_));
  }

  // Returns the isolate inside which the current thread is running.
  V8_INLINE static Isolate* Current() {
    Isolate* isolate = TryGetCurrent();
    DCHECK_NOT_NULL(isolate);
    return isolate;
  }

  // Usually called by Init(), but can be called early e.g. to allow
  // testing components that require logging but not the whole
  // isolate.
  //
  // Safe to call more than once.
  void InitializeLoggingAndCounters();
  bool InitializeCounters();  // Returns false if already initialized.

  bool InitWithoutSnapshot();
  bool InitWithSnapshot(ReadOnlyDeserializer* read_only_deserializer,
                        StartupDeserializer* startup_deserializer);

  // True if at least one thread Enter'ed this isolate.
  bool IsInUse() { return entry_stack_ != nullptr; }

  void ReleaseSharedPtrs();

  void ClearSerializerData();

  bool LogObjectRelocation();

  // Initializes the current thread to run this Isolate.
  // Not thread-safe. Multiple threads should not Enter/Exit the same isolate
  // at the same time, this should be prevented using external locking.
  void Enter();

  // Exits the current thread. The previosuly entered Isolate is restored
  // for the thread.
  // Not thread-safe. Multiple threads should not Enter/Exit the same isolate
  // at the same time, this should be prevented using external locking.
  void Exit();

  // Find the PerThread for this particular (isolate, thread) combination.
  // If one does not yet exist, allocate a new one.
  PerIsolateThreadData* FindOrAllocatePerThreadDataForThisThread();

  // Find the PerThread for this particular (isolate, thread) combination
  // If one does not yet exist, return null.
  PerIsolateThreadData* FindPerThreadDataForThisThread();

  // Find the PerThread for given (isolate, thread) combination
  // If one does not yet exist, return null.
  PerIsolateThreadData* FindPerThreadDataForThread(ThreadId thread_id);

  // Discard the PerThread for this particular (isolate, thread) combination
  // If one does not yet exist, no-op.
  void DiscardPerThreadDataForThisThread();

  // Mutex for serializing access to break control structures.
  base::RecursiveMutex* break_access() { return &break_access_; }

  Address get_address_from_id(IsolateAddressId id);

  // Access to top context (where the current function object was created).
  Context context() { return thread_local_top()->context_; }
  inline void set_context(Context context);
  Context* context_address() { return &thread_local_top()->context_; }

  // Access to current thread id.
  THREAD_LOCAL_TOP_ACCESSOR(ThreadId, thread_id)

  // Interface to pending exception.
  inline Object pending_exception();
  inline void set_pending_exception(Object exception_obj);
  inline void clear_pending_exception();

  V8_EXPORT_PRIVATE bool AreWasmThreadsEnabled(Handle<Context> context);

  THREAD_LOCAL_TOP_ADDRESS(Object, pending_exception)

  inline bool has_pending_exception();

  THREAD_LOCAL_TOP_ADDRESS(Context, pending_handler_context)
  THREAD_LOCAL_TOP_ADDRESS(Address, pending_handler_entrypoint)
  THREAD_LOCAL_TOP_ADDRESS(Address, pending_handler_constant_pool)
  THREAD_LOCAL_TOP_ADDRESS(Address, pending_handler_fp)
  THREAD_LOCAL_TOP_ADDRESS(Address, pending_handler_sp)

  THREAD_LOCAL_TOP_ACCESSOR(bool, external_caught_exception)

  v8::TryCatch* try_catch_handler() {
    return thread_local_top()->try_catch_handler_;
  }
  bool* external_caught_exception_address() {
    return &thread_local_top()->external_caught_exception_;
  }

  THREAD_LOCAL_TOP_ADDRESS(Object, scheduled_exception)

  inline void clear_pending_message();
  Address pending_message_obj_address() {
    return reinterpret_cast<Address>(&thread_local_top()->pending_message_obj_);
  }

  inline Object scheduled_exception();
  inline bool has_scheduled_exception();
  inline void clear_scheduled_exception();

  bool IsJavaScriptHandlerOnTop(Object exception);
  bool IsExternalHandlerOnTop(Object exception);

  inline bool is_catchable_by_javascript(Object exception);

  // JS execution stack (see frames.h).
  static Address c_entry_fp(ThreadLocalTop* thread) {
    return thread->c_entry_fp_;
  }
  static Address handler(ThreadLocalTop* thread) { return thread->handler_; }
  Address c_function() { return thread_local_top()->c_function_; }

  inline Address* c_entry_fp_address() {
    return &thread_local_top()->c_entry_fp_;
  }
  static uint32_t c_entry_fp_offset() {
    return static_cast<uint32_t>(
        OFFSET_OF(Isolate, thread_local_top()->c_entry_fp_) -
        isolate_root_bias());
  }
  inline Address* handler_address() { return &thread_local_top()->handler_; }
  inline Address* c_function_address() {
    return &thread_local_top()->c_function_;
  }

  // Bottom JS entry.
  Address js_entry_sp() { return thread_local_top()->js_entry_sp_; }
  inline Address* js_entry_sp_address() {
    return &thread_local_top()->js_entry_sp_;
  }

  // Returns the global object of the current context. It could be
  // a builtin object, or a JS global object.
  inline Handle<JSGlobalObject> global_object();

  // Returns the global proxy object of the current context.
  inline Handle<JSGlobalProxy> global_proxy();

  static int ArchiveSpacePerThread() { return sizeof(ThreadLocalTop); }
  void FreeThreadResources() { thread_local_top()->Free(); }

  // This method is called by the api after operations that may throw
  // exceptions.  If an exception was thrown and not handled by an external
  // handler the exception is scheduled to be rethrown when we return to running
  // JavaScript code.  If an exception is scheduled true is returned.
  V8_EXPORT_PRIVATE bool OptionalRescheduleException(bool clear_exception);

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

  V8_EXPORT_PRIVATE void SetCaptureStackTraceForUncaughtExceptions(
      bool capture, int frame_limit, StackTrace::StackTraceOptions options);

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
  Object Throw(Object exception, MessageLocation* location = nullptr);
  Object ThrowIllegalOperation();

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
  Object ReThrow(Object exception);

  // Find the correct handler for the current pending exception. This also
  // clears and returns the current pending exception.
  Object UnwindAndFindHandler();

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

  V8_EXPORT_PRIVATE void ScheduleThrow(Object exception);
  // Re-set pending message, script and positions reported to the TryCatch
  // back to the TLS for re-use when rethrowing.
  void RestorePendingMessageFromTryCatch(v8::TryCatch* handler);
  // Un-schedule an exception that was caught by a TryCatch handler.
  void CancelScheduledExceptionFromTryCatch(v8::TryCatch* handler);
  void ReportPendingMessages();
  void ReportPendingMessagesFromJavaScript();

  // Implements code shared between the two above methods
  void ReportPendingMessagesImpl(bool report_externally);

  // Promote a scheduled exception to pending. Asserts has_scheduled_exception.
  Object PromoteScheduledException();

  // Attempts to compute the current source location, storing the
  // result in the target out parameter. The source location is attached to a
  // Message object as the location which should be shown to the user. It's
  // typically the top-most meaningful location on the stack.
  bool ComputeLocation(MessageLocation* target);
  bool ComputeLocationFromException(MessageLocation* target,
                                    Handle<Object> exception);
  V8_EXPORT_PRIVATE bool ComputeLocationFromStackTrace(
      MessageLocation* target, Handle<Object> exception);

  V8_EXPORT_PRIVATE Handle<JSMessageObject> CreateMessage(
      Handle<Object> exception, MessageLocation* location);

  // Out of resource exception helpers.
  Object StackOverflow();
  Object TerminateExecution();
  void CancelTerminateExecution();

  V8_EXPORT_PRIVATE void RequestInterrupt(InterruptCallback callback,
                                          void* data);
  void InvokeApiInterruptCallbacks();

  // Administration
  void Iterate(RootVisitor* v);
  void Iterate(RootVisitor* v, ThreadLocalTop* t);
  char* Iterate(RootVisitor* v, char* t);
  void IterateThread(ThreadVisitor* v, char* t);

  // Returns the current native context.
  inline Handle<NativeContext> native_context();
  inline NativeContext raw_native_context();

  Handle<Context> GetIncumbentContext();

  void RegisterTryCatchHandler(v8::TryCatch* that);
  void UnregisterTryCatchHandler(v8::TryCatch* that);

  char* ArchiveThread(char* to);
  char* RestoreThread(char* from);

  static const int kUC16AlphabetSize = 256;  // See StringSearchBase.
  static const int kBMMaxShift = 250;        // See StringSearchBase.

  // Accessors.
#define GLOBAL_ACCESSOR(type, name, initialvalue)                \
  inline type name() const {                                     \
    DCHECK(OFFSET_OF(Isolate, name##_) == name##_debug_offset_); \
    return name##_;                                              \
  }                                                              \
  inline void set_##name(type value) {                           \
    DCHECK(OFFSET_OF(Isolate, name##_) == name##_debug_offset_); \
    name##_ = value;                                             \
  }
  ISOLATE_INIT_LIST(GLOBAL_ACCESSOR)
#undef GLOBAL_ACCESSOR

#define GLOBAL_ARRAY_ACCESSOR(type, name, length)                \
  inline type* name() {                                          \
    DCHECK(OFFSET_OF(Isolate, name##_) == name##_debug_offset_); \
    return &(name##_)[0];                                        \
  }
  ISOLATE_INIT_ARRAY_LIST(GLOBAL_ARRAY_ACCESSOR)
#undef GLOBAL_ARRAY_ACCESSOR

#define NATIVE_CONTEXT_FIELD_ACCESSOR(index, type, name) \
  inline Handle<type> name();                            \
  inline bool is_##name(type value);
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
  StackGuard* stack_guard() { return isolate_data()->stack_guard(); }
  Heap* heap() { return &heap_; }
  ReadOnlyHeap* read_only_heap() const { return read_only_heap_; }
  static Isolate* FromHeap(Heap* heap) {
    return reinterpret_cast<Isolate*>(reinterpret_cast<Address>(heap) -
                                      OFFSET_OF(Isolate, heap_));
  }

  const IsolateData* isolate_data() const { return &isolate_data_; }
  IsolateData* isolate_data() { return &isolate_data_; }

  // Generated code can embed this address to get access to the isolate-specific
  // data (for example, roots, external references, builtins, etc.).
  // The kRootRegister is set to this value.
  Address isolate_root() const { return isolate_data()->isolate_root(); }
  static size_t isolate_root_bias() {
    return OFFSET_OF(Isolate, isolate_data_) + IsolateData::kIsolateRootBias;
  }
  static Isolate* FromRoot(Address isolate_root) {
    return reinterpret_cast<Isolate*>(isolate_root - isolate_root_bias());
  }

  RootsTable& roots_table() { return isolate_data()->roots(); }

  // A sub-region of the Isolate object that has "predictable" layout which
  // depends only on the pointer size and therefore it's guaranteed that there
  // will be no compatibility issues because of different compilers used for
  // snapshot generator and actual V8 code.
  // Thus, kRootRegister may be used to address any location that falls into
  // this region.
  // See IsolateData::AssertPredictableLayout() for details.
  base::AddressRegion root_register_addressable_region() const {
    return base::AddressRegion(reinterpret_cast<Address>(&isolate_data_),
                               sizeof(IsolateData));
  }

  Object root(RootIndex index) { return Object(roots_table()[index]); }

  Handle<Object> root_handle(RootIndex index) {
    return Handle<Object>(&roots_table()[index]);
  }

  ExternalReferenceTable* external_reference_table() {
    DCHECK(isolate_data()->external_reference_table()->is_initialized());
    return isolate_data()->external_reference_table();
  }

  Address* builtin_entry_table() { return isolate_data_.builtin_entry_table(); }
  V8_INLINE Address* builtins_table() { return isolate_data_.builtins(); }

  StubCache* load_stub_cache() { return load_stub_cache_; }
  StubCache* store_stub_cache() { return store_stub_cache_; }
  DeoptimizerData* deoptimizer_data() { return deoptimizer_data_; }
  bool deoptimizer_lazy_throw() const { return deoptimizer_lazy_throw_; }
  void set_deoptimizer_lazy_throw(bool value) {
    deoptimizer_lazy_throw_ = value;
  }
  void InitializeThreadLocal();
  ThreadLocalTop* thread_local_top() {
    return &isolate_data_.thread_local_top_;
  }
  ThreadLocalTop const* thread_local_top() const {
    return &isolate_data_.thread_local_top_;
  }

  static uint32_t thread_in_wasm_flag_address_offset() {
    // For WebAssembly trap handlers there is a flag in thread-local storage
    // which indicates that the executing thread executes WebAssembly code. To
    // access this flag directly from generated code, we store a pointer to the
    // flag in ThreadLocalTop in thread_in_wasm_flag_address_. This function
    // here returns the offset of that member from {isolate_root()}.
    return static_cast<uint32_t>(
        OFFSET_OF(Isolate, thread_local_top()->thread_in_wasm_flag_address_) -
        isolate_root_bias());
  }

  MaterializedObjectStore* materialized_object_store() {
    return materialized_object_store_;
  }

  DescriptorLookupCache* descriptor_lookup_cache() {
    return descriptor_lookup_cache_;
  }

  HandleScopeData* handle_scope_data() { return &handle_scope_data_; }

  HandleScopeImplementer* handle_scope_implementer() {
    DCHECK(handle_scope_implementer_);
    return handle_scope_implementer_;
  }

  UnicodeCache* unicode_cache() { return unicode_cache_; }

  InnerPointerToCodeCache* inner_pointer_to_code_cache() {
    return inner_pointer_to_code_cache_;
  }

  GlobalHandles* global_handles() { return global_handles_; }

  EternalHandles* eternal_handles() { return eternal_handles_; }

  ThreadManager* thread_manager() { return thread_manager_; }

#ifndef V8_INTL_SUPPORT
  unibrow::Mapping<unibrow::Ecma262UnCanonicalize>* jsregexp_uncanonicalize() {
    return &jsregexp_uncanonicalize_;
  }

  unibrow::Mapping<unibrow::CanonicalizationRange>* jsregexp_canonrange() {
    return &jsregexp_canonrange_;
  }

  unibrow::Mapping<unibrow::Ecma262Canonicalize>*
  regexp_macro_assembler_canonicalize() {
    return &regexp_macro_assembler_canonicalize_;
  }
#endif  // !V8_INTL_SUPPORT

  RuntimeState* runtime_state() { return &runtime_state_; }

  Builtins* builtins() { return &builtins_; }

  RegExpStack* regexp_stack() { return regexp_stack_; }

  size_t total_regexp_code_generated() { return total_regexp_code_generated_; }
  void IncreaseTotalRegexpCodeGenerated(int size) {
    total_regexp_code_generated_ += size;
  }

  std::vector<int>* regexp_indices() { return &regexp_indices_; }

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
    isolate_data_.embedder_data_[slot] = data;
  }
  void* GetData(uint32_t slot) {
    DCHECK_LT(slot, Internals::kNumIsolateDataSlots);
    return isolate_data_.embedder_data_[slot];
  }

  bool serializer_enabled() const { return serializer_enabled_; }

  void enable_serializer() { serializer_enabled_ = true; }

  bool snapshot_available() const {
    return snapshot_blob_ != nullptr && snapshot_blob_->raw_size != 0;
  }

  bool IsDead() { return has_fatal_error_; }
  void SignalFatalError() { has_fatal_error_ = true; }

  V8_EXPORT_PRIVATE bool use_optimizer();

  bool initialized_from_snapshot() { return initialized_from_snapshot_; }

  bool NeedsSourcePositionsForProfiling() const;

  V8_EXPORT_PRIVATE bool NeedsDetailedOptimizedCodeLineInfo() const;

  bool is_best_effort_code_coverage() const {
    return code_coverage_mode() == debug::CoverageMode::kBestEffort;
  }

  bool is_precise_count_code_coverage() const {
    return code_coverage_mode() == debug::CoverageMode::kPreciseCount;
  }

  bool is_precise_binary_code_coverage() const {
    return code_coverage_mode() == debug::CoverageMode::kPreciseBinary;
  }

  bool is_block_count_code_coverage() const {
    return code_coverage_mode() == debug::CoverageMode::kBlockCount;
  }

  bool is_block_binary_code_coverage() const {
    return code_coverage_mode() == debug::CoverageMode::kBlockBinary;
  }

  bool is_block_code_coverage() const {
    return is_block_count_code_coverage() || is_block_binary_code_coverage();
  }

  bool is_binary_code_coverage() const {
    return is_precise_binary_code_coverage() || is_block_binary_code_coverage();
  }

  bool is_count_code_coverage() const {
    return is_precise_count_code_coverage() || is_block_count_code_coverage();
  }

  bool is_collecting_type_profile() const {
    return type_profile_mode() == debug::TypeProfileMode::kCollect;
  }

  // Collect feedback vectors with data for code coverage or type profile.
  // Reset the list, when both code coverage and type profile are not
  // needed anymore. This keeps many feedback vectors alive, but code
  // coverage or type profile are used for debugging only and increase in
  // memory usage is expected.
  void SetFeedbackVectorsForProfilingTools(Object value);

  void MaybeInitializeVectorListFromHeap();

  double time_millis_since_init() {
    return heap_.MonotonicallyIncreasingTimeInMs() - time_millis_at_init_;
  }

  DateCache* date_cache() { return date_cache_; }

  V8_EXPORT_PRIVATE void set_date_cache(DateCache* date_cache);

#ifdef V8_INTL_SUPPORT

  const std::string& default_locale() { return default_locale_; }

  void ResetDefaultLocale() { default_locale_.clear(); }

  void set_default_locale(const std::string& locale) {
    DCHECK_EQ(default_locale_.length(), 0);
    default_locale_ = locale;
  }

  // enum to access the icu object cache.
  enum class ICUObjectCacheType{
      kDefaultCollator, kDefaultNumberFormat, kDefaultSimpleDateFormat,
      kDefaultSimpleDateFormatForTime, kDefaultSimpleDateFormatForDate};

  icu::UMemory* get_cached_icu_object(ICUObjectCacheType cache_type);
  void set_icu_object_in_cache(ICUObjectCacheType cache_type,
                               std::shared_ptr<icu::UMemory> obj);
  void clear_cached_icu_object(ICUObjectCacheType cache_type);

#endif  // V8_INTL_SUPPORT

  bool IsArrayOrObjectOrStringPrototype(Object object);

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

  // Returns true if array is the initial array prototype in any native context.
  bool IsAnyInitialArrayPrototype(Handle<JSArray> array);

  void IterateDeferredHandles(RootVisitor* visitor);
  void LinkDeferredHandles(DeferredHandles* deferred_handles);
  void UnlinkDeferredHandles(DeferredHandles* deferred_handles);

#ifdef DEBUG
  bool IsDeferredHandle(Address* location);
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

  int id() const { return id_; }

  CompilationStatistics* GetTurboStatistics();
  V8_EXPORT_PRIVATE CodeTracer* GetCodeTracer();

  void DumpAndResetStats();

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
  V8_EXPORT_PRIVATE Code FindCodeObject(Address a);

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
  void FireCallCompletedCallback(MicrotaskQueue* microtask_queue);

  void AddBeforeCallEnteredCallback(BeforeCallEnteredCallback callback);
  void RemoveBeforeCallEnteredCallback(BeforeCallEnteredCallback callback);
  inline void FireBeforeCallEnteredCallback();

  void SetPromiseRejectCallback(PromiseRejectCallback callback);
  void ReportPromiseReject(Handle<JSPromise> promise, Handle<Object> value,
                           v8::PromiseRejectEvent event);

  void SetTerminationOnExternalTryCatch();

  Handle<Symbol> SymbolFor(RootIndex dictionary_index, Handle<String> name,
                           bool private_symbol);

  V8_EXPORT_PRIVATE void SetUseCounterCallback(
      v8::Isolate::UseCounterCallback callback);
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

  Address promise_hook_or_debug_is_active_or_async_event_delegate_address() {
    return reinterpret_cast<Address>(
        &promise_hook_or_debug_is_active_or_async_event_delegate_);
  }

  Address handle_scope_implementer_address() {
    return reinterpret_cast<Address>(&handle_scope_implementer_);
  }

  void SetAtomicsWaitCallback(v8::Isolate::AtomicsWaitCallback callback,
                              void* data);
  void RunAtomicsWaitCallback(v8::Isolate::AtomicsWaitEvent event,
                              Handle<JSArrayBuffer> array_buffer,
                              size_t offset_in_bytes, int64_t value,
                              double timeout_in_ms,
                              AtomicsWaitWakeHandle* stop_handle);

  V8_EXPORT_PRIVATE void SetPromiseHook(PromiseHook hook);
  V8_EXPORT_PRIVATE void RunPromiseHook(PromiseHookType type,
                                        Handle<JSPromise> promise,
                                        Handle<Object> parent);
  void PromiseHookStateUpdated();

  void AddDetachedContext(Handle<Context> context);
  void CheckDetachedContextsAfterGC();

  void AddSharedWasmMemory(Handle<WasmMemoryObject> memory_object);

  std::vector<Object>* partial_snapshot_cache() {
    return &partial_snapshot_cache_;
  }

  bool IsGeneratingEmbeddedBuiltins() const {
    return builtins_constants_table_builder() != nullptr;
  }

  BuiltinsConstantsTableBuilder* builtins_constants_table_builder() const {
    return builtins_constants_table_builder_;
  }

  // Hashes bits of the Isolate that are relevant for embedded builtins. In
  // particular, the embedded blob requires builtin Code object layout and the
  // builtins constants table to remain unchanged from build-time.
  size_t HashIsolateForEmbeddedBlob();

  V8_EXPORT_PRIVATE static const uint8_t* CurrentEmbeddedBlob();
  V8_EXPORT_PRIVATE static uint32_t CurrentEmbeddedBlobSize();
  static bool CurrentEmbeddedBlobIsBinaryEmbedded();

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

  void set_array_buffer_allocator_shared(
      std::shared_ptr<v8::ArrayBuffer::Allocator> allocator) {
    array_buffer_allocator_shared_ = std::move(allocator);
  }
  std::shared_ptr<v8::ArrayBuffer::Allocator> array_buffer_allocator_shared()
      const {
    return array_buffer_allocator_shared_;
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

  bool IsInAnyContext(Object object, uint32_t index);

  void ClearKeptObjects();
  void SetHostCleanupFinalizationGroupCallback(
      HostCleanupFinalizationGroupCallback callback);
  void RunHostCleanupFinalizationGroupCallback(Handle<JSFinalizationGroup> fg);

  void SetHostImportModuleDynamicallyCallback(
      HostImportModuleDynamicallyCallback callback);
  V8_EXPORT_PRIVATE MaybeHandle<JSPromise>
  RunHostImportModuleDynamicallyCallback(Handle<Script> referrer,
                                         Handle<Object> specifier);

  void SetHostInitializeImportMetaObjectCallback(
      HostInitializeImportMetaObjectCallback callback);
  V8_EXPORT_PRIVATE Handle<JSObject> RunHostInitializeImportMetaObjectCallback(
      Handle<SourceTextModule> module);

  void RegisterEmbeddedFileWriter(EmbeddedFileWriterInterface* writer) {
    embedded_file_writer_ = writer;
  }

  int LookupOrAddExternallyCompiledFilename(const char* filename);
  const char* GetExternallyCompiledFilename(int index) const;
  int GetExternallyCompiledFilenameCount() const;
  // PrepareBuiltinSourcePositionMap is necessary in order to preserve the
  // builtin source positions before the corresponding code objects are
  // replaced with trampolines. Those source positions are used to
  // annotate the builtin blob with debugging information.
  void PrepareBuiltinSourcePositionMap();

#if defined(V8_OS_WIN64)
  void SetBuiltinUnwindData(
      int builtin_index,
      const win64_unwindinfo::BuiltinUnwindInfo& unwinding_info);
#endif  // V8_OS_WIN64

  void SetPrepareStackTraceCallback(PrepareStackTraceCallback callback);
  MaybeHandle<Object> RunPrepareStackTraceCallback(Handle<Context>,
                                                   Handle<JSObject> Error,
                                                   Handle<JSArray> sites);
  bool HasPrepareStackTraceCallback() const;

  void SetAddCrashKeyCallback(AddCrashKeyCallback callback);
  void AddCrashKey(CrashKeyId id, const std::string& value) {
    if (add_crash_key_callback_) {
      add_crash_key_callback_(id, value);
    }
  }

  void SetRAILMode(RAILMode rail_mode);

  RAILMode rail_mode() { return rail_mode_.load(); }

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
  V8_EXPORT_PRIVATE void RegisterManagedPtrDestructor(
      ManagedPtrDestructor* finalizer);

  // Removes a previously-registered shared object finalizer.
  void UnregisterManagedPtrDestructor(ManagedPtrDestructor* finalizer);

  size_t elements_deletion_counter() { return elements_deletion_counter_; }
  void set_elements_deletion_counter(size_t value) {
    elements_deletion_counter_ = value;
  }

  wasm::WasmEngine* wasm_engine() const { return wasm_engine_.get(); }
  V8_EXPORT_PRIVATE void SetWasmEngine(
      std::shared_ptr<wasm::WasmEngine> engine);

  const v8::Context::BackupIncumbentScope* top_backup_incumbent_scope() const {
    return top_backup_incumbent_scope_;
  }
  void set_top_backup_incumbent_scope(
      const v8::Context::BackupIncumbentScope* top_backup_incumbent_scope) {
    top_backup_incumbent_scope_ = top_backup_incumbent_scope;
  }

  V8_EXPORT_PRIVATE void SetIdle(bool is_idle);

  // Changing various modes can cause differences in generated bytecode which
  // interferes with lazy source positions, so this should be called immediately
  // before such a mode change to ensure that this cannot happen.
  V8_EXPORT_PRIVATE void CollectSourcePositionsForAllBytecodeArrays();

 private:
  explicit Isolate(std::unique_ptr<IsolateAllocator> isolate_allocator);
  ~Isolate();

  V8_EXPORT_PRIVATE bool Init(ReadOnlyDeserializer* read_only_deserializer,
                              StartupDeserializer* startup_deserializer);

  void CheckIsolateLayout();

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
                   Isolate* previous_isolate, EntryStackItem* previous_item)
        : entry_count(1),
          previous_thread_data(previous_thread_data),
          previous_isolate(previous_isolate),
          previous_item(previous_item) {}

    int entry_count;
    PerIsolateThreadData* previous_thread_data;
    Isolate* previous_isolate;
    EntryStackItem* previous_item;

   private:
    DISALLOW_COPY_AND_ASSIGN(EntryStackItem);
  };

  static base::Thread::LocalStorageKey per_isolate_thread_data_key_;
  static base::Thread::LocalStorageKey isolate_key_;

#ifdef DEBUG
  static std::atomic<bool> isolate_key_created_;
#endif

  void Deinit();

  static void SetIsolateThreadLocals(Isolate* isolate,
                                     PerIsolateThreadData* data);

  void MarkCompactPrologue(bool is_compacting,
                           ThreadLocalTop* archived_thread_data);
  void MarkCompactEpilogue(bool is_compacting,
                           ThreadLocalTop* archived_thread_data);

  void FillCache();

  // Propagate pending exception message to the v8::TryCatch.
  // If there is no external try-catch or message was successfully propagated,
  // then return true.
  bool PropagatePendingExceptionToExternalTryCatch();

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

  void AddCrashKeysForIsolateAndHeapPointers();

  // This class contains a collection of data accessible from both C++ runtime
  // and compiled code (including assembly stubs, builtins, interpreter bytecode
  // handlers and optimized code).
  IsolateData isolate_data_;

  std::unique_ptr<IsolateAllocator> isolate_allocator_;
  Heap heap_;
  ReadOnlyHeap* read_only_heap_ = nullptr;

  const int id_;
  EntryStackItem* entry_stack_ = nullptr;
  int stack_trace_nesting_level_ = 0;
  StringStream* incomplete_message_ = nullptr;
  Address isolate_addresses_[kIsolateAddressCount + 1] = {};
  Bootstrapper* bootstrapper_ = nullptr;
  RuntimeProfiler* runtime_profiler_ = nullptr;
  CompilationCache* compilation_cache_ = nullptr;
  std::shared_ptr<Counters> async_counters_;
  base::RecursiveMutex break_access_;
  Logger* logger_ = nullptr;
  StubCache* load_stub_cache_ = nullptr;
  StubCache* store_stub_cache_ = nullptr;
  DeoptimizerData* deoptimizer_data_ = nullptr;
  bool deoptimizer_lazy_throw_ = false;
  MaterializedObjectStore* materialized_object_store_ = nullptr;
  bool capture_stack_trace_for_uncaught_exceptions_ = false;
  int stack_trace_for_uncaught_exceptions_frame_limit_ = 0;
  StackTrace::StackTraceOptions stack_trace_for_uncaught_exceptions_options_ =
      StackTrace::kOverview;
  DescriptorLookupCache* descriptor_lookup_cache_ = nullptr;
  HandleScopeData handle_scope_data_;
  HandleScopeImplementer* handle_scope_implementer_ = nullptr;
  UnicodeCache* unicode_cache_ = nullptr;
  AccountingAllocator* allocator_ = nullptr;
  InnerPointerToCodeCache* inner_pointer_to_code_cache_ = nullptr;
  GlobalHandles* global_handles_ = nullptr;
  EternalHandles* eternal_handles_ = nullptr;
  ThreadManager* thread_manager_ = nullptr;
  RuntimeState runtime_state_;
  Builtins builtins_;
  SetupIsolateDelegate* setup_delegate_ = nullptr;
#ifndef V8_INTL_SUPPORT
  unibrow::Mapping<unibrow::Ecma262UnCanonicalize> jsregexp_uncanonicalize_;
  unibrow::Mapping<unibrow::CanonicalizationRange> jsregexp_canonrange_;
  unibrow::Mapping<unibrow::Ecma262Canonicalize>
      regexp_macro_assembler_canonicalize_;
#endif  // !V8_INTL_SUPPORT
  RegExpStack* regexp_stack_ = nullptr;
  std::vector<int> regexp_indices_;
  DateCache* date_cache_ = nullptr;
  base::RandomNumberGenerator* random_number_generator_ = nullptr;
  base::RandomNumberGenerator* fuzzer_rng_ = nullptr;
  std::atomic<RAILMode> rail_mode_;
  v8::Isolate::AtomicsWaitCallback atomics_wait_callback_ = nullptr;
  void* atomics_wait_callback_data_ = nullptr;
  PromiseHook promise_hook_ = nullptr;
  HostCleanupFinalizationGroupCallback
      host_cleanup_finalization_group_callback_ = nullptr;
  HostImportModuleDynamicallyCallback host_import_module_dynamically_callback_ =
      nullptr;
  HostInitializeImportMetaObjectCallback
      host_initialize_import_meta_object_callback_ = nullptr;
  base::Mutex rail_mutex_;
  double load_start_time_ms_ = 0;

#ifdef V8_INTL_SUPPORT
  std::string default_locale_;

  struct ICUObjectCacheTypeHash {
    std::size_t operator()(ICUObjectCacheType a) const {
      return static_cast<std::size_t>(a);
    }
  };
  std::unordered_map<ICUObjectCacheType, std::shared_ptr<icu::UMemory>,
                     ICUObjectCacheTypeHash>
      icu_object_cache_;

#endif  // V8_INTL_SUPPORT

  // Whether the isolate has been created for snapshotting.
  bool serializer_enabled_ = false;

  // True if fatal error has been signaled for this isolate.
  bool has_fatal_error_ = false;

  // True if this isolate was initialized from a snapshot.
  bool initialized_from_snapshot_ = false;

  // TODO(ishell): remove
  // True if ES2015 tail call elimination feature is enabled.
  bool is_tail_call_elimination_enabled_ = true;

  // True if the isolate is in background. This flag is used
  // to prioritize between memory usage and latency.
  bool is_isolate_in_background_ = false;

  // True if the isolate is in memory savings mode. This flag is used to
  // favor memory over runtime performance.
  bool memory_savings_mode_active_ = false;

  // Time stamp at initialization.
  double time_millis_at_init_ = 0;

#ifdef DEBUG
  V8_EXPORT_PRIVATE static std::atomic<size_t> non_disposed_isolates_;

  JSObject::SpillInformation js_spill_information_;
#endif

  Debug* debug_ = nullptr;
  HeapProfiler* heap_profiler_ = nullptr;
  std::unique_ptr<CodeEventDispatcher> code_event_dispatcher_;

  const AstStringConstants* ast_string_constants_ = nullptr;

  interpreter::Interpreter* interpreter_ = nullptr;

  compiler::PerIsolateCompilerCache* compiler_cache_ = nullptr;
  // The following zone is for compiler-related objects that should live
  // through all compilations (and thus all JSHeapBroker instances).
  Zone* compiler_zone_ = nullptr;

  CompilerDispatcher* compiler_dispatcher_ = nullptr;

  using InterruptEntry = std::pair<InterruptCallback, void*>;
  std::queue<InterruptEntry> api_interrupts_queue_;

#define GLOBAL_BACKING_STORE(type, name, initialvalue) type name##_;
  ISOLATE_INIT_LIST(GLOBAL_BACKING_STORE)
#undef GLOBAL_BACKING_STORE

#define GLOBAL_ARRAY_BACKING_STORE(type, name, length) type name##_[length];
  ISOLATE_INIT_ARRAY_LIST(GLOBAL_ARRAY_BACKING_STORE)
#undef GLOBAL_ARRAY_BACKING_STORE

#ifdef DEBUG
  // This class is huge and has a number of fields controlled by
  // preprocessor defines. Make sure the offsets of these fields agree
  // between compilation units.
#define ISOLATE_FIELD_OFFSET(type, name, ignored) \
  V8_EXPORT_PRIVATE static const intptr_t name##_debug_offset_;
  ISOLATE_INIT_LIST(ISOLATE_FIELD_OFFSET)
  ISOLATE_INIT_ARRAY_LIST(ISOLATE_FIELD_OFFSET)
#undef ISOLATE_FIELD_OFFSET
#endif

  DeferredHandles* deferred_handles_head_ = nullptr;
  OptimizingCompileDispatcher* optimizing_compile_dispatcher_ = nullptr;

  // Counts deopt points if deopt_every_n_times is enabled.
  unsigned int stress_deopt_count_ = 0;

  bool force_slow_path_ = false;

  int next_optimization_id_ = 0;

#if V8_SFI_HAS_UNIQUE_ID
  int next_unique_sfi_id_ = 0;
#endif

  // Vector of callbacks before a Call starts execution.
  std::vector<BeforeCallEnteredCallback> before_call_entered_callbacks_;

  // Vector of callbacks when a Call completes.
  std::vector<CallCompletedCallback> call_completed_callbacks_;

  v8::Isolate::UseCounterCallback use_counter_callback_ = nullptr;

  std::vector<Object> partial_snapshot_cache_;

  // Used during builtins compilation to build the builtins constants table,
  // which is stored on the root list prior to serialization.
  BuiltinsConstantsTableBuilder* builtins_constants_table_builder_ = nullptr;

  void InitializeDefaultEmbeddedBlob();
  void CreateAndSetEmbeddedBlob();
  void TearDownEmbeddedBlob();

  void SetEmbeddedBlob(const uint8_t* blob, uint32_t blob_size);
  void ClearEmbeddedBlob();

  const uint8_t* embedded_blob_ = nullptr;
  uint32_t embedded_blob_size_ = 0;

  v8::ArrayBuffer::Allocator* array_buffer_allocator_ = nullptr;
  std::shared_ptr<v8::ArrayBuffer::Allocator> array_buffer_allocator_shared_;

  FutexWaitListNode futex_wait_list_node_;

  CancelableTaskManager* cancelable_task_manager_ = nullptr;

  debug::ConsoleDelegate* console_delegate_ = nullptr;

  debug::AsyncEventDelegate* async_event_delegate_ = nullptr;
  bool promise_hook_or_async_event_delegate_ = false;
  bool promise_hook_or_debug_is_active_or_async_event_delegate_ = false;
  int async_task_count_ = 0;

  v8::Isolate::AbortOnUncaughtExceptionCallback
      abort_on_uncaught_exception_callback_ = nullptr;

  bool allow_atomics_wait_ = true;

  base::Mutex managed_ptr_destructors_mutex_;
  ManagedPtrDestructor* managed_ptr_destructors_head_ = nullptr;

  size_t total_regexp_code_generated_ = 0;

  size_t elements_deletion_counter_ = 0;

  std::shared_ptr<wasm::WasmEngine> wasm_engine_;

  std::unique_ptr<TracingCpuProfilerImpl> tracing_cpu_profiler_;

  EmbeddedFileWriterInterface* embedded_file_writer_ = nullptr;

  // The top entry of the v8::Context::BackupIncumbentScope stack.
  const v8::Context::BackupIncumbentScope* top_backup_incumbent_scope_ =
      nullptr;

  PrepareStackTraceCallback prepare_stack_trace_callback_ = nullptr;

  // TODO(kenton@cloudflare.com): This mutex can be removed if
  // thread_data_table_ is always accessed under the isolate lock. I do not
  // know if this is the case, so I'm preserving it for now.
  base::Mutex thread_data_table_mutex_;
  ThreadDataTable thread_data_table_;

  // Enables the host application to provide a mechanism for recording a
  // predefined set of data as crash keys to be used in postmortem debugging
  // in case of a crash.
  AddCrashKeyCallback add_crash_key_callback_ = nullptr;

  // Delete new/delete operators to ensure that Isolate::New() and
  // Isolate::Delete() are used for Isolate creation and deletion.
  void* operator new(size_t, void* ptr) { return ptr; }
  void* operator new(size_t) = delete;
  void operator delete(void*) = delete;

  friend class heap::HeapTester;
  friend class TestSerializer;

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

// SaveContext scopes save the current context on the Isolate on creation, and
// restore it on destruction.
class V8_EXPORT_PRIVATE SaveContext {
 public:
  explicit SaveContext(Isolate* isolate);

  ~SaveContext();

  Handle<Context> context() { return context_; }

  // Returns true if this save context is below a given JavaScript frame.
  bool IsBelowFrame(StandardFrame* frame);

 private:
  Isolate* const isolate_;
  Handle<Context> context_;
  Address c_entry_fp_;
};

// Like SaveContext, but also switches the Context to a new one in the
// constructor.
class V8_EXPORT_PRIVATE SaveAndSwitchContext : public SaveContext {
 public:
  SaveAndSwitchContext(Isolate* isolate, Context new_context);
};

// A scope which sets the given isolate's context to null for its lifetime to
// ensure that code does not make assumptions on a context being available.
class NullContextScope : public SaveAndSwitchContext {
 public:
  explicit NullContextScope(Isolate* isolate)
      : SaveAndSwitchContext(isolate, Context()) {}
};

class AssertNoContextChange {
#ifdef DEBUG
 public:
  explicit AssertNoContextChange(Isolate* isolate);
  ~AssertNoContextChange() { DCHECK(isolate_->context() == *context_); }

 private:
  Isolate* isolate_;
  Handle<Context> context_;
#else
 public:
  explicit AssertNoContextChange(Isolate* isolate) {}
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
  explicit StackLimitCheck(Isolate* isolate) : isolate_(isolate) {}

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

#endif  // V8_EXECUTION_ISOLATE_H_
