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
#include "hdr/types/struct_sched_param.h"
#include "src/__support/CPP/atomic.h"
#include "src/__support/CPP/optional.h"
#include "src/__support/CPP/string_view.h"
#include "src/__support/CPP/stringstream.h"
#include "src/__support/error_or.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/properties/architectures.h"
#include "src/__support/threads/thread_attributes.h"

// TODO: fix this unguarded linux dep
#include <linux/param.h> // for exec_pagesize.

namespace LIBC_NAMESPACE_DECL {

struct SchedParameters {
  int policy;
  struct sched_param param;
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

  // Set the scheduling policy and parameters of the thread.
  // Return 0 on success, or an error number on failure.
  int setschedparam(SchedParameters params);

  // Get the scheduling policy and parameters of the thread.
  // Return SchedParameters on success, or an error number on failure.
  ErrorOr<SchedParameters> getschedparam() const;
};

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

LIBC_INLINE_VAR LIBC_THREAD_LOCAL Thread self;

} // namespace internal

LIBC_INLINE Thread current_thread() { return internal::self; }

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_THREADS_THREAD_H
