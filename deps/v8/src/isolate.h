// Copyright 2012 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef V8_ISOLATE_H_
#define V8_ISOLATE_H_

#include "../include/v8-debug.h"
#include "allocation.h"
#include "apiutils.h"
#include "atomicops.h"
#include "builtins.h"
#include "contexts.h"
#include "execution.h"
#include "frames.h"
#include "date.h"
#include "global-handles.h"
#include "handles.h"
#include "hashmap.h"
#include "heap.h"
#include "regexp-stack.h"
#include "runtime-profiler.h"
#include "runtime.h"
#include "zone.h"

namespace v8 {
namespace internal {

class Bootstrapper;
class CodeGenerator;
class CodeRange;
class CompilationCache;
class ContextSlotCache;
class ContextSwitcher;
class Counters;
class CpuFeatures;
class CpuProfiler;
class DeoptimizerData;
class Deserializer;
class EmptyStatement;
class ExternalReferenceTable;
class Factory;
class FunctionInfoListener;
class HandleScopeImplementer;
class HeapProfiler;
class InlineRuntimeFunctionsTable;
class NoAllocationStringAllocator;
class InnerPointerToCodeCache;
class PreallocatedMemoryThread;
class RegExpStack;
class SaveContext;
class UnicodeCache;
class StringInputBuffer;
class StringTracker;
class StubCache;
class ThreadManager;
class ThreadState;
class ThreadVisitor;  // Defined in v8threads.h
class VMState;

// 'void function pointer', used to roundtrip the
// ExternalReference::ExternalReferenceRedirector since we can not include
// assembler.h, where it is defined, here.
typedef void* ExternalReferenceRedirectorPointer();


#ifdef ENABLE_DEBUGGER_SUPPORT
class Debug;
class Debugger;
class DebuggerAgent;
#endif

#if !defined(__arm__) && defined(V8_TARGET_ARCH_ARM) || \
    !defined(__mips__) && defined(V8_TARGET_ARCH_MIPS)
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

#define RETURN_IF_SCHEDULED_EXCEPTION(isolate)            \
  do {                                                    \
    Isolate* __isolate__ = (isolate);                     \
    if (__isolate__->has_scheduled_exception()) {         \
      return __isolate__->PromoteScheduledException();    \
    }                                                     \
  } while (false)

#define RETURN_IF_EMPTY_HANDLE_VALUE(isolate, call, value) \
  do {                                                     \
    if ((call).is_null()) {                                \
      ASSERT((isolate)->has_pending_exception());          \
      return (value);                                      \
    }                                                      \
  } while (false)

#define CHECK_NOT_EMPTY_HANDLE(isolate, call)     \
  do {                                            \
    ASSERT(!(isolate)->has_pending_exception());  \
    CHECK(!(call).is_null());                     \
    CHECK(!(isolate)->has_pending_exception());   \
  } while (false)

#define RETURN_IF_EMPTY_HANDLE(isolate, call)                       \
  RETURN_IF_EMPTY_HANDLE_VALUE(isolate, call, Failure::Exception())

#define FOR_EACH_ISOLATE_ADDRESS_NAME(C)                \
  C(Handler, handler)                                   \
  C(CEntryFP, c_entry_fp)                               \
  C(Context, context)                                   \
  C(PendingException, pending_exception)                \
  C(ExternalCaughtException, external_caught_exception) \
  C(JSEntrySP, js_entry_sp)


// Platform-independent, reliable thread identifier.
class ThreadId {
 public:
  // Creates an invalid ThreadId.
  ThreadId() : id_(kInvalidId) {}

  // Returns ThreadId for current thread.
  static ThreadId Current() { return ThreadId(GetCurrentThreadId()); }

  // Returns invalid ThreadId (guaranteed not to be equal to any thread).
  static ThreadId Invalid() { return ThreadId(kInvalidId); }

  // Compares ThreadIds for equality.
  INLINE(bool Equals(const ThreadId& other) const) {
    return id_ == other.id_;
  }

  // Checks whether this ThreadId refers to any thread.
  INLINE(bool IsValid() const) {
    return id_ != kInvalidId;
  }

  // Converts ThreadId to an integer representation
  // (required for public API: V8::V8::GetCurrentThreadId).
  int ToInteger() const { return id_; }

  // Converts ThreadId to an integer representation
  // (required for public API: V8::V8::TerminateExecution).
  static ThreadId FromInteger(int id) { return ThreadId(id); }

 private:
  static const int kInvalidId = -1;

  explicit ThreadId(int id) : id_(id) {}

  static int AllocateThreadId();

  static int GetCurrentThreadId();

  int id_;

  static Atomic32 highest_thread_id_;

  friend class Isolate;
};


class ThreadLocalTop BASE_EMBEDDED {
 public:
  // Does early low-level initialization that does not depend on the
  // isolate being present.
  ThreadLocalTop();

  // Initialize the thread data.
  void Initialize();

  // Get the top C++ try catch handler or NULL if none are registered.
  //
  // This method is not guarenteed to return an address that can be
  // used for comparison with addresses into the JS stack.  If such an
  // address is needed, use try_catch_handler_address.
  v8::TryCatch* TryCatchHandler();

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
  inline Address try_catch_handler_address() {
    return try_catch_handler_address_;
  }

  // Set the address of the top C++ try catch handler.
  inline void set_try_catch_handler_address(Address address) {
    try_catch_handler_address_ = address;
  }

