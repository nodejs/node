// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_CPPGC_MARKER_H_
#define V8_HEAP_CPPGC_MARKER_H_

#include <memory>

#include "include/cppgc/heap.h"
#include "include/cppgc/visitor.h"
#include "src/base/macros.h"
#include "src/base/platform/time.h"
#include "src/heap/cppgc/globals.h"
#include "src/heap/cppgc/marking-state.h"
#include "src/heap/cppgc/marking-visitor.h"
#include "src/heap/cppgc/marking-worklists.h"
#include "src/heap/cppgc/worklist.h"

namespace cppgc {
namespace internal {

class HeapBase;

// Marking algorithm. Example for a valid call sequence creating the marking
// phase:
// 1. StartMarking()
// 2. AdvanceMarkingWithDeadline() [Optional, depending on environment.]
// 3. EnterAtomicPause()
// 4. AdvanceMarkingWithDeadline()
// 5. LeaveAtomicPause()
//
// Alternatively, FinishMarking combines steps 3.-5.
class V8_EXPORT_PRIVATE MarkerBase {
 public:
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

  virtual ~MarkerBase();

  MarkerBase(const MarkerBase&) = delete;
  MarkerBase& operator=(const MarkerBase&) = delete;

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

  inline void WriteBarrierForInConstructionObject(HeapObjectHeader&);
  inline void WriteBarrierForObject(HeapObjectHeader&);

  HeapBase& heap() { return heap_; }

  MarkingWorklists& MarkingWorklistsForTesting() { return marking_worklists_; }
  MarkingState& MarkingStateForTesting() { return mutator_marking_state_; }
  cppgc::Visitor& VisitorForTesting() { return visitor(); }
  void ClearAllWorklistsForTesting();

 protected:
  explicit MarkerBase(HeapBase& heap);

  virtual cppgc::Visitor& visitor() = 0;
  virtual ConservativeTracingVisitor& conservative_visitor() = 0;
  virtual heap::base::StackVisitor& stack_visitor() = 0;

  void VisitRoots();

  void MarkNotFullyConstructedObjects();

  HeapBase& heap_;
  MarkingConfig config_ = MarkingConfig::Default();

  MarkingWorklists marking_worklists_;
  MarkingState mutator_marking_state_;
};

class V8_EXPORT_PRIVATE Marker final : public MarkerBase {
 public:
  explicit Marker(HeapBase&);

 protected:
  cppgc::Visitor& visitor() final { return marking_visitor_; }
  ConservativeTracingVisitor& conservative_visitor() final {
    return conservative_marking_visitor_;
  }
  heap::base::StackVisitor& stack_visitor() final {
    return conservative_marking_visitor_;
  }

 private:
  MarkingVisitor marking_visitor_;
  ConservativeMarkingVisitor conservative_marking_visitor_;
};

void MarkerBase::WriteBarrierForInConstructionObject(HeapObjectHeader& header) {
  MarkingWorklists::NotFullyConstructedWorklist::View
      not_fully_constructed_worklist(
          marking_worklists_.not_fully_constructed_worklist(),
          MarkingWorklists::kMutatorThreadId);
  not_fully_constructed_worklist.Push(&header);
}

void MarkerBase::WriteBarrierForObject(HeapObjectHeader& header) {
  MarkingWorklists::WriteBarrierWorklist::View write_barrier_worklist(
      marking_worklists_.write_barrier_worklist(),
      MarkingWorklists::kMutatorThreadId);
  write_barrier_worklist.Push(&header);
}

}  // namespace internal
}  // namespace cppgc

#endif  // V8_HEAP_CPPGC_MARKER_H_
