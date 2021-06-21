// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_SAFEPOINT_H_
#define V8_HEAP_SAFEPOINT_H_

#include "src/base/platform/condition-variable.h"
#include "src/base/platform/mutex.h"
#include "src/handles/persistent-handles.h"
#include "src/heap/local-heap.h"
#include "src/objects/visitors.h"

namespace v8 {
namespace internal {

class Heap;
class LocalHeap;
class RootVisitor;

// Used to bring all background threads with heap access to a safepoint such
// that e.g. a garbage collection can be performed.
class GlobalSafepoint {
 public:
  explicit GlobalSafepoint(Heap* heap);

  // Wait until unpark operation is safe again
  void WaitInUnpark();

  // Enter the safepoint from a running thread
  void WaitInSafepoint();

  // Running thread reached a safepoint by parking itself.
  void NotifyPark();

  V8_EXPORT_PRIVATE bool ContainsLocalHeap(LocalHeap* local_heap);
  V8_EXPORT_PRIVATE bool ContainsAnyLocalHeap();

  // Iterate handles in local heaps
  void Iterate(RootVisitor* visitor);

  // Iterate local heaps
  template <typename Callback>
  void IterateLocalHeaps(Callback callback) {
    DCHECK(IsActive());
    for (LocalHeap* current = local_heaps_head_; current;
         current = current->next_) {
      callback(current);
    }
  }

  bool IsActive() { return active_safepoint_scopes_ > 0; }

 private:
  class Barrier {
    base::Mutex mutex_;
    base::ConditionVariable cv_resume_;
    base::ConditionVariable cv_stopped_;
    bool armed_;

    int stopped_ = 0;

    bool IsArmed() { return armed_; }

   public:
    Barrier() : armed_(false), stopped_(0) {}

    void Arm();
    void Disarm();
    void WaitUntilRunningThreadsInSafepoint(int running);

    void WaitInSafepoint();
    void WaitInUnpark();
    void NotifyPark();
  };

  void EnterSafepointScope();
  void LeaveSafepointScope();

  template <typename Callback>
  void AddLocalHeap(LocalHeap* local_heap, Callback callback) {
    // Safepoint holds this lock in order to stop threads from starting or
    // stopping.
    base::MutexGuard guard(&local_heaps_mutex_);

    // Additional code protected from safepoint
    callback();

    // Add list to doubly-linked list
    if (local_heaps_head_) local_heaps_head_->prev_ = local_heap;
    local_heap->prev_ = nullptr;
    local_heap->next_ = local_heaps_head_;
    local_heaps_head_ = local_heap;
  }

  template <typename Callback>
  void RemoveLocalHeap(LocalHeap* local_heap, Callback callback) {
    base::MutexGuard guard(&local_heaps_mutex_);

    // Additional code protected from safepoint
    callback();

    // Remove list from doubly-linked list
    if (local_heap->next_) local_heap->next_->prev_ = local_heap->prev_;
    if (local_heap->prev_)
      local_heap->prev_->next_ = local_heap->next_;
    else
      local_heaps_head_ = local_heap->next_;
  }

  Barrier barrier_;
  Heap* heap_;

  base::Mutex local_heaps_mutex_;
  LocalHeap* local_heaps_head_;

  int active_safepoint_scopes_;

  friend class SafepointScope;
  friend class LocalHeap;
  friend class PersistentHandles;
};

class V8_NODISCARD SafepointScope {
 public:
  V8_EXPORT_PRIVATE explicit SafepointScope(Heap* heap);
  V8_EXPORT_PRIVATE ~SafepointScope();

 private:
  GlobalSafepoint* safepoint_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_SAFEPOINT_H_