  void Free() {
    ASSERT(!has_pending_message_);
    ASSERT(!external_caught_exception_);
    ASSERT(try_catch_handler_address_ == NULL);
  }

  Isolate* isolate_;
  // The context where the current execution method is created and for variable
  // lookups.
  Context* context_;
  ThreadId thread_id_;
  MaybeObject* pending_exception_;
  bool has_pending_message_;
  Object* pending_message_obj_;
  Script* pending_message_script_;
  int pending_message_start_pos_;
  int pending_message_end_pos_;
  // Use a separate value for scheduled exceptions to preserve the
  // invariants that hold about pending_exception.  We may want to
  // unify them later.
  MaybeObject* scheduled_exception_;
  bool external_caught_exception_;
  SaveContext* save_context_;
  v8::TryCatch* catcher_;

  // Stack.
  Address c_entry_fp_;  // the frame pointer of the top c entry frame
  Address handler_;   // try-blocks are chained through the stack

#ifdef USE_SIMULATOR
#if defined(V8_TARGET_ARCH_ARM) || defined(V8_TARGET_ARCH_MIPS)
  Simulator* simulator_;
#endif
#endif  // USE_SIMULATOR

  Address js_entry_sp_;  // the stack pointer of the bottom JS entry frame
  Address external_callback_;  // the external callback we're currently in
  StateTag current_vm_state_;

  // Generated code scratch locations.
  int32_t formal_count_;

  // Call back function to report unsafe JS accesses.
  v8::FailedAccessCheckCallback failed_access_check_callback_;

  // Head of the list of live LookupResults.
  LookupResult* top_lookup_result_;

  // Whether out of memory exceptions should be ignored.
  bool ignore_out_of_memory_;

 private:
  void InitializeInternal();

  Address try_catch_handler_address_;
};


#ifdef ENABLE_DEBUGGER_SUPPORT

#define ISOLATE_DEBUGGER_INIT_LIST(V)                                          \
  V(v8::Debug::EventCallback, debug_event_callback, NULL)                      \
  V(DebuggerAgent*, debugger_agent_instance, NULL)
#else

#define ISOLATE_DEBUGGER_INIT_LIST(V)

#endif

#ifdef DEBUG

#define ISOLATE_INIT_DEBUG_ARRAY_LIST(V)                                       \
  V(CommentStatistic, paged_space_comments_statistics,                         \
      CommentStatistic::kMaxComments + 1)
#else

#define ISOLATE_INIT_DEBUG_ARRAY_LIST(V)

#endif

#define ISOLATE_INIT_ARRAY_LIST(V)                                             \
  /* SerializerDeserializer state. */                                          \
  V(Object*, serialize_partial_snapshot_cache, kPartialSnapshotCacheCapacity)  \
  V(int, jsregexp_static_offsets_vector, kJSRegexpStaticOffsetsVectorSize)     \
  V(int, bad_char_shift_table, kUC16AlphabetSize)                              \
  V(int, good_suffix_shift_table, (kBMMaxShift + 1))                           \
  V(int, suffix_table, (kBMMaxShift + 1))                                      \
  V(uint32_t, private_random_seed, 2)                                          \
  ISOLATE_INIT_DEBUG_ARRAY_LIST(V)

typedef List<HeapObject*, PreallocatedStorage> DebugObjectCache;

#define ISOLATE_INIT_LIST(V)                                                   \
  /* SerializerDeserializer state. */                                          \
  V(int, serialize_partial_snapshot_cache_length, 0)                           \
  /* Assembler state. */                                                       \
  /* A previously allocated buffer of kMinimalBufferSize bytes, or NULL. */    \
  V(byte*, assembler_spare_buffer, NULL)                                       \
  V(FatalErrorCallback, exception_behavior, NULL)                              \
  V(AllowCodeGenerationFromStringsCallback, allow_code_gen_callback, NULL)     \
  V(v8::Debug::MessageHandler, message_handler, NULL)                          \
  /* To distinguish the function templates, so that we can find them in the */ \
  /* function cache of the global context. */                                  \
  V(int, next_serial_number, 0)                                                \
  V(ExternalReferenceRedirectorPointer*, external_reference_redirector, NULL)  \
  V(bool, always_allow_natives_syntax, false)                                  \
  /* Part of the state of liveedit. */                                         \
  V(FunctionInfoListener*, active_function_info_listener, NULL)                \
  /* State for Relocatable. */                                                 \
  V(Relocatable*, relocatable_top, NULL)                                       \
  /* State for CodeEntry in profile-generator. */                              \
  V(CodeGenerator*, current_code_generator, NULL)                              \
  V(bool, jump_target_compiling_deferred_code, false)                          \
  V(DebugObjectCache*, string_stream_debug_object_cache, NULL)                 \
  V(Object*, string_stream_current_security_token, NULL)                       \
  /* TODO(isolates): Release this on destruction? */                           \
  V(int*, irregexp_interpreter_backtrack_stack_cache, NULL)                    \
  /* Serializer state. */                                                      \
  V(ExternalReferenceTable*, external_reference_table, NULL)                   \
  /* AstNode state. */                                                         \
  V(int, ast_node_id, 0)                                                       \
  V(unsigned, ast_node_count, 0)                                               \
  /* SafeStackFrameIterator activations count. */                              \
  V(int, safe_stack_iterator_counter, 0)                                       \
  V(uint64_t, enabled_cpu_features, 0)                                         \
  V(CpuProfiler*, cpu_profiler, NULL)                                          \
  V(HeapProfiler*, heap_profiler, NULL)                                        \
  ISOLATE_DEBUGGER_INIT_LIST(V)

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
#if !defined(__arm__) && defined(V8_TARGET_ARCH_ARM) || \
    !defined(__mips__) && defined(V8_TARGET_ARCH_MIPS)
          simulator_(NULL),
#endif
          next_(NULL),
          prev_(NULL) { }
    Isolate* isolate() const { return isolate_; }
    ThreadId thread_id() const { return thread_id_; }
    void set_stack_limit(uintptr_t value) { stack_limit_ = value; }
    uintptr_t stack_limit() const { return stack_limit_; }
    ThreadState* thread_state() const { return thread_state_; }
    void set_thread_state(ThreadState* value) { thread_state_ = value; }

#if !defined(__arm__) && defined(V8_TARGET_ARCH_ARM) || \
    !defined(__mips__) && defined(V8_TARGET_ARCH_MIPS)
    Simulator* simulator() const { return simulator_; }
    void set_simulator(Simulator* simulator) {
      simulator_ = simulator;
    }
#endif

