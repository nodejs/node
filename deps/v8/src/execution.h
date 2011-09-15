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

#ifndef V8_EXECUTION_H_
#define V8_EXECUTION_H_

#include "allocation.h"

namespace v8 {
namespace internal {


// Flag used to set the interrupt causes.
enum InterruptFlag {
  INTERRUPT = 1 << 0,
  DEBUGBREAK = 1 << 1,
  DEBUGCOMMAND = 1 << 2,
  PREEMPT = 1 << 3,
  TERMINATE = 1 << 4,
  RUNTIME_PROFILER_TICK = 1 << 5
};

class Execution : public AllStatic {
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
  static Handle<Object> Call(Handle<Object> callable,
                             Handle<Object> receiver,
                             int argc,
                             Object*** args,
                             bool* pending_exception,
                             bool convert_receiver = false);

  // Construct object from function, the caller supplies an array of
  // arguments. Arguments are Object* type. After function returns,
  // pointers in 'args' might be invalid.
  //
  // *pending_exception tells whether the invoke resulted in
  // a pending exception.
  //
  static Handle<Object> New(Handle<JSFunction> func,
                            int argc,
                            Object*** args,
                            bool* pending_exception);

  // Call a function, just like Call(), but make sure to silently catch
  // any thrown exceptions. The return value is either the result of
  // calling the function (if caught exception is false) or the exception
  // that occurred (if caught exception is true).
  static Handle<Object> TryCall(Handle<JSFunction> func,
                                Handle<Object> receiver,
                                int argc,
                                Object*** args,
                                bool* caught_exception);

  // ECMA-262 9.2
  static Handle<Object> ToBoolean(Handle<Object> obj);

  // ECMA-262 9.3
  static Handle<Object> ToNumber(Handle<Object> obj, bool* exc);

  // ECMA-262 9.4
  static Handle<Object> ToInteger(Handle<Object> obj, bool* exc);

  // ECMA-262 9.5
  static Handle<Object> ToInt32(Handle<Object> obj, bool* exc);

  // ECMA-262 9.6
  static Handle<Object> ToUint32(Handle<Object> obj, bool* exc);

  // ECMA-262 9.8
  static Handle<Object> ToString(Handle<Object> obj, bool* exc);

  // ECMA-262 9.8
  static Handle<Object> ToDetailString(Handle<Object> obj, bool* exc);

  // ECMA-262 9.9
  static Handle<Object> ToObject(Handle<Object> obj, bool* exc);

  // Create a new date object from 'time'.
  static Handle<Object> NewDate(double time, bool* exc);

  // Create a new regular expression object from 'pattern' and 'flags'.
  static Handle<JSRegExp> NewJSRegExp(Handle<String> pattern,
                                      Handle<String> flags,
                                      bool* exc);

  // Used to implement [] notation on strings (calls JS code)
  static Handle<Object> CharAt(Handle<String> str, uint32_t index);

  static Handle<Object> GetFunctionFor();
  static Handle<JSFunction> InstantiateFunction(
      Handle<FunctionTemplateInfo> data, bool* exc);
  static Handle<JSObject> InstantiateObject(Handle<ObjectTemplateInfo> data,
                                            bool* exc);
  static void ConfigureInstance(Handle<Object> instance,
                                Handle<Object> data,
                                bool* exc);
  static Handle<String> GetStackTraceLine(Handle<Object> recv,
                                          Handle<JSFunction> fun,
                                          Handle<Object> pos,
                                          Handle<Object> is_global);
#ifdef ENABLE_DEBUGGER_SUPPORT
  static Object* DebugBreakHelper();
  static void ProcessDebugMesssages(bool debug_command_only);
#endif

  // If the stack guard is triggered, but it is not an actual
  // stack overflow, then handle the interruption accordingly.
  MUST_USE_RESULT static MaybeObject* HandleStackGuardInterrupt();

  // Get a function delegate (or undefined) for the given non-function
  // object. Used for support calling objects as functions.
  static Handle<Object> GetFunctionDelegate(Handle<Object> object);
  static Handle<Object> TryGetFunctionDelegate(Handle<Object> object,
                                               bool* has_pending_exception);

  // Get a function delegate (or undefined) for the given non-function
  // object. Used for support calling objects as constructors.
  static Handle<Object> GetConstructorDelegate(Handle<Object> object);
  static Handle<Object> TryGetConstructorDelegate(Handle<Object> object,
                                                  bool* has_pending_exception);
};


class ExecutionAccess;
class Isolate;


// StackGuard contains the handling of the limits that are used to limit the
// number of nested invocations of JavaScript and the stack size used in each
// invocation.
class StackGuard {
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

  bool IsStackOverflow();
  bool IsPreempted();
  void Preempt();
  bool IsInterrupted();
  void Interrupt();
  bool IsTerminateExecution();
  void TerminateExecution();
  bool IsRuntimeProfilerTick();
  void RequestRuntimeProfilerTick();
#ifdef ENABLE_DEBUGGER_SUPPORT
  bool IsDebugBreak();
  void DebugBreak();
  bool IsDebugCommand();
  void DebugCommand();
#endif
  void Continue(InterruptFlag after_what);

  // This provides an asynchronous read of the stack limits for the current
  // thread.  There are no locks protecting this, but it is assumed that you
  // have the global V8 lock if you are using multiple V8 threads.
  uintptr_t climit() {
    return thread_local_.climit_;
  }
  uintptr_t real_climit() {
    return thread_local_.real_climit_;
  }
  uintptr_t jslimit() {
    return thread_local_.jslimit_;
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

 private:
  StackGuard();

  // You should hold the ExecutionAccess lock when calling this method.
  bool has_pending_interrupts(const ExecutionAccess& lock) {
    // Sanity check: We shouldn't be asking about pending interrupts
    // unless we're not postponing them anymore.
    ASSERT(!should_postpone_interrupts(lock));
    return thread_local_.interrupt_flags_ != 0;
  }

  // You should hold the ExecutionAccess lock when calling this method.
  bool should_postpone_interrupts(const ExecutionAccess& lock) {
    return thread_local_.postpone_interrupts_nesting_ > 0;
  }

  // You should hold the ExecutionAccess lock when calling this method.
  inline void set_interrupt_limits(const ExecutionAccess& lock);

  // Reset limits to actual values. For example after handling interrupt.
  // You should hold the ExecutionAccess lock when calling this method.
  inline void reset_limits(const ExecutionAccess& lock);

  // Enable or disable interrupts.
  void EnableInterrupts();
  void DisableInterrupts();

#ifdef V8_TARGET_ARCH_X64
  static const uintptr_t kInterruptLimit = V8_UINT64_C(0xfffffffffffffffe);
  static const uintptr_t kIllegalLimit = V8_UINT64_C(0xfffffffffffffff8);
#else
  static const uintptr_t kInterruptLimit = 0xfffffffe;
  static const uintptr_t kIllegalLimit = 0xfffffff8;
#endif

  class ThreadLocal {
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
    uintptr_t jslimit_;
    uintptr_t real_climit_;  // Actual C++ stack limit set for the VM.
    uintptr_t climit_;

    int nesting_;
    int postpone_interrupts_nesting_;
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
