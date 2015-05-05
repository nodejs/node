// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_ISOLATE_H_
#define V8_ISOLATE_H_

#include <queue>
#include "include/v8-debug.h"
#include "src/allocation.h"
#include "src/assert-scope.h"
#include "src/base/atomicops.h"
#include "src/builtins.h"
#include "src/contexts.h"
#include "src/date.h"
#include "src/execution.h"
#include "src/frames.h"
#include "src/global-handles.h"
#include "src/handles.h"
#include "src/hashmap.h"
#include "src/heap/heap.h"
#include "src/optimizing-compiler-thread.h"
#include "src/regexp-stack.h"
#include "src/runtime/runtime.h"
#include "src/runtime-profiler.h"
#include "src/zone.h"

namespace v8 {

namespace base {
class RandomNumberGenerator;
}

namespace internal {

class BasicBlockProfiler;
class Bootstrapper;
class CallInterfaceDescriptorData;
class CodeGenerator;
class CodeRange;
class CodeStubDescriptor;
class CodeTracer;
class CompilationCache;
class CompilationStatistics;
class ContextSlotCache;
class Counters;
class CpuFeatures;
class CpuProfiler;
class DeoptimizerData;
class Deserializer;
class EmptyStatement;
class ExternalCallbackScope;
class ExternalReferenceTable;
class Factory;
class FunctionInfoListener;
class HandleScopeImplementer;
class HeapProfiler;
class HStatistics;
class HTracer;
class InlineRuntimeFunctionsTable;
class InnerPointerToCodeCache;
class MaterializedObjectStore;
class CodeAgingHelper;
class RegExpStack;
class SaveContext;
class StringTracker;
class StubCache;
class SweeperThread;
class ThreadManager;
class ThreadState;
class ThreadVisitor;  // Defined in v8threads.h
class UnicodeCache;
template <StateTag Tag> class VMState;

// 'void function pointer', used to roundtrip the
// ExternalReference::ExternalReferenceRedirector since we can not include
// assembler.h, where it is defined, here.
typedef void* ExternalReferenceRedirectorPointer();


class Debug;
class Debugger;
class PromiseOnStack;

#if !defined(__arm__) && V8_TARGET_ARCH_ARM ||       \
    !defined(__aarch64__) && V8_TARGET_ARCH_ARM64 || \
    !defined(__PPC__) && V8_TARGET_ARCH_PPC ||       \
    !defined(__mips__) && V8_TARGET_ARCH_MIPS ||     \
    !defined(__mips__) && V8_TARGET_ARCH_MIPS64
class Redirection;
class Simulator;
#endif


// Static indirection table for handles to constants.  If a frame
// element represents a constant, the data contains an index into
// this table of handles to the actual constants.
// Static indirection table for handles to constants.  If a Result
// represents a constant, the data contains an index into this table
// of handles to the actual constants.
typedef ZoneList<Handle<Object> > ZoneObjectList;

#define RETURN_FAILURE_IF_SCHEDULED_EXCEPTION(isolate)    \
  do {                                                    \
    Isolate* __isolate__ = (isolate);                     \
    if (__isolate__->has_scheduled_exception()) {         \
      return __isolate__->PromoteScheduledException();    \
    }                                                     \
  } while (false)

// Macros for MaybeHandle.

#define RETURN_VALUE_IF_SCHEDULED_EXCEPTION(isolate, value) \
  do {                                                      \
    Isolate* __isolate__ = (isolate);                       \
    if (__isolate__->has_scheduled_exception()) {           \
      __isolate__->PromoteScheduledException();             \
      return value;                                         \
    }                                                       \
  } while (false)

#define RETURN_EXCEPTION_IF_SCHEDULED_EXCEPTION(isolate, T) \
  RETURN_VALUE_IF_SCHEDULED_EXCEPTION(isolate, MaybeHandle<T>())

#define ASSIGN_RETURN_ON_EXCEPTION_VALUE(isolate, dst, call, value)  \
  do {                                                               \
    if (!(call).ToHandle(&dst)) {                                    \
      DCHECK((isolate)->has_pending_exception());                    \
      return value;                                                  \
    }                                                                \
  } while (false)

#define ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, dst, call)  \
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(                             \
      isolate, dst, call, isolate->heap()->exception())

#define ASSIGN_RETURN_ON_EXCEPTION(isolate, dst, call, T)  \
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(isolate, dst, call, MaybeHandle<T>())

#define THROW_NEW_ERROR(isolate, call, T)               \
  do {                                                  \
    return isolate->Throw<T>(isolate->factory()->call); \
  } while (false)

#define THROW_NEW_ERROR_RETURN_FAILURE(isolate, call) \
  do {                                                \
    return isolate->Throw(*isolate->factory()->call); \
  } while (false)

#define RETURN_ON_EXCEPTION_VALUE(isolate, call, value)            \
  do {                                                             \
    if ((call).is_null()) {                                        \
      DCHECK((isolate)->has_pending_exception());                  \
      return value;                                                \
    }                                                              \
  } while (false)

#define RETURN_FAILURE_ON_EXCEPTION(isolate, call)  \
  RETURN_ON_EXCEPTION_VALUE(isolate, call, isolate->heap()->exception())

#define RETURN_ON_EXCEPTION(isolate, call, T)  \
  RETURN_ON_EXCEPTION_VALUE(isolate, call, MaybeHandle<T>())


#define FOR_EACH_ISOLATE_ADDRESS_NAME(C)                \
  C(Handler, handler)                                   \
  C(CEntryFP, c_entry_fp)                               \
  C(CFunction, c_function)                              \
  C(Context, context)                                   \
  C(PendingException, pending_exception)                \
  C(PendingHandlerContext, pending_handler_context)     \
  C(PendingHandlerCode, pending_handler_code)           \
  C(PendingHandlerOffset, pending_handler_offset)       \
  C(PendingHandlerFP, pending_handler_fp)               \
  C(PendingHandlerSP, pending_handler_sp)               \
  C(ExternalCaughtException, external_caught_exception) \
  C(JSEntrySP, js_entry_sp)


// Platform-independent, reliable thread identifier.
class ThreadId {
 public:
  // Creates an invalid ThreadId.
  ThreadId() { base::NoBarrier_Store(&id_, kInvalidId); }

