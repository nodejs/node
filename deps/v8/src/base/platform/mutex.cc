// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/platform/mutex.h"

#include "src/base/platform/platform.h"

namespace v8 {
namespace base {

RecursiveMutex::~RecursiveMutex() {
  DCHECK_EQ(0, level_);
}

void RecursiveMutex::Lock() {
  int own_id = v8::base::OS::GetCurrentThreadId();
  if (thread_id_ == own_id) {
    level_++;
    return;
  }
  mutex_.Lock();
  DCHECK_EQ(0, level_);
  thread_id_ = own_id;
  level_ = 1;
}

void RecursiveMutex::Unlock() {
#ifdef DEBUG
  int own_id = v8::base::OS::GetCurrentThreadId();
  CHECK_EQ(thread_id_, own_id);
#endif
  if ((--level_) == 0) {
    thread_id_ = 0;
    mutex_.Unlock();
  }
}

bool RecursiveMutex::TryLock() {
  int own_id = v8::base::OS::GetCurrentThreadId();
  if (thread_id_ == own_id) {
    level_++;
    return true;
  }
  if (mutex_.TryLock()) {
    DCHECK_EQ(0, level_);
    thread_id_ = own_id;
    level_ = 1;
    return true;
  }
  return false;
}

Mutex::Mutex() {
#ifdef DEBUG
  level_ = 0;
#endif
}

Mutex::~Mutex() { DCHECK_EQ(0, level_); }

void Mutex::Lock() ABSL_NO_THREAD_SAFETY_ANALYSIS {
  native_handle_.Lock();
  AssertUnheldAndMark();
}

void Mutex::Unlock() ABSL_NO_THREAD_SAFETY_ANALYSIS {
  AssertHeldAndUnmark();
  native_handle_.Unlock();
}

bool Mutex::TryLock() ABSL_NO_THREAD_SAFETY_ANALYSIS {
  if (!native_handle_.TryLock()) return false;
  AssertUnheldAndMark();
  return true;
}

}  // namespace base
}  // namespace v8