    bool Matches(Isolate* isolate, ThreadId thread_id) const {
      return isolate_ == isolate && thread_id_.Equals(thread_id);
    }

   private:
    Isolate* isolate_;
    ThreadId thread_id_;
    uintptr_t stack_limit_;
    ThreadState* thread_state_;

#if !defined(__arm__) && defined(V8_TARGET_ARCH_ARM) || \
    !defined(__mips__) && defined(V8_TARGET_ARCH_MIPS)
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

  // Returns the PerIsolateThreadData for the current thread (or NULL if one is
  // not currently set).
  static PerIsolateThreadData* CurrentPerIsolateThreadData() {
    return reinterpret_cast<PerIsolateThreadData*>(
        Thread::GetThreadLocal(per_isolate_thread_data_key_));
  }

  // Returns the isolate inside which the current thread is running.
  INLINE(static Isolate* Current()) {
    Isolate* isolate = reinterpret_cast<Isolate*>(
        Thread::GetExistingThreadLocal(isolate_key_));
    ASSERT(isolate != NULL);
    return isolate;
  }

  INLINE(static Isolate* UncheckedCurrent()) {
    return reinterpret_cast<Isolate*>(Thread::GetThreadLocal(isolate_key_));
  }

  // Usually called by Init(), but can be called early e.g. to allow
  // testing components that require logging but not the whole
  // isolate.
  //
  // Safe to call more than once.
  void InitializeLoggingAndCounters();

  bool Init(Deserializer* des);

  bool IsInitialized() { return state_ == INITIALIZED; }

  // True if at least one thread Enter'ed this isolate.
  bool IsInUse() { return entry_stack_ != NULL; }

  // Destroys the non-default isolates.
  // Sets default isolate into "has_been_disposed" state rather then destroying,
  // for legacy API reasons.
  void TearDown();

  bool IsDefaultIsolate() const { return this == default_isolate_; }

  // Ensures that process-wide resources and the default isolate have been
  // allocated. It is only necessary to call this method in rare cases, for
  // example if you are using V8 from within the body of a static initializer.
  // Safe to call multiple times.
  static void EnsureDefaultIsolate();

  // Find the PerThread for this particular (isolate, thread) combination
  // If one does not yet exist, return null.
  PerIsolateThreadData* FindPerThreadDataForThisThread();

#ifdef ENABLE_DEBUGGER_SUPPORT
  // Get the debugger from the default isolate. Preinitializes the
  // default isolate if needed.
  static Debugger* GetDefaultIsolateDebugger();
#endif

  // Get the stack guard from the default isolate. Preinitializes the
  // default isolate if needed.
  static StackGuard* GetDefaultIsolateStackGuard();

  // Returns the key used to store the pointer to the current isolate.
  // Used internally for V8 threads that do not execute JavaScript but still
  // are part of the domain of an isolate (like the context switcher).
  static Thread::LocalStorageKey isolate_key() {
    return isolate_key_;
  }

  // Returns the key used to store process-wide thread IDs.
  static Thread::LocalStorageKey thread_id_key() {
    return thread_id_key_;
  }

  static Thread::LocalStorageKey per_isolate_thread_data_key();

  // If a client attempts to create a Locker without specifying an isolate,
  // we assume that the client is using legacy behavior. Set up the current
  // thread to be inside the implicit isolate (or fail a check if we have
  // switched to non-legacy behavior).
  static void EnterDefaultIsolate();

  // Mutex for serializing access to break control structures.
  Mutex* break_access() { return break_access_; }

  // Mutex for serializing access to debugger.
  Mutex* debugger_access() { return debugger_access_; }

  Address get_address_from_id(AddressId id);

  // Access to top context (where the current function object was created).
  Context* context() { return thread_local_top_.context_; }
  void set_context(Context* context) {
    ASSERT(context == NULL || context->IsContext());
    thread_local_top_.context_ = context;
  }
  Context** context_address() { return &thread_local_top_.context_; }

  SaveContext* save_context() {return thread_local_top_.save_context_; }
  void set_save_context(SaveContext* save) {
    thread_local_top_.save_context_ = save;
  }

  // Access to current thread id.
  ThreadId thread_id() { return thread_local_top_.thread_id_; }
  void set_thread_id(ThreadId id) { thread_local_top_.thread_id_ = id; }

