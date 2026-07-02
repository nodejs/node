//===--- A platform independent indirection for a thread class --*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_THREADS_THREAD_H
#define LLVM_LIBC_SRC___SUPPORT_THREADS_THREAD_H

#include "hdr/stdint_proxy.h"
#include "src/__support/CPP/atomic.h"
#include "src/__support/CPP/optional.h"
#include "src/__support/CPP/string_view.h"
#include "src/__support/CPP/stringstream.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/properties/architectures.h"

// TODO: fix this unguarded linux dep
#include <linux/param.h> // for exec_pagesize.

#include <stddef.h> // For size_t

namespace LIBC_NAMESPACE_DECL {

using ThreadRunnerPosix = void *(void *);
using ThreadRunnerStdc = int(void *);

union ThreadRunner {
  ThreadRunnerPosix *posix_runner;
  ThreadRunnerStdc *stdc_runner;
};

union ThreadReturnValue {
  void *posix_retval;
  int stdc_retval;
  constexpr ThreadReturnValue() : posix_retval(nullptr) {}
  constexpr ThreadReturnValue(int r) : stdc_retval(r) {}
  constexpr ThreadReturnValue(void *r) : posix_retval(r) {}
};

#if (defined(LIBC_TARGET_ARCH_IS_AARCH64) ||                                   \
     defined(LIBC_TARGET_ARCH_IS_X86_64) ||                                    \
     defined(LIBC_TARGET_ARCH_IS_ANY_RISCV))
constexpr unsigned int STACK_ALIGNMENT = 16;
#elif defined(LIBC_TARGET_ARCH_IS_ARM)
// See Section 6.2.1.2 Stack constraints at a public interface of AAPCS32.
constexpr unsigned int STACK_ALIGNMENT = 8;
#endif
// TODO: Provide stack alignment requirements for other architectures.

enum class DetachState : uint32_t {
  JOINABLE = 0x11,
  EXITING = 0x22,
  DETACHED = 0x33
};

enum class ThreadStyle : uint8_t { POSIX = 0x1, STDC = 0x2 };

// Detach type is useful in testing the detach operation.
enum class DetachType : int {
  // Indicates that the detach operation just set the detach state to DETACHED
  // and returned.
  SIMPLE = 1,

  // Indicates that the detach operation performed thread cleanup.
  CLEANUP = 2
};

class ThreadAtExitCallbackMgr;

// A data type to hold common thread attributes which have to be stored as
// thread state. Note that this is different from public attribute types like
// pthread_attr_t which might contain information which need not be saved as
// part of a thread's state. For example, the stack guard size.
//
// Thread attributes are typically stored on the stack. So, we align as required
// for the target architecture.
struct alignas(STACK_ALIGNMENT) ThreadAttributes {
  // We want the "detach_state" attribute to be an atomic value as it could be
  // updated by one thread while the self thread is reading it. It is a tristate
  // variable with the following state transitions:
  // 1. The a thread is created in a detached state, then user code should never
  //    call a detach or join function. Calling either of them can lead to
  //    undefined behavior.
  //    The value of |detach_state| is expected to be DetachState::DETACHED for
  //    its lifetime.
  // 2. If a thread is created in a joinable state, |detach_state| will start
  //    with the value DetachState::JOINABLE. Another thread can detach this
  //    thread before it exits. The state transitions will as follows:
  //      (a) If the detach method sees the state as JOINABLE, then it will
  //          compare exchange to a state of DETACHED. The thread will clean
  //          itself up after it finishes.
  //      (b) If the detach method does not see JOINABLE in (a), then it will
  //          conclude that the thread is EXITING and will wait until the thread
  //          exits. It will clean up the thread resources once the thread
  //          exits.
  cpp::Atomic<uint32_t> detach_state;
  void *stack;               // Pointer to the thread stack
  size_t stacksize;          // Size of the stack
  size_t guardsize;          // Guard size on stack
  uintptr_t tls;             // Address to the thread TLS memory
  uintptr_t tls_size;        // The size of area pointed to by |tls|.
  unsigned char owned_stack; // Indicates if the thread owns this stack memory
  int tid;
  ThreadStyle style;
  ThreadReturnValue retval;
  ThreadAtExitCallbackMgr *atexit_callback_mgr;
  void *platform_data;
  cpp::Atomic<ThreadAttributes *> joiner;

  LIBC_INLINE constexpr ThreadAttributes()
      : detach_state(uint32_t(DetachState::DETACHED)), stack(nullptr),
        stacksize(0), guardsize(0), tls(0), tls_size(0), owned_stack(false),
        tid(-1), style(ThreadStyle::POSIX), retval(),
        atexit_callback_mgr(nullptr), platform_data(nullptr), joiner(nullptr) {}
};

