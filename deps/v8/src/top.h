// Copyright 2006-2008 the V8 project authors. All rights reserved.
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

#ifndef V8_TOP_H_
#define V8_TOP_H_

#include "frames-inl.h"

namespace v8 {
namespace internal {


#define RETURN_IF_SCHEDULED_EXCEPTION() \
  if (Top::has_scheduled_exception()) return Top::PromoteScheduledException()

// Top has static variables used for JavaScript execution.

class SaveContext;  // Forward declaration.
class ThreadVisitor;  // Defined in v8threads.h

class ThreadLocalTop BASE_EMBEDDED {
 public:
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

  // The context where the current execution method is created and for variable
  // lookups.
  Context* context_;
  int thread_id_;
  Object* pending_exception_;
  bool has_pending_message_;
  const char* pending_message_;
  Object* pending_message_obj_;
  Script* pending_message_script_;
  int pending_message_start_pos_;
  int pending_message_end_pos_;
  // Use a separate value for scheduled exceptions to preserve the
  // invariants that hold about pending_exception.  We may want to
  // unify them later.
  Object* scheduled_exception_;
  bool external_caught_exception_;
  SaveContext* save_context_;
  v8::TryCatch* catcher_;

  // Stack.
  Address c_entry_fp_;  // the frame pointer of the top c entry frame
  Address handler_;   // try-blocks are chained through the stack
#ifdef ENABLE_LOGGING_AND_PROFILING
  Address js_entry_sp_;  // the stack pointer of the bottom js entry frame
#endif
  bool stack_is_cooked_;
  inline bool stack_is_cooked() { return stack_is_cooked_; }
  inline void set_stack_is_cooked(bool value) { stack_is_cooked_ = value; }

  // Generated code scratch locations.
  int32_t formal_count_;

  // Call back function to report unsafe JS accesses.
  v8::FailedAccessCheckCallback failed_access_check_callback_;

 private:
  Address try_catch_handler_address_;
};

#define TOP_ADDRESS_LIST(C)            \
  C(handler_address)                   \
  C(c_entry_fp_address)                \
  C(context_address)                   \
  C(pending_exception_address)         \
  C(external_caught_exception_address)

#ifdef ENABLE_LOGGING_AND_PROFILING
#define TOP_ADDRESS_LIST_PROF(C)       \
  C(js_entry_sp_address)
#else
#define TOP_ADDRESS_LIST_PROF(C)
#endif


class Top {
 public:
  enum AddressId {
#define C(name) k_##name,
    TOP_ADDRESS_LIST(C)
    TOP_ADDRESS_LIST_PROF(C)
#undef C
    k_top_address_count
  };

  static Address get_address_from_id(AddressId id);

  // Access to top context (where the current function object was created).
  static Context* context() { return thread_local_.context_; }
  static void set_context(Context* context) {
    thread_local_.context_ = context;
  }
  static Context** context_address() { return &thread_local_.context_; }

  static SaveContext* save_context() {return thread_local_.save_context_; }
  static void set_save_context(SaveContext* save) {
    thread_local_.save_context_ = save;
  }

  // Access to current thread id.
  static int thread_id() { return thread_local_.thread_id_; }
  static void set_thread_id(int id) { thread_local_.thread_id_ = id; }

  // Interface to pending exception.
  static Object* pending_exception() {
    ASSERT(has_pending_exception());
    return thread_local_.pending_exception_;
  }
  static bool external_caught_exception() {
    return thread_local_.external_caught_exception_;
  }
  static void set_pending_exception(Object* exception) {
    thread_local_.pending_exception_ = exception;
  }
  static void clear_pending_exception() {
    thread_local_.pending_exception_ = Heap::the_hole_value();
  }

  static Object** pending_exception_address() {
    return &thread_local_.pending_exception_;
  }
  static bool has_pending_exception() {
    return !thread_local_.pending_exception_->IsTheHole();
  }
  static void clear_pending_message() {
    thread_local_.has_pending_message_ = false;
    thread_local_.pending_message_ = NULL;
    thread_local_.pending_message_obj_ = Heap::the_hole_value();
    thread_local_.pending_message_script_ = NULL;
  }
  static v8::TryCatch* try_catch_handler() {
    return thread_local_.TryCatchHandler();
  }
  static Address try_catch_handler_address() {
    return thread_local_.try_catch_handler_address();
  }
  // This method is called by the api after operations that may throw
  // exceptions.  If an exception was thrown and not handled by an external
  // handler the exception is scheduled to be rethrown when we return to running
  // JavaScript code.  If an exception is scheduled true is returned.
  static bool OptionalRescheduleException(bool is_bottom_call);