  // Interface to pending exception.
  MaybeObject* pending_exception() {
    ASSERT(has_pending_exception());
    return thread_local_top_.pending_exception_;
  }
  bool external_caught_exception() {
    return thread_local_top_.external_caught_exception_;
  }
  void set_external_caught_exception(bool value) {
    thread_local_top_.external_caught_exception_ = value;
  }
  void set_pending_exception(MaybeObject* exception) {
    thread_local_top_.pending_exception_ = exception;
  }
  void clear_pending_exception() {
    thread_local_top_.pending_exception_ = heap_.the_hole_value();
  }
  MaybeObject** pending_exception_address() {
    return &thread_local_top_.pending_exception_;
  }
  bool has_pending_exception() {
    return !thread_local_top_.pending_exception_->IsTheHole();
  }
  void clear_pending_message() {
    thread_local_top_.has_pending_message_ = false;
    thread_local_top_.pending_message_obj_ = heap_.the_hole_value();
    thread_local_top_.pending_message_script_ = NULL;
  }
  v8::TryCatch* try_catch_handler() {
    return thread_local_top_.TryCatchHandler();
  }
  Address try_catch_handler_address() {
    return thread_local_top_.try_catch_handler_address();
  }
  bool* external_caught_exception_address() {
    return &thread_local_top_.external_caught_exception_;
  }
  v8::TryCatch* catcher() {
    return thread_local_top_.catcher_;
  }
  void set_catcher(v8::TryCatch* catcher) {
    thread_local_top_.catcher_ = catcher;
  }

  MaybeObject** scheduled_exception_address() {
    return &thread_local_top_.scheduled_exception_;
  }
  MaybeObject* scheduled_exception() {
    ASSERT(has_scheduled_exception());
    return thread_local_top_.scheduled_exception_;
  }
  bool has_scheduled_exception() {
    return thread_local_top_.scheduled_exception_ != heap_.the_hole_value();
  }
  void clear_scheduled_exception() {
    thread_local_top_.scheduled_exception_ = heap_.the_hole_value();
  }

  bool IsExternallyCaught();

  bool is_catchable_by_javascript(MaybeObject* exception) {
    return (exception != Failure::OutOfMemoryException()) &&
        (exception != heap()->termination_exception());
  }

  // JS execution stack (see frames.h).
  static Address c_entry_fp(ThreadLocalTop* thread) {
    return thread->c_entry_fp_;
  }
  static Address handler(ThreadLocalTop* thread) { return thread->handler_; }

  inline Address* c_entry_fp_address() {
    return &thread_local_top_.c_entry_fp_;
  }
  inline Address* handler_address() { return &thread_local_top_.handler_; }

  // Bottom JS entry (see StackTracer::Trace in log.cc).
  static Address js_entry_sp(ThreadLocalTop* thread) {
    return thread->js_entry_sp_;
  }
  inline Address* js_entry_sp_address() {
    return &thread_local_top_.js_entry_sp_;
  }

  // Generated code scratch locations.
  void* formal_count_address() { return &thread_local_top_.formal_count_; }

  // Returns the global object of the current context. It could be
  // a builtin object, or a JS global object.
  Handle<GlobalObject> global() {
    return Handle<GlobalObject>(context()->global());
  }

