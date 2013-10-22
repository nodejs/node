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

#ifndef V8_PLATFORM_CONDITION_VARIABLE_H_
#define V8_PLATFORM_CONDITION_VARIABLE_H_

#include "platform/mutex.h"

namespace v8 {
namespace internal {

// Forward declarations.
class ConditionVariableEvent;
class TimeDelta;

// -----------------------------------------------------------------------------
// ConditionVariable
//
// This class is a synchronization primitive that can be used to block a thread,
// or multiple threads at the same time, until:
// - a notification is received from another thread,
// - a timeout expires, or
// - a spurious wakeup occurs
// Any thread that intends to wait on a ConditionVariable has to acquire a lock
// on a Mutex first. The |Wait()| and |WaitFor()| operations atomically release
// the mutex and suspend the execution of the calling thread. When the condition
// variable is notified, the thread is awakened, and the mutex is reacquired.

class ConditionVariable V8_FINAL {
 public:
  ConditionVariable();
  ~ConditionVariable();

  // If any threads are waiting on this condition variable, calling
  // |NotifyOne()| unblocks one of the waiting threads.
  void NotifyOne();

  // Unblocks all threads currently waiting for this condition variable.
  void NotifyAll();

  // |Wait()| causes the calling thread to block until the condition variable is
  // notified or a spurious wakeup occurs. Atomically releases the mutex, blocks
  // the current executing thread, and adds it to the list of threads waiting on
  // this condition variable. The thread will be unblocked when |NotifyAll()| or
  // |NotifyOne()| is executed. It may also be unblocked spuriously. When
  // unblocked, regardless of the reason, the lock on the mutex is reacquired
  // and |Wait()| exits.
  void Wait(Mutex* mutex);

  // Atomically releases the mutex, blocks the current executing thread, and
  // adds it to the list of threads waiting on this condition variable. The
  // thread will be unblocked when |NotifyAll()| or |NotifyOne()| is executed,
  // or when the relative timeout |rel_time| expires. It may also be unblocked
  // spuriously. When unblocked, regardless of the reason, the lock on the mutex
  // is reacquired and |WaitFor()| exits. Returns true if the condition variable
  // was notified prior to the timeout.
  bool WaitFor(Mutex* mutex, const TimeDelta& rel_time) V8_WARN_UNUSED_RESULT;

  // The implementation-defined native handle type.
#if V8_OS_POSIX
  typedef pthread_cond_t NativeHandle;
#elif V8_OS_WIN
  struct Event;
  class NativeHandle V8_FINAL {
   public:
    NativeHandle() : waitlist_(NULL), freelist_(NULL) {}
    ~NativeHandle();

    Event* Pre() V8_WARN_UNUSED_RESULT;
    void Post(Event* event, bool result);

    Mutex* mutex() { return &mutex_; }
    Event* waitlist() { return waitlist_; }

   private:
    Event* waitlist_;
    Event* freelist_;
    Mutex mutex_;

    DISALLOW_COPY_AND_ASSIGN(NativeHandle);
  };
#endif

  NativeHandle& native_handle() {
    return native_handle_;
  }
  const NativeHandle& native_handle() const {
    return native_handle_;
  }

 private:
  NativeHandle native_handle_;

  DISALLOW_COPY_AND_ASSIGN(ConditionVariable);
};


// POD ConditionVariable initialized lazily (i.e. the first time Pointer() is
// called).
// Usage:
//   static LazyConditionVariable my_condvar =
//       LAZY_CONDITION_VARIABLE_INITIALIZER;
//
//   void my_function() {
//     LockGuard<Mutex> lock_guard(&my_mutex);
//     my_condvar.Pointer()->Wait(&my_mutex);
//   }
typedef LazyStaticInstance<ConditionVariable,
                           DefaultConstructTrait<ConditionVariable>,
                           ThreadSafeInitOnceTrait>::type LazyConditionVariable;

#define LAZY_CONDITION_VARIABLE_INITIALIZER LAZY_STATIC_INSTANCE_INITIALIZER

} }  // namespace v8::internal

#endif  // V8_PLATFORM_CONDITION_VARIABLE_H_
