// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/once.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <sched.h>
#endif

namespace v8 {
namespace base {

void CallOnceImpl(OnceType* once, std::function<void()> init_func) {
  // Fast path. The provided function was already executed.
  if (once->load(std::memory_order_acquire) == ONCE_STATE_DONE) {
    return;
  }

  // The function execution did not complete yet. The once object can be in one
  // of the two following states:
  //   - UNINITIALIZED: We are the first thread calling this function.
  //   - EXECUTING_FUNCTION: Another thread is already executing the function.
  //
  // First, try to change the state from UNINITIALIZED to EXECUTING_FUNCTION
  // atomically.
  uint8_t expected = ONCE_STATE_UNINITIALIZED;
  if (once->compare_exchange_strong(expected, ONCE_STATE_EXECUTING_FUNCTION,
                                    std::memory_order_acq_rel)) {
    // We are the first thread to call this function, so we have to call the
    // function.
    init_func();
    once->store(ONCE_STATE_DONE, std::memory_order_release);
  } else {
    // Another thread has already started executing the function. We need to
    // wait until it completes the initialization.
    while (once->load(std::memory_order_acquire) ==
           ONCE_STATE_EXECUTING_FUNCTION) {
#ifdef _WIN32
      ::Sleep(0);
#else
      sched_yield();
#endif
    }
  }
}

}  // namespace base
}  // namespace v8
