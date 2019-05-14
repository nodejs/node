// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/thread-id.h"
#include "src/base/lazy-instance.h"
#include "src/base/platform/platform.h"

namespace v8 {
namespace internal {

namespace {

DEFINE_LAZY_LEAKY_OBJECT_GETTER(base::Thread::LocalStorageKey, GetThreadIdKey,
                                base::Thread::CreateThreadLocalKey())

std::atomic<int> next_thread_id{1};

}  // namespace

// static
ThreadId ThreadId::TryGetCurrent() {
  int thread_id = base::Thread::GetThreadLocalInt(*GetThreadIdKey());
  return thread_id == 0 ? Invalid() : ThreadId(thread_id);
}

// static
int ThreadId::GetCurrentThreadId() {
  auto key = *GetThreadIdKey();
  int thread_id = base::Thread::GetThreadLocalInt(key);
  if (thread_id == 0) {
    thread_id = next_thread_id.fetch_add(1);
    CHECK_LE(1, thread_id);
    base::Thread::SetThreadLocalInt(key, thread_id);
  }
  return thread_id;
}

}  // namespace internal
}  // namespace v8
