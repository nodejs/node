// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_CPPGC_MARKING_WORKLISTS_H_
#define V8_HEAP_CPPGC_MARKING_WORKLISTS_H_

#include "include/cppgc/visitor.h"
#include "src/heap/cppgc/worklist.h"

namespace cppgc {
namespace internal {

class HeapObjectHeader;

class MarkingWorklists {
  static constexpr int kNumConcurrentMarkers = 0;
  static constexpr int kNumMarkers = 1 + kNumConcurrentMarkers;

 public:
  static constexpr int kMutatorThreadId = 0;

  using MarkingItem = cppgc::TraceDescriptor;
  struct WeakCallbackItem {
    cppgc::WeakCallback callback;
    const void* parameter;
  };

  // Segment size of 512 entries necessary to avoid throughput regressions.
  // Since the work list is currently a temporary object this is not a problem.
  using MarkingWorklist =
      Worklist<MarkingItem, 512 /* local entries */, kNumMarkers>;
  using NotFullyConstructedWorklist =
      Worklist<HeapObjectHeader*, 16 /* local entries */, kNumMarkers>;
  using WeakCallbackWorklist =
      Worklist<WeakCallbackItem, 64 /* local entries */, kNumMarkers>;
  using WriteBarrierWorklist =
      Worklist<HeapObjectHeader*, 64 /*local entries */, kNumMarkers>;

  MarkingWorklist* marking_worklist() { return &marking_worklist_; }
  NotFullyConstructedWorklist* not_fully_constructed_worklist() {
    return &not_fully_constructed_worklist_;
  }
  NotFullyConstructedWorklist* previously_not_fully_constructed_worklist() {
    return &previously_not_fully_constructed_worklist_;
  }
  WriteBarrierWorklist* write_barrier_worklist() {
    return &write_barrier_worklist_;
  }
  WeakCallbackWorklist* weak_callback_worklist() {
    return &weak_callback_worklist_;
  }

  // Moves objects in not_fully_constructed_worklist_ to
  // previously_not_full_constructed_worklists_.
  void FlushNotFullyConstructedObjects();

  void ClearForTesting();

 private:
  MarkingWorklist marking_worklist_;
  NotFullyConstructedWorklist not_fully_constructed_worklist_;
  NotFullyConstructedWorklist previously_not_fully_constructed_worklist_;
  WriteBarrierWorklist write_barrier_worklist_;
  WeakCallbackWorklist weak_callback_worklist_;
};

}  // namespace internal
}  // namespace cppgc

#endif  // V8_HEAP_CPPGC_MARKING_WORKLISTS_H_
