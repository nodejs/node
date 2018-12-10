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

struct LocalStorageKeyAllocator {
  static void Construct(void* storage_ptr_arg) {
    auto storage_ptr =
        reinterpret_cast<base::Thread::LocalStorageKey*>(storage_ptr_arg);
    *storage_ptr = base::Thread::CreateThreadLocalKey();
  }
};

static base::LazyInstance<base::Thread::LocalStorageKey,
                          LocalStorageKeyAllocator>::type thread_id_key =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
ThreadId ThreadId::TryGetCurrent() {
  int thread_id = base::Thread::GetThreadLocalInt(thread_id_key.Get());
  return thread_id == 0 ? Invalid() : ThreadId(thread_id);
}

// static
int ThreadId::GetCurrentThreadId() {
  int thread_id = base::Thread::GetThreadLocalInt(thread_id_key.Get());
  if (thread_id == 0) {
    thread_id = AllocateThreadId();
    base::Thread::SetThreadLocalInt(thread_id_key.Get(), thread_id);
  }
  return thread_id;
}

}  // namespace internal
}  // namespace v8
