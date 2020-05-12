// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_SAFEPOINT_H_
#define V8_HEAP_SAFEPOINT_H_

#include "src/base/platform/condition-variable.h"
#include "src/base/platform/mutex.h"

namespace v8 {
namespace internal {

class Heap;
class LocalHeap;
class RootVisitor;

class Safepoint {
 public:
  explicit Safepoint(Heap* heap);

  // Enter the safepoint from a thread
  void EnterFromThread(LocalHeap* local_heap);

  V8_EXPORT_PRIVATE bool ContainsLocalHeap(LocalHeap* local_heap);
  V8_EXPORT_PRIVATE bool ContainsAnyLocalHeap();

  // Iterate handles in local heaps
  void Iterate(RootVisitor* visitor);

  // Use these methods now instead of the more intrusive SafepointScope
  void Start();
  void End();

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

  friend class SafepointScope;
  friend class LocalHeap;
};

class SafepointScope {
 public:
  V8_EXPORT_PRIVATE explicit SafepointScope(Heap* heap);
  V8_EXPORT_PRIVATE ~SafepointScope();

 private:
  Safepoint* safepoint_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_SAFEPOINT_H_
