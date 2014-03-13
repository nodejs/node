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

#include "platform/condition-variable.h"

#include <errno.h>
#include <time.h>

#include "platform/time.h"

namespace v8 {
namespace internal {

#if V8_OS_POSIX

ConditionVariable::ConditionVariable() {
  // TODO(bmeurer): The test for V8_LIBRT_NOT_AVAILABLE is a temporary
  // hack to support cross-compiling Chrome for Android in AOSP. Remove
  // this once AOSP is fixed.
#if (V8_OS_FREEBSD || V8_OS_NETBSD || V8_OS_OPENBSD || \
     (V8_OS_LINUX && V8_LIBC_GLIBC)) && !V8_LIBRT_NOT_AVAILABLE
  // On Free/Net/OpenBSD and Linux with glibc we can change the time
  // source for pthread_cond_timedwait() to use the monotonic clock.
  pthread_condattr_t attr;
  int result = pthread_condattr_init(&attr);
  ASSERT_EQ(0, result);
  result = pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
  ASSERT_EQ(0, result);
  result = pthread_cond_init(&native_handle_, &attr);
  ASSERT_EQ(0, result);
  result = pthread_condattr_destroy(&attr);
#else
  int result = pthread_cond_init(&native_handle_, NULL);
#endif
  ASSERT_EQ(0, result);
  USE(result);
}


ConditionVariable::~ConditionVariable() {
  int result = pthread_cond_destroy(&native_handle_);
  ASSERT_EQ(0, result);
  USE(result);
}


void ConditionVariable::NotifyOne() {
  int result = pthread_cond_signal(&native_handle_);
  ASSERT_EQ(0, result);
  USE(result);
}


void ConditionVariable::NotifyAll() {
  int result = pthread_cond_broadcast(&native_handle_);
  ASSERT_EQ(0, result);
  USE(result);
}


void ConditionVariable::Wait(Mutex* mutex) {
  mutex->AssertHeldAndUnmark();
  int result = pthread_cond_wait(&native_handle_, &mutex->native_handle());
  ASSERT_EQ(0, result);
  USE(result);
  mutex->AssertUnheldAndMark();
}


bool ConditionVariable::WaitFor(Mutex* mutex, const TimeDelta& rel_time) {
  struct timespec ts;
  int result;
  mutex->AssertHeldAndUnmark();
#if V8_OS_MACOSX
  // Mac OS X provides pthread_cond_timedwait_relative_np(), which does
  // not depend on the real time clock, which is what you really WANT here!
  ts = rel_time.ToTimespec();
  ASSERT_GE(ts.tv_sec, 0);
  ASSERT_GE(ts.tv_nsec, 0);
  result = pthread_cond_timedwait_relative_np(
      &native_handle_, &mutex->native_handle(), &ts);
#else
  // TODO(bmeurer): The test for V8_LIBRT_NOT_AVAILABLE is a temporary
  // hack to support cross-compiling Chrome for Android in AOSP. Remove
  // this once AOSP is fixed.
#if (V8_OS_FREEBSD || V8_OS_NETBSD || V8_OS_OPENBSD || \
     (V8_OS_LINUX && V8_LIBC_GLIBC)) && !V8_LIBRT_NOT_AVAILABLE
  // On Free/Net/OpenBSD and Linux with glibc we can change the time
  // source for pthread_cond_timedwait() to use the monotonic clock.
  result = clock_gettime(CLOCK_MONOTONIC, &ts);
  ASSERT_EQ(0, result);
  Time now = Time::FromTimespec(ts);
#else
  // The timeout argument to pthread_cond_timedwait() is in absolute time.
  Time now = Time::NowFromSystemTime();
#endif
  Time end_time = now + rel_time;
  ASSERT_GE(end_time, now);
  ts = end_time.ToTimespec();
  result = pthread_cond_timedwait(
      &native_handle_, &mutex->native_handle(), &ts);
#endif  // V8_OS_MACOSX
  mutex->AssertUnheldAndMark();
  if (result == ETIMEDOUT) {
    return false;
  }
  ASSERT_EQ(0, result);
  return true;
}

#elif V8_OS_WIN

struct ConditionVariable::Event {
  Event() : handle_(::CreateEventA(NULL, true, false, NULL)) {
    ASSERT(handle_ != NULL);
  }

  ~Event() {
    BOOL ok = ::CloseHandle(handle_);
    ASSERT(ok);
    USE(ok);
  }

  bool WaitFor(DWORD timeout_ms) {
    DWORD result = ::WaitForSingleObject(handle_, timeout_ms);
    if (result == WAIT_OBJECT_0) {
      return true;
    }
    ASSERT(result == WAIT_TIMEOUT);
    return false;
  }

  HANDLE handle_;
  Event* next_;
  HANDLE thread_;
  volatile bool notified_;
};


