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

#include "once.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <sched.h>
#endif

#include "atomicops.h"
#include "checks.h"

namespace v8 {
namespace internal {

void CallOnceImpl(OnceType* once, PointerArgFunction init_func, void* arg) {
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
    init_func(arg);
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

} }  // namespace v8::internal
