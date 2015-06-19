// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXECUTION_H_
#define V8_EXECUTION_H_

#include "src/handles.h"

namespace v8 {
namespace internal {

class Execution final : public AllStatic {
 public:
  // Call a function, the caller supplies a receiver and an array
  // of arguments. Arguments are Object* type. After function returns,
  // pointers in 'args' might be invalid.
  //
  // *pending_exception tells whether the invoke resulted in
  // a pending exception.
  //
  // When convert_receiver is set, and the receiver is not an object,
  // and the function called is not in strict mode, receiver is converted to
  // an object.
  //
  MUST_USE_RESULT static MaybeHandle<Object> Call(
      Isolate* isolate,
      Handle<Object> callable,
      Handle<Object> receiver,
      int argc,
      Handle<Object> argv[],
      bool convert_receiver = false);

  // Construct object from function, the caller supplies an array of
  // arguments. Arguments are Object* type. After function returns,
  // pointers in 'args' might be invalid.
  //
  // *pending_exception tells whether the invoke resulted in
  // a pending exception.
  //
  MUST_USE_RESULT static MaybeHandle<Object> New(Handle<JSFunction> func,
                                                 int argc,
                                                 Handle<Object> argv[]);

  // Call a function, just like Call(), but make sure to silently catch
  // any thrown exceptions. The return value is either the result of
  // calling the function (if caught exception is false) or the exception
  // that occurred (if caught exception is true).
  // In the exception case, exception_out holds the caught exceptions, unless
  // it is a termination exception.
  static MaybeHandle<Object> TryCall(Handle<JSFunction> func,
                                     Handle<Object> receiver, int argc,
                                     Handle<Object> argv[],
                                     MaybeHandle<Object>* exception_out = NULL);

  // ECMA-262 9.3
  MUST_USE_RESULT static MaybeHandle<Object> ToNumber(
      Isolate* isolate, Handle<Object> obj);

  // ECMA-262 9.4
  MUST_USE_RESULT static MaybeHandle<Object> ToInteger(
      Isolate* isolate, Handle<Object> obj);

  // ECMA-262 9.5
  MUST_USE_RESULT static MaybeHandle<Object> ToInt32(
      Isolate* isolate, Handle<Object> obj);

  // ECMA-262 9.6
  MUST_USE_RESULT static MaybeHandle<Object> ToUint32(
      Isolate* isolate, Handle<Object> obj);


  // ES6, draft 10-14-14, section 7.1.15
  MUST_USE_RESULT static MaybeHandle<Object> ToLength(
      Isolate* isolate, Handle<Object> obj);

  // ECMA-262 9.8
  MUST_USE_RESULT static MaybeHandle<Object> ToString(
      Isolate* isolate, Handle<Object> obj);

  // ECMA-262 9.8
  MUST_USE_RESULT static MaybeHandle<Object> ToDetailString(
      Isolate* isolate, Handle<Object> obj);

  // ECMA-262 9.9
  MUST_USE_RESULT static MaybeHandle<Object> ToObject(
      Isolate* isolate, Handle<Object> obj);

  // Create a new date object from 'time'.
  MUST_USE_RESULT static MaybeHandle<Object> NewDate(
      Isolate* isolate, double time);

  // Create a new regular expression object from 'pattern' and 'flags'.
  MUST_USE_RESULT static MaybeHandle<JSRegExp> NewJSRegExp(
      Handle<String> pattern, Handle<String> flags);

  // Used to implement [] notation on strings (calls JS code)
  static Handle<Object> CharAt(Handle<String> str, uint32_t index);

  static Handle<Object> GetFunctionFor();
  static Handle<String> GetStackTraceLine(Handle<Object> recv,
                                          Handle<JSFunction> fun,
                                          Handle<Object> pos,
                                          Handle<Object> is_global);

  // Get a function delegate (or undefined) for the given non-function
  // object. Used for support calling objects as functions.
  static Handle<Object> GetFunctionDelegate(Isolate* isolate,
                                            Handle<Object> object);
  MUST_USE_RESULT static MaybeHandle<Object> TryGetFunctionDelegate(
      Isolate* isolate,
      Handle<Object> object);

  // Get a function delegate (or undefined) for the given non-function
  // object. Used for support calling objects as constructors.
  static Handle<Object> GetConstructorDelegate(Isolate* isolate,
                                               Handle<Object> object);
  static MaybeHandle<Object> TryGetConstructorDelegate(Isolate* isolate,
                                                       Handle<Object> object);
};


class ExecutionAccess;
class PostponeInterruptsScope;


// StackGuard contains the handling of the limits that are used to limit the
// number of nested invocations of JavaScript and the stack size used in each
// invocation.
class StackGuard final {
 public:
  // Pass the address beyond which the stack should not grow.  The stack
  // is assumed to grow downwards.
  void SetStackLimit(uintptr_t limit);

  // Threading support.
  char* ArchiveStackGuard(char* to);
  char* RestoreStackGuard(char* from);
  static int ArchiveSpacePerThread() { return sizeof(ThreadLocal); }
  void FreeThreadResources();
  // Sets up the default stack guard for this thread if it has not
  // already been set up.
  void InitThread(const ExecutionAccess& lock);
  // Clears the stack guard for this thread so it does not look as if
  // it has been set up.
  void ClearThread(const ExecutionAccess& lock);

#define INTERRUPT_LIST(V)                                          \
  V(DEBUGBREAK, DebugBreak, 0)                                     \
  V(DEBUGCOMMAND, DebugCommand, 1)                                 \
  V(TERMINATE_EXECUTION, TerminateExecution, 2)                    \
  V(GC_REQUEST, GC, 3)                                             \
  V(INSTALL_CODE, InstallCode, 4)                                  \
  V(API_INTERRUPT, ApiInterrupt, 5)                                \
  V(DEOPT_MARKED_ALLOCATION_SITES, DeoptMarkedAllocationSites, 6)

#define V(NAME, Name, id)                                          \
  inline bool Check##Name() { return CheckInterrupt(NAME); }  \
  inline void Request##Name() { RequestInterrupt(NAME); }     \
  inline void Clear##Name() { ClearInterrupt(NAME); }
  INTERRUPT_LIST(V)
#undef V