  static bool* external_caught_exception_address() {
    return &thread_local_.external_caught_exception_;
  }

  static Object** scheduled_exception_address() {
    return &thread_local_.scheduled_exception_;
  }

  static Object* scheduled_exception() {
    ASSERT(has_scheduled_exception());
    return thread_local_.scheduled_exception_;
  }
  static bool has_scheduled_exception() {
    return !thread_local_.scheduled_exception_->IsTheHole();
  }
  static void clear_scheduled_exception() {
    thread_local_.scheduled_exception_ = Heap::the_hole_value();
  }

  static void setup_external_caught() {
    thread_local_.external_caught_exception_ =
        has_pending_exception() &&
        (thread_local_.catcher_ != NULL) &&
        (try_catch_handler() == thread_local_.catcher_);
  }

  // Tells whether the current context has experienced an out of memory
  // exception.
  static bool is_out_of_memory();

  // JS execution stack (see frames.h).
  static Address c_entry_fp(ThreadLocalTop* thread) {
    return thread->c_entry_fp_;
  }
  static Address handler(ThreadLocalTop* thread) { return thread->handler_; }

  static inline Address* c_entry_fp_address() {
    return &thread_local_.c_entry_fp_;
  }
  static inline Address* handler_address() { return &thread_local_.handler_; }

#ifdef ENABLE_LOGGING_AND_PROFILING
  // Bottom JS entry (see StackTracer::Trace in log.cc).
  static Address js_entry_sp(ThreadLocalTop* thread) {
    return thread->js_entry_sp_;
  }
  static inline Address* js_entry_sp_address() {
    return &thread_local_.js_entry_sp_;
  }
#endif

  // Generated code scratch locations.
  static void* formal_count_address() { return &thread_local_.formal_count_; }

  static void MarkCompactPrologue(bool is_compacting);
  static void MarkCompactEpilogue(bool is_compacting);
  static void MarkCompactPrologue(bool is_compacting,
                                  char* archived_thread_data);
  static void MarkCompactEpilogue(bool is_compacting,
                                  char* archived_thread_data);
  static void PrintCurrentStackTrace(FILE* out);
  static void PrintStackTrace(FILE* out, char* thread_data);
  static void PrintStack(StringStream* accumulator);
  static void PrintStack();
  static Handle<String> StackTraceString();
  static Local<StackTrace> CaptureCurrentStackTrace(
      int frame_limit,
      StackTrace::StackTraceOptions options);

  // Returns if the top context may access the given global object. If
  // the result is false, the pending exception is guaranteed to be
  // set.
  static bool MayNamedAccess(JSObject* receiver,
                             Object* key,
                             v8::AccessType type);
  static bool MayIndexedAccess(JSObject* receiver,
                               uint32_t index,
                               v8::AccessType type);

  static void SetFailedAccessCheckCallback(
      v8::FailedAccessCheckCallback callback);
  static void ReportFailedAccessCheck(JSObject* receiver, v8::AccessType type);

  // Exception throwing support. The caller should use the result
  // of Throw() as its return value.
  static Failure* Throw(Object* exception, MessageLocation* location = NULL);
  // Re-throw an exception.  This involves no error reporting since
  // error reporting was handled when the exception was thrown
  // originally.
  static Failure* ReThrow(Object* exception, MessageLocation* location = NULL);
  static void ScheduleThrow(Object* exception);
  static void ReportPendingMessages();
  static Failure* ThrowIllegalOperation();

  // Promote a scheduled exception to pending. Asserts has_scheduled_exception.
  static Object* PromoteScheduledException();
  static void DoThrow(Object* exception,
                      MessageLocation* location,
                      const char* message);
  static bool ShouldReturnException(bool* is_caught_externally,
                                    bool catchable_by_javascript);
  static void ReportUncaughtException(Handle<Object> exception,
                                      MessageLocation* location,
                                      Handle<String> stack_trace);

  // Attempts to compute the current source location, storing the
  // result in the target out parameter.
  static void ComputeLocation(MessageLocation* target);

  // Override command line flag.
  static void TraceException(bool flag);

  // Out of resource exception helpers.
  static Failure* StackOverflow();
  static Failure* TerminateExecution();

