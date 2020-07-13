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

  // Enter the safepoint from a thread
  void EnterFromThread(LocalHeap* local_heap);

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

  // Use these methods now instead of the more intrusive SafepointScope
  void Start();
  void End();

  bool IsActive() { return is_active_; }

 private:
  class Barrier {
    base::Mutex mutex_;
    base::ConditionVariable cond_;
    bool armed_;

   public:
    Barrier() : armed_(false) {}

    void Arm();
    void Disarm();
    void Wait();
  };

  void StopThreads();
  void ResumeThreads();

  void AddLocalHeap(LocalHeap* local_heap);
  void RemoveLocalHeap(LocalHeap* local_heap);

  Barrier barrier_;
  Heap* heap_;

  base::Mutex local_heaps_mutex_;
  LocalHeap* local_heaps_head_;

  bool is_active_;

  friend class SafepointScope;
  friend class LocalHeap;
  friend class PersistentHandles;
};

class SafepointScope {
 public:
  V8_EXPORT_PRIVATE explicit SafepointScope(Heap* heap);
  V8_EXPORT_PRIVATE ~SafepointScope();

 private:
  GlobalSafepoint* safepoint_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_SAFEPOINT_H_