ConditionVariable::NativeHandle::~NativeHandle() {
  ASSERT(waitlist_ == NULL);

  while (freelist_ != NULL) {
    Event* event = freelist_;
    freelist_ = event->next_;
    delete event;
  }
}


ConditionVariable::Event* ConditionVariable::NativeHandle::Pre() {
  LockGuard<Mutex> lock_guard(&mutex_);

  // Grab an event from the free list or create a new one.
  Event* event = freelist_;
  if (event != NULL) {
    freelist_ = event->next_;
  } else {
    event = new Event;
  }
  event->thread_ = GetCurrentThread();
  event->notified_ = false;

#ifdef DEBUG
  // The event must not be on the wait list.
  for (Event* we = waitlist_; we != NULL; we = we->next_) {
    ASSERT_NE(event, we);
  }
#endif

  // Prepend the event to the wait list.
  event->next_ = waitlist_;
  waitlist_ = event;

  return event;
}


void ConditionVariable::NativeHandle::Post(Event* event, bool result) {
  LockGuard<Mutex> lock_guard(&mutex_);

  // Remove the event from the wait list.
  for (Event** wep = &waitlist_;; wep = &(*wep)->next_) {
    ASSERT_NE(NULL, *wep);
    if (*wep == event) {
      *wep = event->next_;
      break;
    }
  }

#ifdef DEBUG
  // The event must not be on the free list.
  for (Event* fe = freelist_; fe != NULL; fe = fe->next_) {
    ASSERT_NE(event, fe);
  }
#endif

  // Reset the event.
  BOOL ok = ::ResetEvent(event->handle_);
  ASSERT(ok);
  USE(ok);

  // Insert the event into the free list.
  event->next_ = freelist_;
  freelist_ = event;

  // Forward signals delivered after the timeout to the next waiting event.
  if (!result && event->notified_ && waitlist_ != NULL) {
    ok = ::SetEvent(waitlist_->handle_);
    ASSERT(ok);
    USE(ok);
    waitlist_->notified_ = true;
  }
}


ConditionVariable::ConditionVariable() {}


ConditionVariable::~ConditionVariable() {}


void ConditionVariable::NotifyOne() {
  // Notify the thread with the highest priority in the waitlist
  // that was not already signalled.
  LockGuard<Mutex> lock_guard(native_handle_.mutex());
  Event* highest_event = NULL;
  int highest_priority = std::numeric_limits<int>::min();
  for (Event* event = native_handle().waitlist();
       event != NULL;
       event = event->next_) {
    if (event->notified_) {
      continue;
    }
    int priority = GetThreadPriority(event->thread_);
    ASSERT_NE(THREAD_PRIORITY_ERROR_RETURN, priority);
    if (priority >= highest_priority) {
      highest_priority = priority;
      highest_event = event;
    }
  }
  if (highest_event != NULL) {
    ASSERT(!highest_event->notified_);
    ::SetEvent(highest_event->handle_);
    highest_event->notified_ = true;
  }
}


void ConditionVariable::NotifyAll() {
  // Notify all threads on the waitlist.
  LockGuard<Mutex> lock_guard(native_handle_.mutex());
  for (Event* event = native_handle().waitlist();
       event != NULL;
       event = event->next_) {
    if (!event->notified_) {
      ::SetEvent(event->handle_);
      event->notified_ = true;
    }
  }
}


void ConditionVariable::Wait(Mutex* mutex) {
  // Create and setup the wait event.
  Event* event = native_handle_.Pre();

  // Release the user mutex.
  mutex->Unlock();

  // Wait on the wait event.
  while (!event->WaitFor(INFINITE))
    ;

  // Reaquire the user mutex.
  mutex->Lock();

  // Release the wait event (we must have been notified).
  ASSERT(event->notified_);
  native_handle_.Post(event, true);
}


bool ConditionVariable::WaitFor(Mutex* mutex, const TimeDelta& rel_time) {
  // Create and setup the wait event.
  Event* event = native_handle_.Pre();

  // Release the user mutex.
  mutex->Unlock();

  // Wait on the wait event.
  TimeTicks now = TimeTicks::Now();
  TimeTicks end = now + rel_time;
  bool result = false;
  while (true) {
    int64_t msec = (end - now).InMilliseconds();
    if (msec >= static_cast<int64_t>(INFINITE)) {
      result = event->WaitFor(INFINITE - 1);
      if (result) {
        break;
      }
      now = TimeTicks::Now();
    } else {
      result = event->WaitFor((msec < 0) ? 0 : static_cast<DWORD>(msec));
      break;
    }
  }

  // Reaquire the user mutex.
  mutex->Lock();

  // Release the wait event.
  ASSERT(!result || event->notified_);
  native_handle_.Post(event, result);

  return result;
}

#endif  // V8_OS_POSIX

} }  // namespace v8::internal