using TSSDtor = void(void *);

// Create a new TSS key and associate the |dtor| as the corresponding
// destructor. Can be used to implement public functions like
// pthread_key_create.
cpp::optional<unsigned int> new_tss_key(TSSDtor *dtor);

// Delete the |key|. Can be used to implement public functions like
// pthread_key_delete.
//
// Return true on success, false on failure.
bool tss_key_delete(unsigned int key);

// Set the value associated with |key| for the current thread. Can be used
// to implement public functions like pthread_setspecific.
//
// Return true on success, false on failure.
bool set_tss_value(unsigned int key, void *value);

// Return the value associated with |key| for the current thread. Return
// nullptr if |key| is invalid. Can be used to implement public functions like
// pthread_getspecific.
void *get_tss_value(unsigned int key);

struct Thread {
  // NB: Default stacksize of 64kb is exceedingly small compared to the 2mb norm
  // and will break many programs expecting the full 2mb.
  static constexpr size_t DEFAULT_STACKSIZE = 1 << 16;
  static constexpr size_t DEFAULT_GUARDSIZE = EXEC_PAGESIZE;
  static constexpr bool DEFAULT_DETACHED = false;

  ThreadAttributes *attrib;

  constexpr Thread() : attrib(nullptr) {}
  constexpr Thread(ThreadAttributes *attr) : attrib(attr) {}

  int run(ThreadRunnerPosix *func, void *arg, void *stack = nullptr,
          size_t stacksize = DEFAULT_STACKSIZE,
          size_t guardsize = DEFAULT_GUARDSIZE,
          bool detached = DEFAULT_DETACHED) {
    ThreadRunner runner;
    runner.posix_runner = func;
    return run(ThreadStyle::POSIX, runner, arg, stack, stacksize, guardsize,
               detached);
  }

  int run(ThreadRunnerStdc *func, void *arg, void *stack = nullptr,
          size_t stacksize = DEFAULT_STACKSIZE,
          size_t guardsize = DEFAULT_GUARDSIZE,
          bool detached = DEFAULT_DETACHED) {
    ThreadRunner runner;
    runner.stdc_runner = func;
    return run(ThreadStyle::STDC, runner, arg, stack, stacksize, guardsize,
               detached);
  }

  int join(int *val) {
    ThreadReturnValue retval;
    int status = join(retval);
    if (status != 0)
      return status;
    if (val != nullptr)
      *val = retval.stdc_retval;
    return 0;
  }

  int join(void **val) {
    ThreadReturnValue retval;
    int status = join(retval);
    if (status != 0)
      return status;
    if (val != nullptr)
      *val = retval.posix_retval;
    return 0;
  }

  // Platform should implement the functions below.

  // Return 0 on success or an error value on failure.
  int run(ThreadStyle style, ThreadRunner runner, void *arg, void *stack,
          size_t stacksize, size_t guardsize, bool detached);

  // Return 0 on success or an error value on failure.
  int join(ThreadReturnValue &retval);

  // Detach a joinable thread.
  //
  // This method does not have error return value. However, the type of detach
  // is returned to help with testing.
  int detach();

  // Wait for the thread to finish. This method can only be called
  // if:
  // 1. A detached thread is guaranteed to be running.
  // 2. A joinable thread has not been detached or joined. As long as it has
  //    not been detached or joined, wait can be called multiple times.
  //
  // Also, only one thread can wait and expect to get woken up when the thread
  // finishes.
  //
  // NOTE: This function is to be used for testing only. There is no standard
  // which requires exposing it via a public API.
  void wait();

  // Return true if this thread is equal to the other thread.
  bool operator==(const Thread &other) const;

  // Set the name of the thread. Return the error number on error.
  int set_name(const cpp::string_view &name);

  // Return the name of the thread in |name|. Return the error number of error.
  int get_name(cpp::StringStream &name) const;
};

LIBC_INLINE_VAR LIBC_THREAD_LOCAL Thread self;

// Platforms should implement this function.
[[noreturn]] void thread_exit(ThreadReturnValue retval, ThreadStyle style);

namespace internal {
// Internal namespace containing utilities which are to be used by platform
// implementations of threads.

// Return the current thread's atexit callback manager. After thread startup
// but before running the thread function, platform implementations should
// set the "atexit_callback_mgr" field of the thread's attributes to the value
// returned by this function.
ThreadAtExitCallbackMgr *get_thread_atexit_callback_mgr();

// Call the currently registered thread specific atexit callbacks. Useful for
// implementing the thread_exit function.
void call_atexit_callbacks(ThreadAttributes *attrib);

} // namespace internal

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_THREADS_THREAD_H