  // Returns the global proxy object of the current context.
  Object* global_proxy() {
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

  class ExceptionScope {
   public:
    explicit ExceptionScope(Isolate* isolate) :
      // Scope currently can only be used for regular exceptions, not
      // failures like OOM or termination exception.
      isolate_(isolate),
      pending_exception_(isolate_->pending_exception()->ToObjectUnchecked()),
      catcher_(isolate_->catcher())
    { }

    ~ExceptionScope() {
      isolate_->set_catcher(catcher_);
      isolate_->set_pending_exception(*pending_exception_);
    }

   private:
    Isolate* isolate_;
    Handle<Object> pending_exception_;
    v8::TryCatch* catcher_;
  };

  void SetCaptureStackTraceForUncaughtExceptions(
      bool capture,
      int frame_limit,
      StackTrace::StackTraceOptions options);

  // Tells whether the current context has experienced an out of memory
  // exception.
  bool is_out_of_memory();
  bool ignore_out_of_memory() {
    return thread_local_top_.ignore_out_of_memory_;
  }
  void set_ignore_out_of_memory(bool value) {
    thread_local_top_.ignore_out_of_memory_ = value;
  }

  void PrintCurrentStackTrace(FILE* out);
  void PrintStackTrace(FILE* out, char* thread_data);
  void PrintStack(StringStream* accumulator);
  void PrintStack();
  Handle<String> StackTraceString();
  Handle<JSArray> CaptureCurrentStackTrace(
      int frame_limit,
      StackTrace::StackTraceOptions options);

  void CaptureAndSetCurrentStackTraceFor(Handle<JSObject> error_object);

  // Returns if the top context may access the given global object. If
  // the result is false, the pending exception is guaranteed to be
  // set.
  bool MayNamedAccess(JSObject* receiver,
                      Object* key,
                      v8::AccessType type);
  bool MayIndexedAccess(JSObject* receiver,
                        uint32_t index,
                        v8::AccessType type);

  void SetFailedAccessCheckCallback(v8::FailedAccessCheckCallback callback);
  void ReportFailedAccessCheck(JSObject* receiver, v8::AccessType type);

  // Exception throwing support. The caller should use the result
  // of Throw() as its return value.
  Failure* Throw(Object* exception, MessageLocation* location = NULL);
  // Re-throw an exception.  This involves no error reporting since
  // error reporting was handled when the exception was thrown
  // originally.
  Failure* ReThrow(MaybeObject* exception, MessageLocation* location = NULL);
  void ScheduleThrow(Object* exception);
  void ReportPendingMessages();
  Failure* ThrowIllegalOperation();

  // Promote a scheduled exception to pending. Asserts has_scheduled_exception.
  Failure* PromoteScheduledException();
  void DoThrow(Object* exception, MessageLocation* location);
  // Checks if exception should be reported and finds out if it's
  // caught externally.
  bool ShouldReportException(bool* can_be_caught_externally,
                             bool catchable_by_javascript);

  // Attempts to compute the current source location, storing the
  // result in the target out parameter.
  void ComputeLocation(MessageLocation* target);

  // Override command line flag.
  void TraceException(bool flag);

  // Out of resource exception helpers.
  Failure* StackOverflow();
  Failure* TerminateExecution();

  // Administration
  void Iterate(ObjectVisitor* v);
  void Iterate(ObjectVisitor* v, ThreadLocalTop* t);
  char* Iterate(ObjectVisitor* v, char* t);
  void IterateThread(ThreadVisitor* v);
  void IterateThread(ThreadVisitor* v, char* t);


  // Returns the current global context.
  Handle<Context> global_context();

  // Returns the global context of the calling JavaScript code.  That
  // is, the global context of the top-most JavaScript frame.
  Handle<Context> GetCallingGlobalContext();

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
    ASSERT(OFFSET_OF(Isolate, name##_) == name##_debug_offset_);        \
    return name##_;                                                     \
  }                                                                     \
  inline void set_##name(type value) {                                  \
    ASSERT(OFFSET_OF(Isolate, name##_) == name##_debug_offset_);        \
    name##_ = value;                                                    \
  }
  ISOLATE_INIT_LIST(GLOBAL_ACCESSOR)
#undef GLOBAL_ACCESSOR

#define GLOBAL_ARRAY_ACCESSOR(type, name, length)                       \
  inline type* name() {                                                 \
    ASSERT(OFFSET_OF(Isolate, name##_) == name##_debug_offset_);        \
    return &(name##_)[0];                                               \
  }
  ISOLATE_INIT_ARRAY_LIST(GLOBAL_ARRAY_ACCESSOR)
#undef GLOBAL_ARRAY_ACCESSOR

#define GLOBAL_CONTEXT_FIELD_ACCESSOR(index, type, name)      \
  Handle<type> name() {                                       \
    return Handle<type>(context()->global_context()->name()); \
  }
  GLOBAL_CONTEXT_FIELDS(GLOBAL_CONTEXT_FIELD_ACCESSOR)
#undef GLOBAL_CONTEXT_FIELD_ACCESSOR

  Bootstrapper* bootstrapper() { return bootstrapper_; }
  Counters* counters() {
    // Call InitializeLoggingAndCounters() if logging is needed before
    // the isolate is fully initialized.
    ASSERT(counters_ != NULL);
    return counters_;
  }
  CodeRange* code_range() { return code_range_; }
  RuntimeProfiler* runtime_profiler() { return runtime_profiler_; }
  CompilationCache* compilation_cache() { return compilation_cache_; }
  Logger* logger() {
    // Call InitializeLoggingAndCounters() if logging is needed before
    // the isolate is fully initialized.
    ASSERT(logger_ != NULL);
    return logger_;
  }
  StackGuard* stack_guard() { return &stack_guard_; }
  Heap* heap() { return &heap_; }
  StatsTable* stats_table();
  StubCache* stub_cache() { return stub_cache_; }
  DeoptimizerData* deoptimizer_data() { return deoptimizer_data_; }
  ThreadLocalTop* thread_local_top() { return &thread_local_top_; }

  TranscendentalCache* transcendental_cache() const {
    return transcendental_cache_;
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

  v8::ImplementationUtilities::HandleScopeData* handle_scope_data() {
    return &handle_scope_data_;
  }
  HandleScopeImplementer* handle_scope_implementer() {
    ASSERT(handle_scope_implementer_);
    return handle_scope_implementer_;
  }
  Zone* zone() { return &zone_; }

  UnicodeCache* unicode_cache() {
    return unicode_cache_;
  }

  InnerPointerToCodeCache* inner_pointer_to_code_cache() {
    return inner_pointer_to_code_cache_;
  }

  StringInputBuffer* write_input_buffer() { return write_input_buffer_; }

  GlobalHandles* global_handles() { return global_handles_; }

  ThreadManager* thread_manager() { return thread_manager_; }

  ContextSwitcher* context_switcher() { return context_switcher_; }

  void set_context_switcher(ContextSwitcher* switcher) {
    context_switcher_ = switcher;
  }

  StringTracker* string_tracker() { return string_tracker_; }

  unibrow::Mapping<unibrow::Ecma262UnCanonicalize>* jsregexp_uncanonicalize() {
    return &jsregexp_uncanonicalize_;
  }

  unibrow::Mapping<unibrow::CanonicalizationRange>* jsregexp_canonrange() {
    return &jsregexp_canonrange_;
  }

  StringInputBuffer* objects_string_compare_buffer_a() {
    return &objects_string_compare_buffer_a_;
  }

  StringInputBuffer* objects_string_compare_buffer_b() {
    return &objects_string_compare_buffer_b_;
  }

  StaticResource<StringInputBuffer>* objects_string_input_buffer() {
    return &objects_string_input_buffer_;
  }

  RuntimeState* runtime_state() { return &runtime_state_; }

  void set_fp_stubs_generated(bool value) {
    fp_stubs_generated_ = value;
  }

  bool fp_stubs_generated() { return fp_stubs_generated_; }

  StaticResource<SafeStringInputBuffer>* compiler_safe_string_input_buffer() {
    return &compiler_safe_string_input_buffer_;
  }

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

  void* PreallocatedStorageNew(size_t size);
  void PreallocatedStorageDelete(void* p);
  void PreallocatedStorageInit(size_t size);

#ifdef ENABLE_DEBUGGER_SUPPORT
  Debugger* debugger() {
    if (!NoBarrier_Load(&debugger_initialized_)) InitializeDebugger();
    return debugger_;
  }
  Debug* debug() {
    if (!NoBarrier_Load(&debugger_initialized_)) InitializeDebugger();
    return debug_;
  }
#endif

  inline bool IsDebuggerActive();
  inline bool DebuggerHasBreakPoints();

#ifdef DEBUG
  HistogramInfo* heap_histograms() { return heap_histograms_; }

  JSObject::SpillInformation* js_spill_information() {
    return &js_spill_information_;
  }

  int* code_kind_statistics() { return code_kind_statistics_; }
#endif

#if defined(V8_TARGET_ARCH_ARM) && !defined(__arm__) || \
    defined(V8_TARGET_ARCH_MIPS) && !defined(__mips__)
  bool simulator_initialized() { return simulator_initialized_; }
  void set_simulator_initialized(bool initialized) {
    simulator_initialized_ = initialized;
  }

  HashMap* simulator_i_cache() { return simulator_i_cache_; }
  void set_simulator_i_cache(HashMap* hash_map) {
    simulator_i_cache_ = hash_map;
  }

  Redirection* simulator_redirection() {
    return simulator_redirection_;
  }
  void set_simulator_redirection(Redirection* redirection) {
    simulator_redirection_ = redirection;
  }
#endif

  Factory* factory() { return reinterpret_cast<Factory*>(this); }

  // SerializerDeserializer state.
  static const int kPartialSnapshotCacheCapacity = 1400;

  static const int kJSRegexpStaticOffsetsVectorSize = 128;

  Address external_callback() {
    return thread_local_top_.external_callback_;
  }
  void set_external_callback(Address callback) {
    thread_local_top_.external_callback_ = callback;
  }

  StateTag current_vm_state() {
    return thread_local_top_.current_vm_state_;
  }

  void SetCurrentVMState(StateTag state) {
    if (RuntimeProfiler::IsEnabled()) {
      // Make sure thread local top is initialized.
      ASSERT(thread_local_top_.isolate_ == this);
      StateTag current_state = thread_local_top_.current_vm_state_;
      if (current_state != JS && state == JS) {
        // Non-JS -> JS transition.
        RuntimeProfiler::IsolateEnteredJS(this);
      } else if (current_state == JS && state != JS) {
        // JS -> non-JS transition.
        ASSERT(RuntimeProfiler::IsSomeIsolateInJS());
        RuntimeProfiler::IsolateExitedJS(this);
      } else {
        // Other types of state transitions are not interesting to the
        // runtime profiler, because they don't affect whether we're
        // in JS or not.
        ASSERT((current_state == JS) == (state == JS));
      }
    }
    thread_local_top_.current_vm_state_ = state;
  }

  void SetData(void* data) { embedder_data_ = data; }
  void* GetData() { return embedder_data_; }

  LookupResult* top_lookup_result() {
    return thread_local_top_.top_lookup_result_;
  }
  void SetTopLookupResult(LookupResult* top) {
    thread_local_top_.top_lookup_result_ = top;
  }

  bool context_exit_happened() {
    return context_exit_happened_;
  }
  void set_context_exit_happened(bool context_exit_happened) {
    context_exit_happened_ = context_exit_happened;
  }

  double time_millis_since_init() {
    return OS::TimeCurrentMillis() - time_millis_at_init_;
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

 private:
  Isolate();

  friend struct GlobalState;
  friend struct InitializeGlobalState;

  enum State {
    UNINITIALIZED,    // Some components may not have been allocated.
    INITIALIZED       // All components are fully initialized.
  };

  // These fields are accessed through the API, offsets must be kept in sync
  // with v8::internal::Internals (in include/v8.h) constants. This is also
  // verified in Isolate::Init() using runtime checks.
  State state_;  // Will be padded to kApiPointerSize.
  void* embedder_data_;
  Heap heap_;

  // The per-process lock should be acquired before the ThreadDataTable is
  // modified.
  class ThreadDataTable {
   public:
    ThreadDataTable();
    ~ThreadDataTable();

    PerIsolateThreadData* Lookup(Isolate* isolate, ThreadId thread_id);
    void Insert(PerIsolateThreadData* data);
    void Remove(Isolate* isolate, ThreadId thread_id);
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

  // This mutex protects highest_thread_id_, thread_data_table_ and
  // default_isolate_.
  static Mutex* process_wide_mutex_;

  static Thread::LocalStorageKey per_isolate_thread_data_key_;
  static Thread::LocalStorageKey isolate_key_;
  static Thread::LocalStorageKey thread_id_key_;
  static Isolate* default_isolate_;
  static ThreadDataTable* thread_data_table_;

  void Deinit();

  static void SetIsolateThreadLocals(Isolate* isolate,
                                     PerIsolateThreadData* data);

  // Allocate and insert PerIsolateThreadData into the ThreadDataTable
  // (regardless of whether such data already exists).
  PerIsolateThreadData* AllocatePerIsolateThreadData(ThreadId thread_id);

  // Find the PerThread for this particular (isolate, thread) combination.
  // If one does not yet exist, allocate a new one.
  PerIsolateThreadData* FindOrAllocatePerThreadDataForThisThread();

  // PreInits and returns a default isolate. Needed when a new thread tries
  // to create a Locker for the first time (the lock itself is in the isolate).
  static Isolate* GetDefaultIsolateForLocking();

  // Initializes the current thread to run this Isolate.
  // Not thread-safe. Multiple threads should not Enter/Exit the same isolate
  // at the same time, this should be prevented using external locking.
  void Enter();

  // Exits the current thread. The previosuly entered Isolate is restored
  // for the thread.
  // Not thread-safe. Multiple threads should not Enter/Exit the same isolate
  // at the same time, this should be prevented using external locking.
  void Exit();

  void PreallocatedMemoryThreadStart();
  void PreallocatedMemoryThreadStop();
  void InitializeThreadLocal();

  void PrintStackTrace(FILE* out, ThreadLocalTop* thread);
  void MarkCompactPrologue(bool is_compacting,
                           ThreadLocalTop* archived_thread_data);
  void MarkCompactEpilogue(bool is_compacting,
                           ThreadLocalTop* archived_thread_data);

  void FillCache();

  void PropagatePendingExceptionToExternalTryCatch();

  void InitializeDebugger();

  // Traverse prototype chain to find out whether the object is derived from
  // the Error object.
  bool IsErrorObject(Handle<Object> obj);

  EntryStackItem* entry_stack_;
  int stack_trace_nesting_level_;
  StringStream* incomplete_message_;
  // The preallocated memory thread singleton.
  PreallocatedMemoryThread* preallocated_memory_thread_;
  Address isolate_addresses_[kIsolateAddressCount + 1];  // NOLINT
  NoAllocationStringAllocator* preallocated_message_space_;
  Bootstrapper* bootstrapper_;
  RuntimeProfiler* runtime_profiler_;
  CompilationCache* compilation_cache_;
  Counters* counters_;
  CodeRange* code_range_;
  Mutex* break_access_;
  Atomic32 debugger_initialized_;
  Mutex* debugger_access_;
  Logger* logger_;
  StackGuard stack_guard_;
  StatsTable* stats_table_;
  StubCache* stub_cache_;
  DeoptimizerData* deoptimizer_data_;
  ThreadLocalTop thread_local_top_;
  bool capture_stack_trace_for_uncaught_exceptions_;
  int stack_trace_for_uncaught_exceptions_frame_limit_;
  StackTrace::StackTraceOptions stack_trace_for_uncaught_exceptions_options_;
  TranscendentalCache* transcendental_cache_;
  MemoryAllocator* memory_allocator_;
  KeyedLookupCache* keyed_lookup_cache_;
  ContextSlotCache* context_slot_cache_;
  DescriptorLookupCache* descriptor_lookup_cache_;
  v8::ImplementationUtilities::HandleScopeData handle_scope_data_;
  HandleScopeImplementer* handle_scope_implementer_;
  UnicodeCache* unicode_cache_;
  Zone zone_;
  PreallocatedStorage in_use_list_;
  PreallocatedStorage free_list_;
  bool preallocated_storage_preallocated_;
  InnerPointerToCodeCache* inner_pointer_to_code_cache_;
  StringInputBuffer* write_input_buffer_;
  GlobalHandles* global_handles_;
  ContextSwitcher* context_switcher_;
  ThreadManager* thread_manager_;
  RuntimeState runtime_state_;
  bool fp_stubs_generated_;
  StaticResource<SafeStringInputBuffer> compiler_safe_string_input_buffer_;
  Builtins builtins_;
  bool has_installed_extensions_;
  StringTracker* string_tracker_;
  unibrow::Mapping<unibrow::Ecma262UnCanonicalize> jsregexp_uncanonicalize_;
  unibrow::Mapping<unibrow::CanonicalizationRange> jsregexp_canonrange_;
  StringInputBuffer objects_string_compare_buffer_a_;
  StringInputBuffer objects_string_compare_buffer_b_;
  StaticResource<StringInputBuffer> objects_string_input_buffer_;
  unibrow::Mapping<unibrow::Ecma262Canonicalize>
      regexp_macro_assembler_canonicalize_;
  RegExpStack* regexp_stack_;
  DateCache* date_cache_;
  unibrow::Mapping<unibrow::Ecma262Canonicalize> interp_canonicalize_mapping_;

  // The garbage collector should be a little more aggressive when it knows
  // that a context was recently exited.
  bool context_exit_happened_;

  // Time stamp at initialization.
  double time_millis_at_init_;

#if defined(V8_TARGET_ARCH_ARM) && !defined(__arm__) || \
    defined(V8_TARGET_ARCH_MIPS) && !defined(__mips__)
  bool simulator_initialized_;
  HashMap* simulator_i_cache_;
  Redirection* simulator_redirection_;
#endif

#ifdef DEBUG
  // A static array of histogram info for each type.
  HistogramInfo heap_histograms_[LAST_TYPE + 1];
  JSObject::SpillInformation js_spill_information_;
  int code_kind_statistics_[Code::NUMBER_OF_KINDS];
#endif

#ifdef ENABLE_DEBUGGER_SUPPORT
  Debugger* debugger_;
  Debug* debug_;
#endif

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

  friend class ExecutionAccess;
  friend class IsolateInitializer;
  friend class ThreadManager;
  friend class Simulator;
  friend class StackGuard;
  friend class ThreadId;
  friend class TestMemoryAllocatorScope;
  friend class v8::Isolate;
  friend class v8::Locker;
  friend class v8::Unlocker;

  DISALLOW_COPY_AND_ASSIGN(Isolate);
};


// If the GCC version is 4.1.x or 4.2.x an additional field is added to the
// class as a work around for a bug in the generated code found with these
// versions of GCC. See V8 issue 122 for details.
class SaveContext BASE_EMBEDDED {
 public:
  inline explicit SaveContext(Isolate* isolate);

  ~SaveContext() {
    if (context_.is_null()) {
      Isolate* isolate = Isolate::Current();
      isolate->set_context(NULL);
      isolate->set_save_context(prev_);
    } else {
      Isolate* isolate = context_->GetIsolate();
      isolate->set_context(*context_);
      isolate->set_save_context(prev_);
    }
  }

  Handle<Context> context() { return context_; }
  SaveContext* prev() { return prev_; }

  // Returns true if this save context is below a given JavaScript frame.
  bool IsBelowFrame(JavaScriptFrame* frame) {
    return (c_entry_fp_ == 0) || (c_entry_fp_ > frame->sp());
  }

 private:
  Handle<Context> context_;
#if __GNUC_VERSION__ >= 40100 && __GNUC_VERSION__ < 40300
  Handle<Context> dummy_;
#endif
  SaveContext* prev_;
  Address c_entry_fp_;
};


class AssertNoContextChange BASE_EMBEDDED {
#ifdef DEBUG
 public:
  AssertNoContextChange() :
      scope_(Isolate::Current()),
      context_(Isolate::Current()->context(), Isolate::Current()) {
  }

  ~AssertNoContextChange() {
    ASSERT(Isolate::Current()->context() == *context_);
  }

 private:
  HandleScope scope_;
  Handle<Context> context_;
#else
 public:
  AssertNoContextChange() { }
#endif
};


class ExecutionAccess BASE_EMBEDDED {
 public:
  explicit ExecutionAccess(Isolate* isolate) : isolate_(isolate) {
    Lock(isolate);
  }
  ~ExecutionAccess() { Unlock(isolate_); }

  static void Lock(Isolate* isolate) { isolate->break_access_->Lock(); }
  static void Unlock(Isolate* isolate) { isolate->break_access_->Unlock(); }

  static bool TryLock(Isolate* isolate) {
    return isolate->break_access_->TryLock();
  }

 private:
  Isolate* isolate_;
};


// Support for checking for stack-overflows in C++ code.
class StackLimitCheck BASE_EMBEDDED {
 public:
  explicit StackLimitCheck(Isolate* isolate) : isolate_(isolate) { }

  bool HasOverflowed() const {
    StackGuard* stack_guard = isolate_->stack_guard();
    // Stack has overflowed in C++ code only if stack pointer exceeds the C++
    // stack guard and the limits are not set to interrupt values.
    // TODO(214): Stack overflows are ignored if a interrupt is pending. This
    // code should probably always use the initial C++ limit.
    return (reinterpret_cast<uintptr_t>(this) < stack_guard->climit()) &&
           stack_guard->IsStackOverflow();
  }
 private:
  Isolate* isolate_;
};


// Support for temporarily postponing interrupts. When the outermost
// postpone scope is left the interrupts will be re-enabled and any
// interrupts that occurred while in the scope will be taken into
// account.
class PostponeInterruptsScope BASE_EMBEDDED {
 public:
  explicit PostponeInterruptsScope(Isolate* isolate)
      : stack_guard_(isolate->stack_guard()) {
    stack_guard_->thread_local_.postpone_interrupts_nesting_++;
    stack_guard_->DisableInterrupts();
  }

  ~PostponeInterruptsScope() {
    if (--stack_guard_->thread_local_.postpone_interrupts_nesting_ == 0) {
      stack_guard_->EnableInterrupts();
    }
  }
 private:
  StackGuard* stack_guard_;
};


// Temporary macros for accessing current isolate and its subobjects.
// They provide better readability, especially when used a lot in the code.
#define HEAP (v8::internal::Isolate::Current()->heap())
#define FACTORY (v8::internal::Isolate::Current()->factory())
#define ISOLATE (v8::internal::Isolate::Current())
#define ZONE (v8::internal::Isolate::Current()->zone())
#define LOGGER (v8::internal::Isolate::Current()->logger())


// Tells whether the global context is marked with out of memory.
inline bool Context::has_out_of_memory() {
  return global_context()->out_of_memory()->IsTrue();
}


// Mark the global context with out of memory.
inline void Context::mark_out_of_memory() {
  global_context()->set_out_of_memory(HEAP->true_value());
}


} }  // namespace v8::internal

#endif  // V8_ISOLATE_H_
