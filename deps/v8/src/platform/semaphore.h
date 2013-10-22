// Copyright 2013 the V8 project authors. All rights reserved.
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

#ifndef V8_PLATFORM_SEMAPHORE_H_
#define V8_PLATFORM_SEMAPHORE_H_

#include "lazy-instance.h"
#if V8_OS_WIN
#include "win32-headers.h"
#endif

#if V8_OS_MACOSX
#include <mach/semaphore.h>  // NOLINT
#elif V8_OS_POSIX
#include <semaphore.h>  // NOLINT
#endif

namespace v8 {
namespace internal {

// Forward declarations.
class TimeDelta;

// ----------------------------------------------------------------------------
// Semaphore
//
// A semaphore object is a synchronization object that maintains a count. The
// count is decremented each time a thread completes a wait for the semaphore
// object and incremented each time a thread signals the semaphore. When the
// count reaches zero,  threads waiting for the semaphore blocks until the
// count becomes non-zero.

class Semaphore V8_FINAL {
 public:
  explicit Semaphore(int count);
  ~Semaphore();

  // Increments the semaphore counter.
  void Signal();

  // Suspends the calling thread until the semaphore counter is non zero
  // and then decrements the semaphore counter.
  void Wait();

  // Suspends the calling thread until the counter is non zero or the timeout
  // time has passed. If timeout happens the return value is false and the
  // counter is unchanged. Otherwise the semaphore counter is decremented and
  // true is returned.
  bool WaitFor(const TimeDelta& rel_time) V8_WARN_UNUSED_RESULT;

#if V8_OS_MACOSX
  typedef semaphore_t NativeHandle;
#elif V8_OS_POSIX
  typedef sem_t NativeHandle;
#elif V8_OS_WIN
  typedef HANDLE NativeHandle;
#endif

  NativeHandle& native_handle() {
    return native_handle_;
  }
  const NativeHandle& native_handle() const {
    return native_handle_;
  }

 private:
  NativeHandle native_handle_;

  DISALLOW_COPY_AND_ASSIGN(Semaphore);
};


// POD Semaphore initialized lazily (i.e. the first time Pointer() is called).
// Usage:
//   // The following semaphore starts at 0.
//   static LazySemaphore<0>::type my_semaphore = LAZY_SEMAPHORE_INITIALIZER;
//
//   void my_function() {
//     // Do something with my_semaphore.Pointer().
//   }
//

template <int N>
struct CreateSemaphoreTrait {
  static Semaphore* Create() {
    return new Semaphore(N);
  }
};

template <int N>
struct LazySemaphore {
  typedef typename LazyDynamicInstance<
      Semaphore,
      CreateSemaphoreTrait<N>,
      ThreadSafeInitOnceTrait>::type type;
};

#define LAZY_SEMAPHORE_INITIALIZER LAZY_DYNAMIC_INSTANCE_INITIALIZER

} }  // namespace v8::internal

#endif  // V8_PLATFORM_SEMAPHORE_H_
