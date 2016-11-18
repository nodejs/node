// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BASE_PLATFORM_SEMAPHORE_H_
#define V8_BASE_PLATFORM_SEMAPHORE_H_

#include "src/base/lazy-instance.h"
#if V8_OS_WIN
#include "src/base/win32-headers.h"
#endif

#if V8_OS_MACOSX
#include <mach/semaphore.h>  // NOLINT
#elif V8_OS_POSIX
#include <semaphore.h>  // NOLINT
#endif

namespace v8 {
namespace base {

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

class Semaphore final {
 public:
  explicit Semaphore(int count);
  ~Semaphore();

  // Increments the semaphore counter.
  void Signal();

  // Decrements the semaphore counter if it is positive, or blocks until it
  // becomes positive and then decrements the counter.
  void Wait();

  // Like Wait() but returns after rel_time time has passed. If the timeout
  // happens the return value is false and the counter is unchanged. Otherwise
  // the semaphore counter is decremented and true is returned.
  bool WaitFor(const TimeDelta& rel_time) WARN_UNUSED_RESULT;

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
  typedef typename LazyDynamicInstance<Semaphore, CreateSemaphoreTrait<N>,
                                       ThreadSafeInitOnceTrait>::type type;
};

#define LAZY_SEMAPHORE_INITIALIZER LAZY_DYNAMIC_INSTANCE_INITIALIZER

}  // namespace base
}  // namespace v8

#endif  // V8_BASE_PLATFORM_SEMAPHORE_H_
