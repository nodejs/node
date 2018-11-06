// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/platform/semaphore.h"

#if V8_OS_MACOSX
#include <mach/mach_init.h>
#include <mach/task.h>
#endif

#include <errno.h>

#include "src/base/logging.h"
#include "src/base/platform/elapsed-timer.h"
#include "src/base/platform/time.h"

namespace v8 {
namespace base {

#if V8_OS_MACOSX

Semaphore::Semaphore(int count) {
  kern_return_t result = semaphore_create(
      mach_task_self(), &native_handle_, SYNC_POLICY_FIFO, count);
  DCHECK_EQ(KERN_SUCCESS, result);
  USE(result);
}


Semaphore::~Semaphore() {
  kern_return_t result = semaphore_destroy(mach_task_self(), native_handle_);
  DCHECK_EQ(KERN_SUCCESS, result);
  USE(result);
}

void Semaphore::Signal() {
  kern_return_t result = semaphore_signal(native_handle_);
  DCHECK_EQ(KERN_SUCCESS, result);
  USE(result);
}


void Semaphore::Wait() {
  while (true) {
    kern_return_t result = semaphore_wait(native_handle_);
    if (result == KERN_SUCCESS) return;  // Semaphore was signalled.
    DCHECK_EQ(KERN_ABORTED, result);
  }
}


bool Semaphore::WaitFor(const TimeDelta& rel_time) {
  TimeTicks now = TimeTicks::Now();
  TimeTicks end = now + rel_time;
  while (true) {
    mach_timespec_t ts;
    if (now >= end) {
      // Return immediately if semaphore was not signalled.
      ts.tv_sec = 0;
      ts.tv_nsec = 0;
    } else {
      ts = (end - now).ToMachTimespec();
    }
    kern_return_t result = semaphore_timedwait(native_handle_, ts);
    if (result == KERN_SUCCESS) return true;  // Semaphore was signalled.
    if (result == KERN_OPERATION_TIMED_OUT) return false;  // Timeout.
    DCHECK_EQ(KERN_ABORTED, result);
    now = TimeTicks::Now();
  }
}

#elif V8_OS_POSIX

Semaphore::Semaphore(int count) {
  DCHECK_GE(count, 0);
  int result = sem_init(&native_handle_, 0, count);
  DCHECK_EQ(0, result);
  USE(result);
}


Semaphore::~Semaphore() {
  int result = sem_destroy(&native_handle_);
  DCHECK_EQ(0, result);
  USE(result);
}

void Semaphore::Signal() {
  int result = sem_post(&native_handle_);
  // This check may fail with <libc-2.21, which we use on the try bots, if the
  // semaphore is destroyed while sem_post is still executed. A work around is
  // to extend the lifetime of the semaphore.
  if (result != 0) {
    FATAL("Error when signaling semaphore, errno: %d", errno);
  }
}


void Semaphore::Wait() {
  while (true) {
    int result = sem_wait(&native_handle_);
    if (result == 0) return;  // Semaphore was signalled.
    // Signal caused spurious wakeup.
    DCHECK_EQ(-1, result);
    DCHECK_EQ(EINTR, errno);
  }
}


bool Semaphore::WaitFor(const TimeDelta& rel_time) {
  // Compute the time for end of timeout.
  const Time time = Time::NowFromSystemTime() + rel_time;
  const struct timespec ts = time.ToTimespec();

  // Wait for semaphore signalled or timeout.
  while (true) {
    int result = sem_timedwait(&native_handle_, &ts);
    if (result == 0) return true;  // Semaphore was signalled.
#if V8_LIBC_GLIBC && !V8_GLIBC_PREREQ(2, 4)
    if (result > 0) {
      // sem_timedwait in glibc prior to 2.3.4 returns the errno instead of -1.
      errno = result;
      result = -1;
    }
#endif
    if (result == -1 && errno == ETIMEDOUT) {
      // Timed out while waiting for semaphore.
      return false;
    }
    // Signal caused spurious wakeup.
    DCHECK_EQ(-1, result);
    DCHECK_EQ(EINTR, errno);
  }
}

#elif V8_OS_WIN

Semaphore::Semaphore(int count) {
  DCHECK_GE(count, 0);
  native_handle_ = ::CreateSemaphoreA(nullptr, count, 0x7FFFFFFF, nullptr);
  DCHECK_NOT_NULL(native_handle_);
}


Semaphore::~Semaphore() {
  BOOL result = CloseHandle(native_handle_);
  DCHECK(result);
  USE(result);
}

void Semaphore::Signal() {
  LONG dummy;
  BOOL result = ReleaseSemaphore(native_handle_, 1, &dummy);
  DCHECK(result);
  USE(result);
}


void Semaphore::Wait() {
  DWORD result = WaitForSingleObject(native_handle_, INFINITE);
  DCHECK(result == WAIT_OBJECT_0);
  USE(result);
}


bool Semaphore::WaitFor(const TimeDelta& rel_time) {
  TimeTicks now = TimeTicks::Now();
  TimeTicks end = now + rel_time;
  while (true) {
    int64_t msec = (end - now).InMilliseconds();
    if (msec >= static_cast<int64_t>(INFINITE)) {
      DWORD result = WaitForSingleObject(native_handle_, INFINITE - 1);
      if (result == WAIT_OBJECT_0) {
        return true;
      }
      DCHECK(result == WAIT_TIMEOUT);
      now = TimeTicks::Now();
    } else {
      DWORD result = WaitForSingleObject(
          native_handle_, (msec < 0) ? 0 : static_cast<DWORD>(msec));
      if (result == WAIT_TIMEOUT) {
        return false;
      }
      DCHECK(result == WAIT_OBJECT_0);
      return true;
    }
  }
}

#endif  // V8_OS_MACOSX

}  // namespace base
}  // namespace v8
