// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_CPPGC_MARKER_H_
#define V8_HEAP_CPPGC_MARKER_H_

#include <memory>

#include "include/cppgc/heap.h"
#include "include/cppgc/trace-trait.h"
#include "include/cppgc/visitor.h"
#include "src/base/platform/time.h"
#include "src/heap/cppgc/worklist.h"

namespace cppgc {
namespace internal {

class HeapBase;
class HeapObjectHeader;
class MutatorThreadMarkingVisitor;

// Marking algorithm. Example for a valid call sequence creating the marking
// phase:
// 1. StartMarking()
// 2. AdvanceMarkingWithDeadline() [Optional, depending on environment.]
// 3. EnterAtomicPause()
// 4. AdvanceMarkingWithDeadline()
// 5. LeaveAtomicPause()
//
// Alternatively, FinishMarking combines steps 3.-5.
class V8_EXPORT_PRIVATE Marker {
  static constexpr int kNumConcurrentMarkers = 0;
  static constexpr int kNumMarkers = 1 + kNumConcurrentMarkers;

 public:
  static constexpr int kMutatorThreadId = 0;

  using MarkingItem = cppgc::TraceDescriptor;
  using NotFullyConstructedItem = const void*;
  struct WeakCallbackItem {
    cppgc::WeakCallback callback;
    const void* parameter;
  };

  // Segment size of 512 entries necessary to avoid throughput regressions.
  // Since the work list is currently a temporary object this is not a problem.
  using MarkingWorklist =
      Worklist<MarkingItem, 512 /* local entries */, kNumMarkers>;
  using NotFullyConstructedWorklist =
      Worklist<NotFullyConstructedItem, 16 /* local entries */, kNumMarkers>;
  using WeakCallbackWorklist =
      Worklist<WeakCallbackItem, 64 /* local entries */, kNumMarkers>;
  using WriteBarrierWorklist =
      Worklist<HeapObjectHeader*, 64 /*local entries */, kNumMarkers>;

  struct MarkingConfig {
    enum class CollectionType : uint8_t {
      kMinor,
      kMajor,
    };
    using StackState = cppgc::Heap::StackState;
    enum MarkingType : uint8_t {
      kAtomic,
      kIncremental,
      kIncrementalAndConcurrent
    };

    static constexpr MarkingConfig Default() { return {}; }

    CollectionType collection_type = CollectionType::kMajor;
    StackState stack_state = StackState::kMayContainHeapPointers;
    MarkingType marking_type = MarkingType::kAtomic;
  };

  explicit Marker(HeapBase& heap);
  virtual ~Marker();

  Marker(const Marker&) = delete;
  Marker& operator=(const Marker&) = delete;

  // Initialize marking according to the given config. This method will
  // trigger incremental/concurrent marking if needed.
  void StartMarking(MarkingConfig config);

  // Signals entering the atomic marking pause. The method
  // - stops incremental/concurrent marking;
  // - flushes back any in-construction worklists if needed;
  // - Updates the MarkingConfig if the stack state has changed;
  void EnterAtomicPause(MarkingConfig config);

  // Makes marking progress.
  virtual bool AdvanceMarkingWithDeadline(v8::base::TimeDelta);

  // Signals leaving the atomic marking pause. This method expects no more
  // objects to be marked and merely updates marking states if needed.
  void LeaveAtomicPause();

  // Combines:
  // - EnterAtomicPause()
  // - AdvanceMarkingWithDeadline()
  // - LeaveAtomicPause()
  void FinishMarking(MarkingConfig config);

  void ProcessWeakness();

  HeapBase& heap() { return heap_; }
  MarkingWorklist* marking_worklist() { return &marking_worklist_; }
  NotFullyConstructedWorklist* not_fully_constructed_worklist() {
    return &not_fully_constructed_worklist_;
  }
  WriteBarrierWorklist* write_barrier_worklist() {
    return &write_barrier_worklist_;
  }
  WeakCallbackWorklist* weak_callback_worklist() {
    return &weak_callback_worklist_;
  }

  void ClearAllWorklistsForTesting();

  MutatorThreadMarkingVisitor* GetMarkingVisitorForTesting() {
    return marking_visitor_.get();
  }

 protected:
  virtual std::unique_ptr<MutatorThreadMarkingVisitor>
  CreateMutatorThreadMarkingVisitor();

  void VisitRoots();

  void FlushNotFullyConstructedObjects();
  void MarkNotFullyConstructedObjects();

  HeapBase& heap_;
  MarkingConfig config_ = MarkingConfig::Default();

  std::unique_ptr<MutatorThreadMarkingVisitor> marking_visitor_;

  MarkingWorklist marking_worklist_;
  NotFullyConstructedWorklist not_fully_constructed_worklist_;
  NotFullyConstructedWorklist previously_not_fully_constructed_worklist_;
  WriteBarrierWorklist write_barrier_worklist_;
  WeakCallbackWorklist weak_callback_worklist_;
};

}  // namespace internal
}  // namespace cppgc

#endif  // V8_HEAP_CPPGC_MARKER_H_