  // Administration
  static void Initialize();
  static void TearDown();
  static void Iterate(ObjectVisitor* v);
  static void Iterate(ObjectVisitor* v, ThreadLocalTop* t);
  static char* Iterate(ObjectVisitor* v, char* t);
  static void IterateThread(ThreadVisitor* v);
  static void IterateThread(ThreadVisitor* v, char* t);

  // Returns the global object of the current context. It could be
  // a builtin object, or a js global object.
  static Handle<GlobalObject> global() {
    return Handle<GlobalObject>(context()->global());
  }

  // Returns the global proxy object of the current context.
  static Object* global_proxy() {
    return context()->global_proxy();
  }

  // Returns the current global context.
  static Handle<Context> global_context();

  // Returns the global context of the calling JavaScript code.  That
  // is, the global context of the top-most JavaScript frame.
  static Handle<Context> GetCallingGlobalContext();

  static Handle<JSBuiltinsObject> builtins() {
    return Handle<JSBuiltinsObject>(thread_local_.context_->builtins());
  }

  static void RegisterTryCatchHandler(v8::TryCatch* that);
  static void UnregisterTryCatchHandler(v8::TryCatch* that);

#define TOP_GLOBAL_CONTEXT_FIELD_ACCESSOR(index, type, name)  \
  static Handle<type> name() {                                \
    return Handle<type>(context()->global_context()->name()); \
  }
  GLOBAL_CONTEXT_FIELDS(TOP_GLOBAL_CONTEXT_FIELD_ACCESSOR)
#undef TOP_GLOBAL_CONTEXT_FIELD_ACCESSOR

  static inline ThreadLocalTop* GetCurrentThread() { return &thread_local_; }
  static int ArchiveSpacePerThread() { return sizeof(ThreadLocalTop); }
  static char* ArchiveThread(char* to);
  static char* RestoreThread(char* from);
  static void FreeThreadResources() { thread_local_.Free(); }

  static const char* kStackOverflowMessage;

 private:
  // The context that initiated this JS execution.
  static ThreadLocalTop thread_local_;
  static void InitializeThreadLocal();
  static void PrintStackTrace(FILE* out, ThreadLocalTop* thread);
  static void MarkCompactPrologue(bool is_compacting,
                                  ThreadLocalTop* archived_thread_data);
  static void MarkCompactEpilogue(bool is_compacting,
                                  ThreadLocalTop* archived_thread_data);

  // Debug.
  // Mutex for serializing access to break control structures.
  static Mutex* break_access_;

  friend class SaveContext;
  friend class AssertNoContextChange;
  friend class ExecutionAccess;

  static void FillCache();
};


// If the GCC version is 4.1.x or 4.2.x an additional field is added to the
// class as a work around for a bug in the generated code found with these
// versions of GCC. See V8 issue 122 for details.
class SaveContext BASE_EMBEDDED {
 public:
  SaveContext()
      : context_(Top::context()),
#if __GNUC_VERSION__ >= 40100 && __GNUC_VERSION__ < 40300
        dummy_(Top::context()),
#endif
        prev_(Top::save_context()) {
    Top::set_save_context(this);

    // If there is no JS frame under the current C frame, use the value 0.
    JavaScriptFrameIterator it;
    js_sp_ = it.done() ? 0 : it.frame()->sp();
  }

  ~SaveContext() {
    Top::set_context(*context_);
    Top::set_save_context(prev_);
  }

  Handle<Context> context() { return context_; }
  SaveContext* prev() { return prev_; }

  // Returns true if this save context is below a given JavaScript frame.
  bool below(JavaScriptFrame* frame) {
    return (js_sp_ == 0) || (frame->sp() < js_sp_);
  }

 private:
  Handle<Context> context_;
#if __GNUC_VERSION__ >= 40100 && __GNUC_VERSION__ < 40300
  Handle<Context> dummy_;
#endif
  SaveContext* prev_;
  Address js_sp_;  // The top JS frame's sp when saving context.
};


class AssertNoContextChange BASE_EMBEDDED {
#ifdef DEBUG
 public:
  AssertNoContextChange() :
      context_(Top::context()) {
  }

  ~AssertNoContextChange() {
    ASSERT(Top::context() == *context_);
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
  ExecutionAccess();
  ~ExecutionAccess();
};

} }  // namespace v8::internal

#endif  // V8_TOP_H_
