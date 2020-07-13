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

class Heap;
class MutatorThreadMarkingVisitor;

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

  struct MarkingConfig {
    using StackState = cppgc::Heap::StackState;
    enum class IncrementalMarking : uint8_t { kDisabled };
    enum class ConcurrentMarking : uint8_t { kDisabled };

    static MarkingConfig Default() {
      return {StackState::kMayContainHeapPointers,
              IncrementalMarking::kDisabled, ConcurrentMarking::kDisabled};
    }

    explicit MarkingConfig(StackState stack_state)
        : MarkingConfig(stack_state, IncrementalMarking::kDisabled,
                        ConcurrentMarking::kDisabled) {}

    MarkingConfig(StackState stack_state,
                  IncrementalMarking incremental_marking_state,
                  ConcurrentMarking concurrent_marking_state)
        : stack_state_(stack_state),
          incremental_marking_state_(incremental_marking_state),
          concurrent_marking_state_(concurrent_marking_state) {}

    StackState stack_state_;
    IncrementalMarking incremental_marking_state_;
    ConcurrentMarking concurrent_marking_state_;
  };

  explicit Marker(Heap* heap);
  virtual ~Marker();

  Marker(const Marker&) = delete;
  Marker& operator=(const Marker&) = delete;

  // Initialize marking according to the given config. This method will
  // trigger incremental/concurrent marking if needed.
  void StartMarking(MarkingConfig config);
  // Finalize marking. This method stops incremental/concurrent marking
  // if exsists and performs atomic pause marking.
  void FinishMarking();

  void ProcessWeakness();

  Heap* heap() { return heap_; }
  MarkingWorklist* marking_worklist() { return &marking_worklist_; }
  NotFullyConstructedWorklist* not_fully_constructed_worklist() {
    return &not_fully_constructed_worklist_;
  }
  WeakCallbackWorklist* weak_callback_worklist() {
    return &weak_callback_worklist_;
  }

  void ClearAllWorklistsForTesting();

 protected:
  virtual std::unique_ptr<MutatorThreadMarkingVisitor>
  CreateMutatorThreadMarkingVisitor();

 private:
  void VisitRoots();

  bool AdvanceMarkingWithDeadline(v8::base::TimeDelta);
  void FlushNotFullyConstructedObjects();

  Heap* const heap_;
  MarkingConfig config_ = MarkingConfig::Default();

  std::unique_ptr<MutatorThreadMarkingVisitor> marking_visitor_;

  MarkingWorklist marking_worklist_;
  NotFullyConstructedWorklist not_fully_constructed_worklist_;
  NotFullyConstructedWorklist previously_not_fully_constructed_worklist_;
  WeakCallbackWorklist weak_callback_worklist_;
};

}  // namespace internal
}  // namespace cppgc

#endif  // V8_HEAP_CPPGC_MARKER_H_
