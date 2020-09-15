// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_CPPGC_MARKING_WORKLISTS_H_
#define V8_HEAP_CPPGC_MARKING_WORKLISTS_H_

#include "include/cppgc/visitor.h"
#include "src/heap/base/worklist.h"

namespace cppgc {
namespace internal {

class HeapObjectHeader;

class MarkingWorklists {
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
      heap::base::Worklist<MarkingItem, 512 /* local entries */>;
  using NotFullyConstructedWorklist =
      heap::base::Worklist<HeapObjectHeader*, 16 /* local entries */>;
  using WeakCallbackWorklist =
      heap::base::Worklist<WeakCallbackItem, 64 /* local entries */>;
  using WriteBarrierWorklist =
      heap::base::Worklist<HeapObjectHeader*, 64 /*local entries */>;

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
