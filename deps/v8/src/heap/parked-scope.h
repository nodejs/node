// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_PARKED_SCOPE_H_
#define V8_HEAP_PARKED_SCOPE_H_

#include "src/base/platform/condition-variable.h"
#include "src/base/platform/mutex.h"
#include "src/base/platform/semaphore.h"
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

// Scope that automatically parks the thread while blocking on the given
// base::Mutex.
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

// A subclass of base::ConditionVariable that automatically parks the thread
// while waiting.
class V8_NODISCARD ParkingConditionVariable final
    : public base::ConditionVariable {
 public:
  ParkingConditionVariable() = default;
  ParkingConditionVariable(const ParkingConditionVariable&) = delete;
  ParkingConditionVariable& operator=(const ParkingConditionVariable&) = delete;

  void ParkedWait(LocalIsolate* local_isolate, base::Mutex* mutex) {
    ParkedWait(local_isolate->heap(), mutex);
  }
  void ParkedWait(LocalHeap* local_heap, base::Mutex* mutex) {
    ParkedScope scope(local_heap);
    ParkedWait(scope, mutex);
  }
  void ParkedWait(const ParkedScope& scope, base::Mutex* mutex) {
    USE(scope);
    Wait(mutex);
  }

  bool ParkedWaitFor(LocalIsolate* local_isolate, base::Mutex* mutex,
                     const base::TimeDelta& rel_time) V8_WARN_UNUSED_RESULT {
    return ParkedWaitFor(local_isolate->heap(), mutex, rel_time);
  }
  bool ParkedWaitFor(LocalHeap* local_heap, base::Mutex* mutex,
                     const base::TimeDelta& rel_time) V8_WARN_UNUSED_RESULT {
    ParkedScope scope(local_heap);
    return ParkedWaitFor(scope, mutex, rel_time);
  }
  bool ParkedWaitFor(const ParkedScope& scope, base::Mutex* mutex,
                     const base::TimeDelta& rel_time) V8_WARN_UNUSED_RESULT {
    USE(scope);
    return WaitFor(mutex, rel_time);
  }

 private:
  using base::ConditionVariable::Wait;
  using base::ConditionVariable::WaitFor;
};

// A subclass of base::Semaphore that automatically parks the thread while
// waiting.
class V8_NODISCARD ParkingSemaphore final : public base::Semaphore {
 public:
  explicit ParkingSemaphore(int count) : base::Semaphore(count) {}
  ParkingSemaphore(const ParkingSemaphore&) = delete;
  ParkingSemaphore& operator=(const ParkingSemaphore&) = delete;

  void ParkedWait(LocalIsolate* local_isolate) {
    ParkedWait(local_isolate->heap());
  }
  void ParkedWait(LocalHeap* local_heap) {
    ParkedScope scope(local_heap);
    ParkedWait(scope);
  }
  void ParkedWait(const ParkedScope& scope) {
    USE(scope);
    Wait();
  }

  bool ParkedWaitFor(LocalIsolate* local_isolate,
                     const base::TimeDelta& rel_time) V8_WARN_UNUSED_RESULT {
    return ParkedWaitFor(local_isolate->heap(), rel_time);
  }
  bool ParkedWaitFor(LocalHeap* local_heap,
                     const base::TimeDelta& rel_time) V8_WARN_UNUSED_RESULT {
    ParkedScope scope(local_heap);
    return ParkedWaitFor(scope, rel_time);
  }
  bool ParkedWaitFor(const ParkedScope& scope,
                     const base::TimeDelta& rel_time) {
    USE(scope);
    return WaitFor(rel_time);
  }

 private:
  using base::Semaphore::Wait;
  using base::Semaphore::WaitFor;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_PARKED_SCOPE_H_
