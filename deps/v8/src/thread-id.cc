// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/thread-id.h"
#include "src/base/lazy-instance.h"
#include "src/base/platform/platform.h"

namespace v8 {
namespace internal {

base::Atomic32 ThreadId::highest_thread_id_ = 0;

namespace {

DEFINE_LAZY_LEAKY_OBJECT_GETTER(base::Thread::LocalStorageKey, GetThreadIdKey,
                                base::Thread::CreateThreadLocalKey());

}  // namespace

// static
ThreadId ThreadId::TryGetCurrent() {
  int thread_id = base::Thread::GetThreadLocalInt(*GetThreadIdKey());
  return thread_id == 0 ? Invalid() : ThreadId(thread_id);
}

// static
int ThreadId::GetCurrentThreadId() {
  int thread_id = base::Thread::GetThreadLocalInt(*GetThreadIdKey());
  if (thread_id == 0) {
    thread_id = AllocateThreadId();
    base::Thread::SetThreadLocalInt(*GetThreadIdKey(), thread_id);
  }
  return thread_id;
}

}  // namespace internal
}  // namespace v8
