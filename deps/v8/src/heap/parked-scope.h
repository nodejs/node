// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_PARKED_SCOPE_H_
#define V8_HEAP_PARKED_SCOPE_H_

#include "src/base/platform/mutex.h"
#include "src/execution/local-isolate.h"
#include "src/heap/local-heap.h"

namespace v8 {
namespace internal {

// Scope that explicitly parks a thread, prohibiting access to the heap and the
// creation of handles.
class V8_NODISCARD ParkedScope {
 public:
  explicit ParkedScope(LocalIsolate* local_isolate)
      : ParkedScope(local_isolate->heap()) {}
  explicit ParkedScope(LocalHeap* local_heap) : local_heap_(local_heap) {
    local_heap_->Park();
  }

  ~ParkedScope() { local_heap_->Unpark(); }

 private:
  LocalHeap* const local_heap_;
};

// Scope that explicitly unparks a thread, allowing access to the heap and the
// creation of handles.
class V8_NODISCARD UnparkedScope {
 public:
  explicit UnparkedScope(LocalIsolate* local_isolate)
      : UnparkedScope(local_isolate->heap()) {}
  explicit UnparkedScope(LocalHeap* local_heap) : local_heap_(local_heap) {
    local_heap_->Unpark();
  }

  ~UnparkedScope() { local_heap_->Park(); }

 private:
  LocalHeap* const local_heap_;
};

class V8_NODISCARD ParkedMutexGuard {
 public:
  explicit ParkedMutexGuard(LocalIsolate* local_isolate, base::Mutex* mutex)
      : ParkedMutexGuard(local_isolate->heap(), mutex) {}
  explicit ParkedMutexGuard(LocalHeap* local_heap, base::Mutex* mutex)
      : mutex_(mutex) {
    DCHECK(AllowGarbageCollection::IsAllowed());
    if (!mutex_->TryLock()) {
      ParkedScope scope(local_heap);
      mutex_->Lock();
    }
  }

  ParkedMutexGuard(const ParkedMutexGuard&) = delete;
  ParkedMutexGuard& operator=(const ParkedMutexGuard&) = delete;

  ~ParkedMutexGuard() { mutex_->Unlock(); }

 private:
  base::Mutex* mutex_;
};

template <base::MutexSharedType kIsShared,
          base::NullBehavior Behavior = base::NullBehavior::kRequireNotNull>
class V8_NODISCARD ParkedSharedMutexGuardIf final {
 public:
  ParkedSharedMutexGuardIf(LocalIsolate* local_isolate,
                           base::SharedMutex* mutex, bool enable_mutex)
      : ParkedSharedMutexGuardIf(local_isolate->heap(), mutex, enable_mutex) {}
  ParkedSharedMutexGuardIf(LocalHeap* local_heap, base::SharedMutex* mutex,
                           bool enable_mutex) {
    DCHECK(AllowGarbageCollection::IsAllowed());
    DCHECK_IMPLIES(Behavior == base::NullBehavior::kRequireNotNull,
                   mutex != nullptr);
    if (!enable_mutex) return;
    mutex_ = mutex;

    if (kIsShared) {
      if (!mutex_->TryLockShared()) {
        ParkedScope scope(local_heap);
        mutex_->LockShared();
      }
    } else {
      if (!mutex_->TryLockExclusive()) {
        ParkedScope scope(local_heap);
        mutex_->LockExclusive();
      }
    }
  }
  ParkedSharedMutexGuardIf(const ParkedSharedMutexGuardIf&) = delete;
  ParkedSharedMutexGuardIf& operator=(const ParkedSharedMutexGuardIf&) = delete;

  ~ParkedSharedMutexGuardIf() {
    if (!mutex_) return;

    if (kIsShared) {
      mutex_->UnlockShared();
    } else {
      mutex_->UnlockExclusive();
    }
  }

 private:
  base::SharedMutex* mutex_ = nullptr;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_PARKED_SCOPE_H_
