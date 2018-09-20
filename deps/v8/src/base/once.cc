// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/once.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <sched.h>
#endif

#include "src/base/atomicops.h"

namespace v8 {
namespace base {

void CallOnceImpl(OnceType* once, std::function<void()> init_func) {
  AtomicWord state = Acquire_Load(once);
  // Fast path. The provided function was already executed.
  if (state == ONCE_STATE_DONE) {
    return;
  }

  // The function execution did not complete yet. The once object can be in one
  // of the two following states:
  //   - UNINITIALIZED: We are the first thread calling this function.
  //   - EXECUTING_FUNCTION: Another thread is already executing the function.
  //
  // First, try to change the state from UNINITIALIZED to EXECUTING_FUNCTION
  // atomically.
  state = Acquire_CompareAndSwap(
      once, ONCE_STATE_UNINITIALIZED, ONCE_STATE_EXECUTING_FUNCTION);
  if (state == ONCE_STATE_UNINITIALIZED) {
    // We are the first thread to call this function, so we have to call the
    // function.
    init_func();
    Release_Store(once, ONCE_STATE_DONE);
  } else {
    // Another thread has already started executing the function. We need to
    // wait until it completes the initialization.
    while (state == ONCE_STATE_EXECUTING_FUNCTION) {
#ifdef _WIN32
      ::Sleep(0);
#else
      sched_yield();
#endif
      state = Acquire_Load(once);
    }
  }
}

}  // namespace base
}  // namespace v8