  ThreadId& operator=(const ThreadId& other) {
    base::NoBarrier_Store(&id_, base::NoBarrier_Load(&other.id_));
    return *this;
  }

  // Returns ThreadId for current thread.
  static ThreadId Current() { return ThreadId(GetCurrentThreadId()); }

  // Returns invalid ThreadId (guaranteed not to be equal to any thread).
  static ThreadId Invalid() { return ThreadId(kInvalidId); }

  // Compares ThreadIds for equality.
  INLINE(bool Equals(const ThreadId& other) const) {
    return base::NoBarrier_Load(&id_) == base::NoBarrier_Load(&other.id_);
  }

  // Checks whether this ThreadId refers to any thread.
  INLINE(bool IsValid() const) {
    return base::NoBarrier_Load(&id_) != kInvalidId;
  }

  // Converts ThreadId to an integer representation
  // (required for public API: V8::V8::GetCurrentThreadId).
  int ToInteger() const { return static_cast<int>(base::NoBarrier_Load(&id_)); }

  // Converts ThreadId to an integer representation
  // (required for public API: V8::V8::TerminateExecution).
  static ThreadId FromInteger(int id) { return ThreadId(id); }

 private:
  static const int kInvalidId = -1;

  explicit ThreadId(int id) { base::NoBarrier_Store(&id_, id); }

  static int AllocateThreadId();

  static int GetCurrentThreadId();

  base::Atomic32 id_;

  static base::Atomic32 highest_thread_id_;