  // Flag used to set the interrupt causes.
  enum InterruptFlag {
  #define V(NAME, Name, id) NAME = (1 << id),
    INTERRUPT_LIST(V)
  #undef V
  #define V(NAME, Name, id) NAME |
    ALL_INTERRUPTS = INTERRUPT_LIST(V) 0
  #undef V
  };

  uintptr_t climit() { return thread_local_.climit(); }
  uintptr_t jslimit() { return thread_local_.jslimit(); }
  // This provides an asynchronous read of the stack limits for the current
  // thread.  There are no locks protecting this, but it is assumed that you
  // have the global V8 lock if you are using multiple V8 threads.
  uintptr_t real_climit() {
    return thread_local_.real_climit_;
  }
  uintptr_t real_jslimit() {
    return thread_local_.real_jslimit_;
  }
  Address address_of_jslimit() {
    return reinterpret_cast<Address>(&thread_local_.jslimit_);
  }
  Address address_of_real_jslimit() {
    return reinterpret_cast<Address>(&thread_local_.real_jslimit_);
  }

  // If the stack guard is triggered, but it is not an actual
  // stack overflow, then handle the interruption accordingly.
  Object* HandleInterrupts();

  bool InterruptRequested() { return GetCurrentStackPosition() < climit(); }

  void CheckAndHandleGCInterrupt();

 private:
  StackGuard();

  bool CheckInterrupt(InterruptFlag flag);
  void RequestInterrupt(InterruptFlag flag);
  void ClearInterrupt(InterruptFlag flag);
  bool CheckAndClearInterrupt(InterruptFlag flag);

  // You should hold the ExecutionAccess lock when calling this method.
  bool has_pending_interrupts(const ExecutionAccess& lock) {
    return thread_local_.interrupt_flags_ != 0;
  }

  // You should hold the ExecutionAccess lock when calling this method.
  inline void set_interrupt_limits(const ExecutionAccess& lock);

  // Reset limits to actual values. For example after handling interrupt.
  // You should hold the ExecutionAccess lock when calling this method.
  inline void reset_limits(const ExecutionAccess& lock);

  // Enable or disable interrupts.
  void EnableInterrupts();
  void DisableInterrupts();

#if V8_TARGET_ARCH_64_BIT
  static const uintptr_t kInterruptLimit = V8_UINT64_C(0xfffffffffffffffe);
  static const uintptr_t kIllegalLimit = V8_UINT64_C(0xfffffffffffffff8);
#else
  static const uintptr_t kInterruptLimit = 0xfffffffe;
  static const uintptr_t kIllegalLimit = 0xfffffff8;
#endif

  void PushPostponeInterruptsScope(PostponeInterruptsScope* scope);
  void PopPostponeInterruptsScope();

  class ThreadLocal final {
   public:
    ThreadLocal() { Clear(); }
    // You should hold the ExecutionAccess lock when you call Initialize or
    // Clear.
    void Clear();

    // Returns true if the heap's stack limits should be set, false if not.
    bool Initialize(Isolate* isolate);

    // The stack limit is split into a JavaScript and a C++ stack limit. These
    // two are the same except when running on a simulator where the C++ and
    // JavaScript stacks are separate. Each of the two stack limits have two
    // values. The one eith the real_ prefix is the actual stack limit
    // set for the VM. The one without the real_ prefix has the same value as
    // the actual stack limit except when there is an interruption (e.g. debug
    // break or preemption) in which case it is lowered to make stack checks
    // fail. Both the generated code and the runtime system check against the
    // one without the real_ prefix.
    uintptr_t real_jslimit_;  // Actual JavaScript stack limit set for the VM.
    uintptr_t real_climit_;  // Actual C++ stack limit set for the VM.

    // jslimit_ and climit_ can be read without any lock.
    // Writing requires the ExecutionAccess lock.
    base::AtomicWord jslimit_;
    base::AtomicWord climit_;

    uintptr_t jslimit() {
      return bit_cast<uintptr_t>(base::NoBarrier_Load(&jslimit_));
    }
    void set_jslimit(uintptr_t limit) {
      return base::NoBarrier_Store(&jslimit_,
                                   static_cast<base::AtomicWord>(limit));
    }
    uintptr_t climit() {
      return bit_cast<uintptr_t>(base::NoBarrier_Load(&climit_));
    }
    void set_climit(uintptr_t limit) {
      return base::NoBarrier_Store(&climit_,
                                   static_cast<base::AtomicWord>(limit));
    }

    PostponeInterruptsScope* postpone_interrupts_;
    int interrupt_flags_;
  };

  // TODO(isolates): Technically this could be calculated directly from a
  //                 pointer to StackGuard.
  Isolate* isolate_;
  ThreadLocal thread_local_;

  friend class Isolate;
  friend class StackLimitCheck;
  friend class PostponeInterruptsScope;

  DISALLOW_COPY_AND_ASSIGN(StackGuard);
};

} }  // namespace v8::internal

#endif  // V8_EXECUTION_H_
