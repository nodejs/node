// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXECUTION_H_
#define V8_EXECUTION_H_

#include "src/base/atomicops.h"
#include "src/globals.h"

namespace v8 {
namespace internal {

class MicrotaskQueue;

template <typename T>
class Handle;

class Execution final : public AllStatic {
 public:
  // Whether to report pending messages, or keep them pending on the isolate.
  enum class MessageHandling { kReport, kKeepPending };
  enum class Target { kCallable, kRunMicrotasks };

  // Call a function, the caller supplies a receiver and an array
  // of arguments.
  //
  // When the function called is not in strict mode, receiver is
  // converted to an object.
  //
  V8_EXPORT_PRIVATE V8_WARN_UNUSED_RESULT static MaybeHandle<Object> Call(
      Isolate* isolate, Handle<Object> callable, Handle<Object> receiver,
      int argc, Handle<Object> argv[]);

  // Construct object from function, the caller supplies an array of
  // arguments.
  V8_WARN_UNUSED_RESULT static MaybeHandle<Object> New(
      Isolate* isolate, Handle<Object> constructor, int argc,
      Handle<Object> argv[]);
  V8_WARN_UNUSED_RESULT static MaybeHandle<Object> New(
      Isolate* isolate, Handle<Object> constructor, Handle<Object> new_target,
      int argc, Handle<Object> argv[]);

  // Call a function, just like Call(), but handle don't report exceptions
  // externally.
  // The return value is either the result of calling the function (if no
  // exception occurred), or an empty handle.
  // If message_handling is MessageHandling::kReport, exceptions (except for
  // termination exceptions) will be stored in exception_out (if not a
  // nullptr).
  V8_EXPORT_PRIVATE static MaybeHandle<Object> TryCall(
      Isolate* isolate, Handle<Object> callable, Handle<Object> receiver,
      int argc, Handle<Object> argv[], MessageHandling message_handling,
      MaybeHandle<Object>* exception_out);
  // Convenience method for performing RunMicrotasks
  static MaybeHandle<Object> TryRunMicrotasks(
      Isolate* isolate, MicrotaskQueue* microtask_queue,
      MaybeHandle<Object>* exception_out);
};


class ExecutionAccess;
class InterruptsScope;

// StackGuard contains the handling of the limits that are used to limit the
// number of nested invocations of JavaScript and the stack size used in each
// invocation.
class V8_EXPORT_PRIVATE StackGuard final {
 public:
  explicit StackGuard(Isolate* isolate) : isolate_(isolate) {}

  // Pass the address beyond which the stack should not grow.  The stack
  // is assumed to grow downwards.
  void SetStackLimit(uintptr_t limit);

  // The simulator uses a separate JS stack. Limits on the JS stack might have
  // to be adjusted in order to reflect overflows of the C stack, because we
  // cannot rely on the interleaving of frames on the simulator.
  void AdjustStackLimitForSimulator();

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

#define INTERRUPT_LIST(V)                                         \
  V(TERMINATE_EXECUTION, TerminateExecution, 0)                   \
  V(GC_REQUEST, GC, 1)                                            \
  V(INSTALL_CODE, InstallCode, 2)                                 \
  V(API_INTERRUPT, ApiInterrupt, 3)                               \
  V(DEOPT_MARKED_ALLOCATION_SITES, DeoptMarkedAllocationSites, 4) \
  V(GROW_SHARED_MEMORY, GrowSharedMemory, 5)                      \
  V(LOG_WASM_CODE, LogWasmCode, 6)

#define V(NAME, Name, id)                                                    \
  inline bool Check##Name() { return CheckInterrupt(NAME); }                 \
  inline bool CheckAndClear##Name() { return CheckAndClearInterrupt(NAME); } \
  inline void Request##Name() { RequestInterrupt(NAME); }                    \
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
  Object HandleInterrupts();

 private:
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
  static const uintptr_t kInterruptLimit = uintptr_t{0xfffffffffffffffe};
  static const uintptr_t kIllegalLimit = uintptr_t{0xfffffffffffffff8};
#else
  static const uintptr_t kInterruptLimit = 0xfffffffe;
  static const uintptr_t kIllegalLimit = 0xfffffff8;
#endif

  void PushInterruptsScope(InterruptsScope* scope);
  void PopInterruptsScope();

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
      return bit_cast<uintptr_t>(base::Relaxed_Load(&jslimit_));
    }
    void set_jslimit(uintptr_t limit) {
      return base::Relaxed_Store(&jslimit_,
                                 static_cast<base::AtomicWord>(limit));
    }
    uintptr_t climit() {
      return bit_cast<uintptr_t>(base::Relaxed_Load(&climit_));
    }
    void set_climit(uintptr_t limit) {
      return base::Relaxed_Store(&climit_,
                                 static_cast<base::AtomicWord>(limit));
    }

    InterruptsScope* interrupt_scopes_;
    int interrupt_flags_;
  };

  // TODO(isolates): Technically this could be calculated directly from a
  //                 pointer to StackGuard.
  Isolate* isolate_;
  ThreadLocal thread_local_;

  friend class Isolate;
  friend class StackLimitCheck;
  friend class InterruptsScope;

  DISALLOW_COPY_AND_ASSIGN(StackGuard);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_EXECUTION_H_