  friend class Isolate;
};


#define FIELD_ACCESSOR(type, name)                 \
  inline void set_##name(type v) { name##_ = v; }  \
  inline type name() const { return name##_; }


class ThreadLocalTop BASE_EMBEDDED {
 public:
  // Does early low-level initialization that does not depend on the
  // isolate being present.
  ThreadLocalTop();

  // Initialize the thread data.
  void Initialize();

  // Get the top C++ try catch handler or NULL if none are registered.
  //
  // This method is not guaranteed to return an address that can be
  // used for comparison with addresses into the JS stack.  If such an
  // address is needed, use try_catch_handler_address.
  FIELD_ACCESSOR(v8::TryCatch*, try_catch_handler)

  // Get the address of the top C++ try catch handler or NULL if
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

  Isolate* isolate_;
  // The context where the current execution method is created and for variable
  // lookups.
  Context* context_;
  ThreadId thread_id_;
  Object* pending_exception_;

  // Communication channel between Isolate::FindHandler and the CEntryStub.
  Context* pending_handler_context_;
  Code* pending_handler_code_;
  intptr_t pending_handler_offset_;
  Address pending_handler_fp_;
  Address pending_handler_sp_;

  // Communication channel between Isolate::Throw and message consumers.
  bool rethrowing_message_;
  Object* pending_message_obj_;

  // Use a separate value for scheduled exceptions to preserve the
  // invariants that hold about pending_exception.  We may want to
  // unify them later.
  Object* scheduled_exception_;
  bool external_caught_exception_;
  SaveContext* save_context_;

  // Stack.
  Address c_entry_fp_;  // the frame pointer of the top c entry frame
  Address handler_;     // try-blocks are chained through the stack
  Address c_function_;  // C function that was called at c entry.

  // Throwing an exception may cause a Promise rejection.  For this purpose
  // we keep track of a stack of nested promises and the corresponding
  // try-catch handlers.
  PromiseOnStack* promise_on_stack_;

#ifdef USE_SIMULATOR
  Simulator* simulator_;
#endif

  Address js_entry_sp_;  // the stack pointer of the bottom JS entry frame
  // the external callback we're currently in
  ExternalCallbackScope* external_callback_scope_;
  StateTag current_vm_state_;

  // Call back function to report unsafe JS accesses.
  v8::FailedAccessCheckCallback failed_access_check_callback_;

 private:
  void InitializeInternal();

  v8::TryCatch* try_catch_handler_;
};


#if V8_TARGET_ARCH_ARM && !defined(__arm__) ||       \
    V8_TARGET_ARCH_ARM64 && !defined(__aarch64__) || \
    V8_TARGET_ARCH_PPC && !defined(__PPC__) ||       \
    V8_TARGET_ARCH_MIPS && !defined(__mips__) ||     \
    V8_TARGET_ARCH_MIPS64 && !defined(__mips__)

#define ISOLATE_INIT_SIMULATOR_LIST(V)                                         \
  V(bool, simulator_initialized, false)                                        \
  V(HashMap*, simulator_i_cache, NULL)                                         \
  V(Redirection*, simulator_redirection, NULL)
#else

#define ISOLATE_INIT_SIMULATOR_LIST(V)

#endif


#ifdef DEBUG

#define ISOLATE_INIT_DEBUG_ARRAY_LIST(V)                                       \
  V(CommentStatistic, paged_space_comments_statistics,                         \
      CommentStatistic::kMaxComments + 1)                                      \
  V(int, code_kind_statistics, Code::NUMBER_OF_KINDS)
#else

#define ISOLATE_INIT_DEBUG_ARRAY_LIST(V)

#endif

#define ISOLATE_INIT_ARRAY_LIST(V)                                             \
  /* SerializerDeserializer state. */                                          \
  V(int32_t, jsregexp_static_offsets_vector, kJSRegexpStaticOffsetsVectorSize) \
  V(int, bad_char_shift_table, kUC16AlphabetSize)                              \
  V(int, good_suffix_shift_table, (kBMMaxShift + 1))                           \
  V(int, suffix_table, (kBMMaxShift + 1))                                      \
  V(uint32_t, private_random_seed, 2)                                          \
  ISOLATE_INIT_DEBUG_ARRAY_LIST(V)

typedef List<HeapObject*> DebugObjectCache;

#define ISOLATE_INIT_LIST(V)                                                   \
  /* Assembler state. */                                                       \
  V(FatalErrorCallback, exception_behavior, NULL)                              \
  V(LogEventCallback, event_logger, NULL)                                      \
  V(AllowCodeGenerationFromStringsCallback, allow_code_gen_callback, NULL)     \
  /* To distinguish the function templates, so that we can find them in the */ \
  /* function cache of the native context. */                                  \
  V(int, next_serial_number, 0)                                                \
  V(ExternalReferenceRedirectorPointer*, external_reference_redirector, NULL)  \
  /* Part of the state of liveedit. */                                         \
  V(FunctionInfoListener*, active_function_info_listener, NULL)                \
  /* State for Relocatable. */                                                 \
  V(Relocatable*, relocatable_top, NULL)                                       \
  V(DebugObjectCache*, string_stream_debug_object_cache, NULL)                 \
  V(Object*, string_stream_current_security_token, NULL)                       \
  V(ExternalReferenceTable*, external_reference_table, NULL)                   \
  V(HashMap*, external_reference_map, NULL)                                    \
  V(HashMap*, root_index_map, NULL)                                            \
  V(int, pending_microtask_count, 0)                                           \
  V(bool, autorun_microtasks, true)                                            \
  V(HStatistics*, hstatistics, NULL)                                           \
  V(CompilationStatistics*, turbo_statistics, NULL)                            \
  V(HTracer*, htracer, NULL)                                                   \
  V(CodeTracer*, code_tracer, NULL)                                            \
  V(bool, fp_stubs_generated, false)                                           \
  V(int, max_available_threads, 0)                                             \
  V(uint32_t, per_isolate_assert_data, 0xFFFFFFFFu)                            \
  V(PromiseRejectCallback, promise_reject_callback, NULL)                      \
  V(const v8::StartupData*, snapshot_blob, NULL)                               \
  ISOLATE_INIT_SIMULATOR_LIST(V)

#define THREAD_LOCAL_TOP_ACCESSOR(type, name)                        \
  inline void set_##name(type v) { thread_local_top_.name##_ = v; }  \
  inline type name() const { return thread_local_top_.name##_; }

#define THREAD_LOCAL_TOP_ADDRESS(type, name) \
  type* name##_address() { return &thread_local_top_.name##_; }


class Isolate {
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
          thread_state_(NULL),
#if !defined(__arm__) && V8_TARGET_ARCH_ARM ||       \
    !defined(__aarch64__) && V8_TARGET_ARCH_ARM64 || \
    !defined(__PPC__) && V8_TARGET_ARCH_PPC ||       \
    !defined(__mips__) && V8_TARGET_ARCH_MIPS ||     \
    !defined(__mips__) && V8_TARGET_ARCH_MIPS64
          simulator_(NULL),
#endif
          next_(NULL),
          prev_(NULL) { }
    ~PerIsolateThreadData();
    Isolate* isolate() const { return isolate_; }
    ThreadId thread_id() const { return thread_id_; }

    FIELD_ACCESSOR(uintptr_t, stack_limit)
    FIELD_ACCESSOR(ThreadState*, thread_state)

#if !defined(__arm__) && V8_TARGET_ARCH_ARM ||       \
    !defined(__aarch64__) && V8_TARGET_ARCH_ARM64 || \
    !defined(__PPC__) && V8_TARGET_ARCH_PPC ||       \
    !defined(__mips__) && V8_TARGET_ARCH_MIPS ||     \
    !defined(__mips__) && V8_TARGET_ARCH_MIPS64
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

#if !defined(__arm__) && V8_TARGET_ARCH_ARM ||       \
    !defined(__aarch64__) && V8_TARGET_ARCH_ARM64 || \
    !defined(__PPC__) && V8_TARGET_ARCH_PPC ||       \
    !defined(__mips__) && V8_TARGET_ARCH_MIPS ||     \
    !defined(__mips__) && V8_TARGET_ARCH_MIPS64
    Simulator* simulator_;
#endif

    PerIsolateThreadData* next_;
    PerIsolateThreadData* prev_;

    friend class Isolate;
    friend class ThreadDataTable;
    friend class EntryStackItem;

    DISALLOW_COPY_AND_ASSIGN(PerIsolateThreadData);
  };


  enum AddressId {
#define DECLARE_ENUM(CamelName, hacker_name) k##CamelName##Address,
    FOR_EACH_ISOLATE_ADDRESS_NAME(DECLARE_ENUM)
#undef DECLARE_ENUM
    kIsolateAddressCount
  };

  static void InitializeOncePerProcess();

  // Returns the PerIsolateThreadData for the current thread (or NULL if one is
  // not currently set).
  static PerIsolateThreadData* CurrentPerIsolateThreadData() {
    return reinterpret_cast<PerIsolateThreadData*>(
        base::Thread::GetThreadLocal(per_isolate_thread_data_key_));
  }

  // Returns the isolate inside which the current thread is running.
  INLINE(static Isolate* Current()) {
    DCHECK(base::NoBarrier_Load(&isolate_key_created_) == 1);
    Isolate* isolate = reinterpret_cast<Isolate*>(
        base::Thread::GetExistingThreadLocal(isolate_key_));
    DCHECK(isolate != NULL);
    return isolate;
  }

  INLINE(static Isolate* UncheckedCurrent()) {
    DCHECK(base::NoBarrier_Load(&isolate_key_created_) == 1);
    return reinterpret_cast<Isolate*>(
        base::Thread::GetThreadLocal(isolate_key_));
  }

  // Like UncheckedCurrent, but skips the check that |isolate_key_| was
  // initialized. Callers have to ensure that themselves.
  INLINE(static Isolate* UnsafeCurrent()) {
    return reinterpret_cast<Isolate*>(
        base::Thread::GetThreadLocal(isolate_key_));
  }

  // Usually called by Init(), but can be called early e.g. to allow
  // testing components that require logging but not the whole
  // isolate.
  //
  // Safe to call more than once.
  void InitializeLoggingAndCounters();

  bool Init(Deserializer* des);

  // True if at least one thread Enter'ed this isolate.
  bool IsInUse() { return entry_stack_ != NULL; }

  // Destroys the non-default isolates.
  // Sets default isolate into "has_been_disposed" state rather then destroying,
  // for legacy API reasons.
  void TearDown();

  static void GlobalTearDown();

  void ClearSerializerData();

  // Find the PerThread for this particular (isolate, thread) combination
  // If one does not yet exist, return null.
  PerIsolateThreadData* FindPerThreadDataForThisThread();

  // Find the PerThread for given (isolate, thread) combination
  // If one does not yet exist, return null.
  PerIsolateThreadData* FindPerThreadDataForThread(ThreadId thread_id);

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

  Address get_address_from_id(AddressId id);

  // Access to top context (where the current function object was created).
  Context* context() { return thread_local_top_.context_; }
  void set_context(Context* context) {
    DCHECK(context == NULL || context->IsContext());
    thread_local_top_.context_ = context;
  }
  Context** context_address() { return &thread_local_top_.context_; }

  THREAD_LOCAL_TOP_ACCESSOR(SaveContext*, save_context)

  // Access to current thread id.
  THREAD_LOCAL_TOP_ACCESSOR(ThreadId, thread_id)

  // Interface to pending exception.
  Object* pending_exception() {
    DCHECK(has_pending_exception());
    DCHECK(!thread_local_top_.pending_exception_->IsException());
    return thread_local_top_.pending_exception_;
  }

  void set_pending_exception(Object* exception_obj) {
    DCHECK(!exception_obj->IsException());
    thread_local_top_.pending_exception_ = exception_obj;
  }

  void clear_pending_exception() {
    DCHECK(!thread_local_top_.pending_exception_->IsException());
    thread_local_top_.pending_exception_ = heap_.the_hole_value();
  }

  THREAD_LOCAL_TOP_ADDRESS(Object*, pending_exception)

  bool has_pending_exception() {
    DCHECK(!thread_local_top_.pending_exception_->IsException());
    return !thread_local_top_.pending_exception_->IsTheHole();
  }

  THREAD_LOCAL_TOP_ADDRESS(Context*, pending_handler_context)
  THREAD_LOCAL_TOP_ADDRESS(Code*, pending_handler_code)
  THREAD_LOCAL_TOP_ADDRESS(intptr_t, pending_handler_offset)
  THREAD_LOCAL_TOP_ADDRESS(Address, pending_handler_fp)
  THREAD_LOCAL_TOP_ADDRESS(Address, pending_handler_sp)

  THREAD_LOCAL_TOP_ACCESSOR(bool, external_caught_exception)

  void clear_pending_message() {
    thread_local_top_.pending_message_obj_ = heap_.the_hole_value();
  }
  v8::TryCatch* try_catch_handler() {
    return thread_local_top_.try_catch_handler();
  }
  bool* external_caught_exception_address() {
    return &thread_local_top_.external_caught_exception_;
  }

  THREAD_LOCAL_TOP_ADDRESS(Object*, scheduled_exception)

  Address pending_message_obj_address() {
    return reinterpret_cast<Address>(&thread_local_top_.pending_message_obj_);
  }

  Object* scheduled_exception() {
    DCHECK(has_scheduled_exception());
    DCHECK(!thread_local_top_.scheduled_exception_->IsException());
    return thread_local_top_.scheduled_exception_;
  }
  bool has_scheduled_exception() {
    DCHECK(!thread_local_top_.scheduled_exception_->IsException());
    return thread_local_top_.scheduled_exception_ != heap_.the_hole_value();
  }
  void clear_scheduled_exception() {
    DCHECK(!thread_local_top_.scheduled_exception_->IsException());
    thread_local_top_.scheduled_exception_ = heap_.the_hole_value();
  }

  bool IsJavaScriptHandlerOnTop(Object* exception);
  bool IsExternalHandlerOnTop(Object* exception);

  bool is_catchable_by_javascript(Object* exception) {
    return exception != heap()->termination_exception();
  }

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
  Handle<GlobalObject> global_object() {
    return Handle<GlobalObject>(context()->global_object());
  }

  // Returns the global proxy object of the current context.
  JSObject* global_proxy() {
    return context()->global_proxy();
  }

  Handle<JSBuiltinsObject> js_builtins_object() {
    return Handle<JSBuiltinsObject>(thread_local_top_.context_->builtins());
  }

  static int ArchiveSpacePerThread() { return sizeof(ThreadLocalTop); }
  void FreeThreadResources() { thread_local_top_.Free(); }

  // This method is called by the api after operations that may throw
  // exceptions.  If an exception was thrown and not handled by an external
  // handler the exception is scheduled to be rethrown when we return to running
  // JavaScript code.  If an exception is scheduled true is returned.
  bool OptionalRescheduleException(bool is_bottom_call);

  // Push and pop a promise and the current try-catch handler.
  void PushPromise(Handle<JSObject> promise, Handle<JSFunction> function);
  void PopPromise();
  Handle<Object> GetPromiseOnStackOnThrow();

  class ExceptionScope {
   public:
    // Scope currently can only be used for regular exceptions,
    // not termination exception.
    explicit ExceptionScope(Isolate* isolate)
        : isolate_(isolate),
          pending_exception_(isolate_->pending_exception(), isolate_) {}

    ~ExceptionScope() {
      isolate_->set_pending_exception(*pending_exception_);
    }

   private:
    Isolate* isolate_;
    Handle<Object> pending_exception_;
  };

  void SetCaptureStackTraceForUncaughtExceptions(
      bool capture,
      int frame_limit,
      StackTrace::StackTraceOptions options);

  void PrintCurrentStackTrace(FILE* out);
  void PrintStack(StringStream* accumulator);
  void PrintStack(FILE* out);
  Handle<String> StackTraceString();
  NO_INLINE(void PushStackTraceAndDie(unsigned int magic,
                                      Object* object,
                                      Map* map,
                                      unsigned int magic2));
  Handle<JSArray> CaptureCurrentStackTrace(
      int frame_limit,
      StackTrace::StackTraceOptions options);
  Handle<Object> CaptureSimpleStackTrace(Handle<JSObject> error_object,
                                         Handle<Object> caller);
  void CaptureAndSetDetailedStackTrace(Handle<JSObject> error_object);
  void CaptureAndSetSimpleStackTrace(Handle<JSObject> error_object,
                                     Handle<Object> caller);
  Handle<JSArray> GetDetailedStackTrace(Handle<JSObject> error_object);
  Handle<JSArray> GetDetailedFromSimpleStackTrace(
      Handle<JSObject> error_object);

  // Returns if the top context may access the given global object. If
  // the result is false, the pending exception is guaranteed to be
  // set.

  bool MayAccess(Handle<JSObject> receiver);
  bool IsInternallyUsedPropertyName(Handle<Object> name);
  bool IsInternallyUsedPropertyName(Object* name);

  void SetFailedAccessCheckCallback(v8::FailedAccessCheckCallback callback);
  void ReportFailedAccessCheck(Handle<JSObject> receiver);

  // Exception throwing support. The caller should use the result
  // of Throw() as its return value.
  Object* Throw(Object* exception, MessageLocation* location = NULL);
  Object* ThrowIllegalOperation();

  template <typename T>
  MUST_USE_RESULT MaybeHandle<T> Throw(Handle<Object> exception,
                                       MessageLocation* location = NULL) {
    Throw(*exception, location);
    return MaybeHandle<T>();
  }

  // Re-throw an exception.  This involves no error reporting since error
  // reporting was handled when the exception was thrown originally.
  Object* ReThrow(Object* exception);

  // Find the correct handler for the current pending exception. This also
  // clears and returns the current pending exception.
  Object* FindHandler();

  // Tries to predict whether an exception will be caught. Note that this can
  // only produce an estimate, because it is undecidable whether a finally
  // clause will consume or re-throw an exception. We conservatively assume any
  // finally clause will behave as if the exception were consumed.
  enum CatchType { NOT_CAUGHT, CAUGHT_BY_JAVASCRIPT, CAUGHT_BY_EXTERNAL };
  CatchType PredictExceptionCatcher();

  void ScheduleThrow(Object* exception);
  // Re-set pending message, script and positions reported to the TryCatch
  // back to the TLS for re-use when rethrowing.
  void RestorePendingMessageFromTryCatch(v8::TryCatch* handler);
  // Un-schedule an exception that was caught by a TryCatch handler.
  void CancelScheduledExceptionFromTryCatch(v8::TryCatch* handler);
  void ReportPendingMessages();
  // Return pending location if any or unfilled structure.
  MessageLocation GetMessageLocation();

  // Promote a scheduled exception to pending. Asserts has_scheduled_exception.
  Object* PromoteScheduledException();

  // Attempts to compute the current source location, storing the
  // result in the target out parameter.
  void ComputeLocation(MessageLocation* target);
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
  void Iterate(ObjectVisitor* v);
  void Iterate(ObjectVisitor* v, ThreadLocalTop* t);
  char* Iterate(ObjectVisitor* v, char* t);
  void IterateThread(ThreadVisitor* v, char* t);

  // Returns the current native context.
  Handle<Context> native_context();

  // Returns the native context of the calling JavaScript code.  That
  // is, the native context of the top-most JavaScript frame.
  Handle<Context> GetCallingNativeContext();

  void RegisterTryCatchHandler(v8::TryCatch* that);
  void UnregisterTryCatchHandler(v8::TryCatch* that);

  char* ArchiveThread(char* to);
  char* RestoreThread(char* from);

  static const char* const kStackOverflowMessage;

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

#define NATIVE_CONTEXT_FIELD_ACCESSOR(index, type, name)            \
  Handle<type> name() {                                             \
    return Handle<type>(native_context()->name(), this);            \
  }                                                                 \
  bool is_##name(type* value) {                                     \
    return native_context()->is_##name(value);                      \
  }
  NATIVE_CONTEXT_FIELDS(NATIVE_CONTEXT_FIELD_ACCESSOR)
#undef NATIVE_CONTEXT_FIELD_ACCESSOR

  Bootstrapper* bootstrapper() { return bootstrapper_; }
  Counters* counters() {
    // Call InitializeLoggingAndCounters() if logging is needed before
    // the isolate is fully initialized.
    DCHECK(counters_ != NULL);
    return counters_;
  }
  CodeRange* code_range() { return code_range_; }
  RuntimeProfiler* runtime_profiler() { return runtime_profiler_; }
  CompilationCache* compilation_cache() { return compilation_cache_; }
  Logger* logger() {
    // Call InitializeLoggingAndCounters() if logging is needed before
    // the isolate is fully initialized.
    DCHECK(logger_ != NULL);
    return logger_;
  }
  StackGuard* stack_guard() { return &stack_guard_; }
  Heap* heap() { return &heap_; }
  StatsTable* stats_table();
  StubCache* stub_cache() { return stub_cache_; }
  CodeAgingHelper* code_aging_helper() { return code_aging_helper_; }
  DeoptimizerData* deoptimizer_data() { return deoptimizer_data_; }
  ThreadLocalTop* thread_local_top() { return &thread_local_top_; }
  MaterializedObjectStore* materialized_object_store() {
    return materialized_object_store_;
  }

  MemoryAllocator* memory_allocator() {
    return memory_allocator_;
  }

  KeyedLookupCache* keyed_lookup_cache() {
    return keyed_lookup_cache_;
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
  Zone* runtime_zone() { return &runtime_zone_; }

  UnicodeCache* unicode_cache() {
    return unicode_cache_;
  }

  InnerPointerToCodeCache* inner_pointer_to_code_cache() {
    return inner_pointer_to_code_cache_;
  }

  GlobalHandles* global_handles() { return global_handles_; }

  EternalHandles* eternal_handles() { return eternal_handles_; }

  ThreadManager* thread_manager() { return thread_manager_; }

  StringTracker* string_tracker() { return string_tracker_; }

  unibrow::Mapping<unibrow::Ecma262UnCanonicalize>* jsregexp_uncanonicalize() {
    return &jsregexp_uncanonicalize_;
  }

  unibrow::Mapping<unibrow::CanonicalizationRange>* jsregexp_canonrange() {
    return &jsregexp_canonrange_;
  }

  RuntimeState* runtime_state() { return &runtime_state_; }

  Builtins* builtins() { return &builtins_; }

  void NotifyExtensionInstalled() {
    has_installed_extensions_ = true;
  }

  bool has_installed_extensions() { return has_installed_extensions_; }

  unibrow::Mapping<unibrow::Ecma262Canonicalize>*
      regexp_macro_assembler_canonicalize() {
    return &regexp_macro_assembler_canonicalize_;
  }

  RegExpStack* regexp_stack() { return regexp_stack_; }

  unibrow::Mapping<unibrow::Ecma262Canonicalize>*
      interp_canonicalize_mapping() {
    return &interp_canonicalize_mapping_;
  }

  Debug* debug() { return debug_; }

  CpuProfiler* cpu_profiler() const { return cpu_profiler_; }
  HeapProfiler* heap_profiler() const { return heap_profiler_; }

#ifdef DEBUG
  HistogramInfo* heap_histograms() { return heap_histograms_; }

  JSObject::SpillInformation* js_spill_information() {
    return &js_spill_information_;
  }
#endif

  Factory* factory() { return reinterpret_cast<Factory*>(this); }

  static const int kJSRegexpStaticOffsetsVectorSize = 128;

  THREAD_LOCAL_TOP_ACCESSOR(ExternalCallbackScope*, external_callback_scope)

  THREAD_LOCAL_TOP_ACCESSOR(StateTag, current_vm_state)

  void SetData(uint32_t slot, void* data) {
    DCHECK(slot < Internals::kNumIsolateDataSlots);
    embedder_data_[slot] = data;
  }
  void* GetData(uint32_t slot) {
    DCHECK(slot < Internals::kNumIsolateDataSlots);
    return embedder_data_[slot];
  }

  bool serializer_enabled() const { return serializer_enabled_; }
  bool snapshot_available() const { return snapshot_blob_ != NULL; }

  bool IsDead() { return has_fatal_error_; }
  void SignalFatalError() { has_fatal_error_ = true; }

  bool use_crankshaft() const;

  bool initialized_from_snapshot() { return initialized_from_snapshot_; }

  double time_millis_since_init() {
    return base::OS::TimeCurrentMillis() - time_millis_at_init_;
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

  Map* get_initial_js_array_map(ElementsKind kind);

  bool IsFastArrayConstructorPrototypeChainIntact();

  CallInterfaceDescriptorData* call_descriptor_data(int index);

  void IterateDeferredHandles(ObjectVisitor* visitor);
  void LinkDeferredHandles(DeferredHandles* deferred_handles);
  void UnlinkDeferredHandles(DeferredHandles* deferred_handles);

#ifdef DEBUG
  bool IsDeferredHandle(Object** location);
#endif  // DEBUG

  bool concurrent_recompilation_enabled() {
    // Thread is only available with flag enabled.
    DCHECK(optimizing_compiler_thread_ == NULL ||
           FLAG_concurrent_recompilation);
    return optimizing_compiler_thread_ != NULL;
  }

  bool concurrent_osr_enabled() const {
    // Thread is only available with flag enabled.
    DCHECK(optimizing_compiler_thread_ == NULL ||
           FLAG_concurrent_recompilation);
    return optimizing_compiler_thread_ != NULL && FLAG_concurrent_osr;
  }

  OptimizingCompilerThread* optimizing_compiler_thread() {
    return optimizing_compiler_thread_;
  }

  int id() const { return static_cast<int>(id_); }

  HStatistics* GetHStatistics();
  CompilationStatistics* GetTurboStatistics();
  HTracer* GetHTracer();
  CodeTracer* GetCodeTracer();

  void DumpAndResetCompilationStats();

  FunctionEntryHook function_entry_hook() { return function_entry_hook_; }
  void set_function_entry_hook(FunctionEntryHook function_entry_hook) {
    function_entry_hook_ = function_entry_hook;
  }

  void* stress_deopt_count_address() { return &stress_deopt_count_; }

  inline base::RandomNumberGenerator* random_number_generator();

  // Given an address occupied by a live code object, return that object.
  Object* FindCodeObject(Address a);

  int NextOptimizationId() {
    int id = next_optimization_id_++;
    if (!Smi::IsValid(next_optimization_id_)) {
      next_optimization_id_ = 0;
    }
    return id;
  }

  // Get (and lazily initialize) the registry for per-isolate symbols.
  Handle<JSObject> GetSymbolRegistry();

  void AddCallCompletedCallback(CallCompletedCallback callback);
  void RemoveCallCompletedCallback(CallCompletedCallback callback);
  void FireCallCompletedCallback();

  void SetPromiseRejectCallback(PromiseRejectCallback callback);
  void ReportPromiseReject(Handle<JSObject> promise, Handle<Object> value,
                           v8::PromiseRejectEvent event);

  void EnqueueMicrotask(Handle<Object> microtask);
  void RunMicrotasks();

  void SetUseCounterCallback(v8::Isolate::UseCounterCallback callback);
  void CountUsage(v8::Isolate::UseCounterFeature feature);

  BasicBlockProfiler* GetOrCreateBasicBlockProfiler();
  BasicBlockProfiler* basic_block_profiler() { return basic_block_profiler_; }

  static Isolate* NewForTesting() { return new Isolate(false); }

  std::string GetTurboCfgFileName();

#if TRACE_MAPS
  int GetNextUniqueSharedFunctionInfoId() { return next_unique_sfi_id_++; }
#endif

  void set_store_buffer_hash_set_1_address(
      uintptr_t* store_buffer_hash_set_1_address) {
    store_buffer_hash_set_1_address_ = store_buffer_hash_set_1_address;
  }

  uintptr_t* store_buffer_hash_set_1_address() {
    return store_buffer_hash_set_1_address_;
  }

  void set_store_buffer_hash_set_2_address(
      uintptr_t* store_buffer_hash_set_2_address) {
    store_buffer_hash_set_2_address_ = store_buffer_hash_set_2_address;
  }

  uintptr_t* store_buffer_hash_set_2_address() {
    return store_buffer_hash_set_2_address_;
  }

  void AddDetachedContext(Handle<Context> context);
  void CheckDetachedContextsAfterGC();

  List<Object*>* partial_snapshot_cache() { return &partial_snapshot_cache_; }

 protected:
  explicit Isolate(bool enable_serializer);

 private:
  friend struct GlobalState;
  friend struct InitializeGlobalState;

  // These fields are accessed through the API, offsets must be kept in sync
  // with v8::internal::Internals (in include/v8.h) constants. This is also
  // verified in Isolate::Init() using runtime checks.
  void* embedder_data_[Internals::kNumIsolateDataSlots];
  Heap heap_;

  // The per-process lock should be acquired before the ThreadDataTable is
  // modified.
  class ThreadDataTable {
   public:
    ThreadDataTable();
    ~ThreadDataTable();

    PerIsolateThreadData* Lookup(Isolate* isolate, ThreadId thread_id);
    void Insert(PerIsolateThreadData* data);
    void Remove(PerIsolateThreadData* data);
    void RemoveAllThreads(Isolate* isolate);

   private:
    PerIsolateThreadData* list_;
  };

  // These items form a stack synchronously with threads Enter'ing and Exit'ing
  // the Isolate. The top of the stack points to a thread which is currently
  // running the Isolate. When the stack is empty, the Isolate is considered
  // not entered by any thread and can be Disposed.
  // If the same thread enters the Isolate more then once, the entry_count_
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

  static base::LazyMutex thread_data_table_mutex_;

  static base::Thread::LocalStorageKey per_isolate_thread_data_key_;
  static base::Thread::LocalStorageKey isolate_key_;
  static base::Thread::LocalStorageKey thread_id_key_;
  static ThreadDataTable* thread_data_table_;

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

  // Traverse prototype chain to find out whether the object is derived from
  // the Error object.
  bool IsErrorObject(Handle<Object> obj);

  base::Atomic32 id_;
  EntryStackItem* entry_stack_;
  int stack_trace_nesting_level_;
  StringStream* incomplete_message_;
  Address isolate_addresses_[kIsolateAddressCount + 1];  // NOLINT
  Bootstrapper* bootstrapper_;
  RuntimeProfiler* runtime_profiler_;
  CompilationCache* compilation_cache_;
  Counters* counters_;
  CodeRange* code_range_;
  base::RecursiveMutex break_access_;
  Logger* logger_;
  StackGuard stack_guard_;
  StatsTable* stats_table_;
  StubCache* stub_cache_;
  CodeAgingHelper* code_aging_helper_;
  DeoptimizerData* deoptimizer_data_;
  MaterializedObjectStore* materialized_object_store_;
  ThreadLocalTop thread_local_top_;
  bool capture_stack_trace_for_uncaught_exceptions_;
  int stack_trace_for_uncaught_exceptions_frame_limit_;
  StackTrace::StackTraceOptions stack_trace_for_uncaught_exceptions_options_;
  MemoryAllocator* memory_allocator_;
  KeyedLookupCache* keyed_lookup_cache_;
  ContextSlotCache* context_slot_cache_;
  DescriptorLookupCache* descriptor_lookup_cache_;
  HandleScopeData handle_scope_data_;
  HandleScopeImplementer* handle_scope_implementer_;
  UnicodeCache* unicode_cache_;
  Zone runtime_zone_;
  InnerPointerToCodeCache* inner_pointer_to_code_cache_;
  GlobalHandles* global_handles_;
  EternalHandles* eternal_handles_;
  ThreadManager* thread_manager_;
  RuntimeState runtime_state_;
  Builtins builtins_;
  bool has_installed_extensions_;
  StringTracker* string_tracker_;
  unibrow::Mapping<unibrow::Ecma262UnCanonicalize> jsregexp_uncanonicalize_;
  unibrow::Mapping<unibrow::CanonicalizationRange> jsregexp_canonrange_;
  unibrow::Mapping<unibrow::Ecma262Canonicalize>
      regexp_macro_assembler_canonicalize_;
  RegExpStack* regexp_stack_;
  DateCache* date_cache_;
  unibrow::Mapping<unibrow::Ecma262Canonicalize> interp_canonicalize_mapping_;
  CallInterfaceDescriptorData* call_descriptor_data_;
  base::RandomNumberGenerator* random_number_generator_;
  // TODO(hpayer): Remove the following store buffer addresses.
  uintptr_t* store_buffer_hash_set_1_address_;
  uintptr_t* store_buffer_hash_set_2_address_;

  // Whether the isolate has been created for snapshotting.
  bool serializer_enabled_;

  // True if fatal error has been signaled for this isolate.
  bool has_fatal_error_;

  // True if this isolate was initialized from a snapshot.
  bool initialized_from_snapshot_;

  // Time stamp at initialization.
  double time_millis_at_init_;

#ifdef DEBUG
  // A static array of histogram info for each type.
  HistogramInfo heap_histograms_[LAST_TYPE + 1];
  JSObject::SpillInformation js_spill_information_;
#endif

  Debug* debug_;
  CpuProfiler* cpu_profiler_;
  HeapProfiler* heap_profiler_;
  FunctionEntryHook function_entry_hook_;

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
  OptimizingCompilerThread* optimizing_compiler_thread_;

  // Counts deopt points if deopt_every_n_times is enabled.
  unsigned int stress_deopt_count_;

  int next_optimization_id_;

#if TRACE_MAPS
  int next_unique_sfi_id_;
#endif

  // List of callbacks when a Call completes.
  List<CallCompletedCallback> call_completed_callbacks_;

  v8::Isolate::UseCounterCallback use_counter_callback_;
  BasicBlockProfiler* basic_block_profiler_;

  List<Object*> partial_snapshot_cache_;

  friend class ExecutionAccess;
  friend class HandleScopeImplementer;
  friend class OptimizingCompilerThread;
  friend class SweeperThread;
  friend class ThreadManager;
  friend class Simulator;
  friend class StackGuard;
  friend class ThreadId;
  friend class TestMemoryAllocatorScope;
  friend class TestCodeRangeScope;
  friend class v8::Isolate;
  friend class v8::Locker;
  friend class v8::Unlocker;
  friend v8::StartupData v8::V8::CreateSnapshotDataBlob(const char*);

  DISALLOW_COPY_AND_ASSIGN(Isolate);
};


#undef FIELD_ACCESSOR
#undef THREAD_LOCAL_TOP_ACCESSOR


class PromiseOnStack {
 public:
  PromiseOnStack(Handle<JSFunction> function, Handle<JSObject> promise,
                 PromiseOnStack* prev)
      : function_(function), promise_(promise), prev_(prev) {}
  Handle<JSFunction> function() { return function_; }
  Handle<JSObject> promise() { return promise_; }
  PromiseOnStack* prev() { return prev_; }

 private:
  Handle<JSFunction> function_;
  Handle<JSObject> promise_;
  PromiseOnStack* prev_;
};


// If the GCC version is 4.1.x or 4.2.x an additional field is added to the
// class as a work around for a bug in the generated code found with these
// versions of GCC. See V8 issue 122 for details.
class SaveContext BASE_EMBEDDED {
 public:
  inline explicit SaveContext(Isolate* isolate);

  ~SaveContext() {
    isolate_->set_context(context_.is_null() ? NULL : *context_);
    isolate_->set_save_context(prev_);
  }

  Handle<Context> context() { return context_; }
  SaveContext* prev() { return prev_; }

  // Returns true if this save context is below a given JavaScript frame.
  bool IsBelowFrame(JavaScriptFrame* frame) {
    return (c_entry_fp_ == 0) || (c_entry_fp_ > frame->sp());
  }

 private:
  Isolate* isolate_;
  Handle<Context> context_;
  SaveContext* prev_;
  Address c_entry_fp_;
};


class AssertNoContextChange BASE_EMBEDDED {
#ifdef DEBUG
 public:
  explicit AssertNoContextChange(Isolate* isolate)
    : isolate_(isolate),
      context_(isolate->context(), isolate) { }
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


class ExecutionAccess BASE_EMBEDDED {
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
class StackLimitCheck BASE_EMBEDDED {
 public:
  explicit StackLimitCheck(Isolate* isolate) : isolate_(isolate) { }

  // Use this to check for stack-overflows in C++ code.
  inline bool HasOverflowed() const {
    StackGuard* stack_guard = isolate_->stack_guard();
    return GetCurrentStackPosition() < stack_guard->real_climit();
  }

  // Use this to check for stack-overflow when entering runtime from JS code.
  bool JsHasOverflowed() const;

 private:
  Isolate* isolate_;
};


// Support for temporarily postponing interrupts. When the outermost
// postpone scope is left the interrupts will be re-enabled and any
// interrupts that occurred while in the scope will be taken into
// account.
class PostponeInterruptsScope BASE_EMBEDDED {
 public:
  PostponeInterruptsScope(Isolate* isolate,
                          int intercept_mask = StackGuard::ALL_INTERRUPTS)
      : stack_guard_(isolate->stack_guard()),
        intercept_mask_(intercept_mask),
        intercepted_flags_(0) {
    stack_guard_->PushPostponeInterruptsScope(this);
  }

  ~PostponeInterruptsScope() {
    stack_guard_->PopPostponeInterruptsScope();
  }

  // Find the bottom-most scope that intercepts this interrupt.
  // Return whether the interrupt has been intercepted.
  bool Intercept(StackGuard::InterruptFlag flag);

 private:
  StackGuard* stack_guard_;
  int intercept_mask_;
  int intercepted_flags_;
  PostponeInterruptsScope* prev_;

  friend class StackGuard;
};


class CodeTracer FINAL : public Malloced {
 public:
  explicit CodeTracer(int isolate_id)
      : file_(NULL),
        scope_depth_(0) {
    if (!ShouldRedirect()) {
      file_ = stdout;
      return;
    }

    if (FLAG_redirect_code_traces_to == NULL) {
      SNPrintF(filename_,
               "code-%d-%d.asm",
               base::OS::GetCurrentProcessId(),
               isolate_id);
    } else {
      StrNCpy(filename_, FLAG_redirect_code_traces_to, filename_.length());
    }

    WriteChars(filename_.start(), "", 0, false);
  }

  class Scope {
   public:
    explicit Scope(CodeTracer* tracer) : tracer_(tracer) { tracer->OpenFile(); }
    ~Scope() { tracer_->CloseFile();  }

    FILE* file() const { return tracer_->file(); }

   private:
    CodeTracer* tracer_;
  };

  void OpenFile() {
    if (!ShouldRedirect()) {
      return;
    }

    if (file_ == NULL) {
      file_ = base::OS::FOpen(filename_.start(), "ab");
    }

    scope_depth_++;
  }

  void CloseFile() {
    if (!ShouldRedirect()) {
      return;
    }

    if (--scope_depth_ == 0) {
      fclose(file_);
      file_ = NULL;
    }
  }

  FILE* file() const { return file_; }

 private:
  static bool ShouldRedirect() {
    return FLAG_redirect_code_traces;
  }

  EmbeddedVector<char, 128> filename_;
  FILE* file_;
  int scope_depth_;
};

} }  // namespace v8::internal

#endif  // V8_ISOLATE_H_
