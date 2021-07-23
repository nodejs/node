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
#include "src/heap/base/worklist.h"
#include "src/heap/cppgc/concurrent-marker.h"
#include "src/heap/cppgc/globals.h"
#include "src/heap/cppgc/incremental-marking-schedule.h"
#include "src/heap/cppgc/marking-state.h"
#include "src/heap/cppgc/marking-visitor.h"
#include "src/heap/cppgc/marking-worklists.h"
#include "src/heap/cppgc/task-handle.h"

namespace cppgc {
namespace internal {

class HeapBase;
class MarkerFactory;

// Marking algorithm. Example for a valid call sequence creating the marking
// phase:
// 1. StartMarking() [Called implicitly when creating a Marker using
//                    MarkerFactory]
// 2. AdvanceMarkingWithLimits() [Optional, depending on environment.]
// 3. EnterAtomicPause()
// 4. AdvanceMarkingWithLimits()
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
    using MarkingType = cppgc::Heap::MarkingType;
    enum class IsForcedGC : uint8_t {
      kNotForced,
      kForced,
    };

    static constexpr MarkingConfig Default() { return {}; }

    const CollectionType collection_type = CollectionType::kMajor;
    StackState stack_state = StackState::kMayContainHeapPointers;
    MarkingType marking_type = MarkingType::kIncremental;
    IsForcedGC is_forced_gc = IsForcedGC::kNotForced;
  };

  enum class WriteBarrierType {
    kDijkstra,
    kSteele,
  };

  virtual ~MarkerBase();

  MarkerBase(const MarkerBase&) = delete;
  MarkerBase& operator=(const MarkerBase&) = delete;

  // Signals entering the atomic marking pause. The method
  // - stops incremental/concurrent marking;
  // - flushes back any in-construction worklists if needed;
  // - Updates the MarkingConfig if the stack state has changed;
  void EnterAtomicPause(MarkingConfig::StackState);

  // Makes marking progress.  A `marked_bytes_limit` of 0 means that the limit
  // is determined by the internal marking scheduler.
  //
  // TODO(chromium:1056170): Remove TimeDelta argument when unified heap no
  // longer uses it.
  bool AdvanceMarkingWithLimits(
      v8::base::TimeDelta = kMaximumIncrementalStepDuration,
      size_t marked_bytes_limit = 0);

  // Makes marking progress when allocation a new lab.
  void AdvanceMarkingOnAllocation();

  // Signals leaving the atomic marking pause. This method expects no more
  // objects to be marked and merely updates marking states if needed.
  void LeaveAtomicPause();

  // Combines:
  // - EnterAtomicPause()
  // - AdvanceMarkingWithLimits()
  // - ProcessWeakness()
  // - LeaveAtomicPause()
  void FinishMarking(MarkingConfig::StackState);

  void ProcessWeakness();

  inline void WriteBarrierForInConstructionObject(HeapObjectHeader&);

  template <WriteBarrierType type>
  inline void WriteBarrierForObject(HeapObjectHeader&);

  HeapBase& heap() { return heap_; }

  MarkingWorklists& MarkingWorklistsForTesting() { return marking_worklists_; }
  MutatorMarkingState& MutatorMarkingStateForTesting() {
    return mutator_marking_state_;
  }
  cppgc::Visitor& Visitor() { return visitor(); }
  void ClearAllWorklistsForTesting();

  bool IncrementalMarkingStepForTesting(MarkingConfig::StackState);

  class IncrementalMarkingTask final : public cppgc::Task {
   public:
    using Handle = SingleThreadedHandle;

    IncrementalMarkingTask(MarkerBase*, MarkingConfig::StackState);

    static Handle Post(cppgc::TaskRunner*, MarkerBase*);

   private:
    void Run() final;

    MarkerBase* const marker_;
    MarkingConfig::StackState stack_state_;
    // TODO(chromium:1056170): Change to CancelableTask.
    Handle handle_;
  };

  void SetMainThreadMarkingDisabledForTesting(bool);

  void WaitForConcurrentMarkingForTesting();

  void NotifyCompactionCancelled();

  bool IsMarking() const { return is_marking_; }

 protected:
  static constexpr v8::base::TimeDelta kMaximumIncrementalStepDuration =
      v8::base::TimeDelta::FromMilliseconds(2);

  class Key {
   private:
    Key() = default;
    friend class MarkerFactory;
  };

  MarkerBase(Key, HeapBase&, cppgc::Platform*, MarkingConfig);

  // Initialize marking according to the given config. This method will
  // trigger incremental/concurrent marking if needed.
  void StartMarking();

  virtual cppgc::Visitor& visitor() = 0;
  virtual ConservativeTracingVisitor& conservative_visitor() = 0;
  virtual heap::base::StackVisitor& stack_visitor() = 0;

  bool ProcessWorklistsWithDeadline(size_t, v8::base::TimeTicks);

  void VisitRoots(MarkingConfig::StackState);

  void MarkNotFullyConstructedObjects();

  void ScheduleIncrementalMarkingTask();

  bool IncrementalMarkingStep(MarkingConfig::StackState);

  HeapBase& heap_;
  MarkingConfig config_ = MarkingConfig::Default();

  cppgc::Platform* platform_;
  std::shared_ptr<cppgc::TaskRunner> foreground_task_runner_;
  IncrementalMarkingTask::Handle incremental_marking_handle_;

  MarkingWorklists marking_worklists_;
  MutatorMarkingState mutator_marking_state_;
  bool is_marking_{false};

  IncrementalMarkingSchedule schedule_;

  std::unique_ptr<ConcurrentMarkerBase> concurrent_marker_{nullptr};

  bool main_marking_disabled_for_testing_{false};

  friend class MarkerFactory;
};

class V8_EXPORT_PRIVATE MarkerFactory {
 public:
  template <typename T, typename... Args>
  static std::unique_ptr<T> CreateAndStartMarking(Args&&... args) {
    static_assert(std::is_base_of<MarkerBase, T>::value,
                  "MarkerFactory can only create subclasses of MarkerBase");
    std::unique_ptr<T> marker =
        std::make_unique<T>(MarkerBase::Key(), std::forward<Args>(args)...);
    marker->StartMarking();
    return marker;
  }
};

class V8_EXPORT_PRIVATE Marker final : public MarkerBase {
 public:
  Marker(Key, HeapBase&, cppgc::Platform*,
         MarkingConfig = MarkingConfig::Default());

 protected:
  cppgc::Visitor& visitor() final { return marking_visitor_; }
  ConservativeTracingVisitor& conservative_visitor() final {
    return conservative_marking_visitor_;
  }
  heap::base::StackVisitor& stack_visitor() final {
    return conservative_marking_visitor_;
  }

 private:
  MutatorMarkingVisitor marking_visitor_;
  ConservativeMarkingVisitor conservative_marking_visitor_;
};

void MarkerBase::WriteBarrierForInConstructionObject(HeapObjectHeader& header) {
  mutator_marking_state_.not_fully_constructed_worklist()
      .Push<AccessMode::kAtomic>(&header);
}

template <MarkerBase::WriteBarrierType type>
void MarkerBase::WriteBarrierForObject(HeapObjectHeader& header) {
  switch (type) {
    case MarkerBase::WriteBarrierType::kDijkstra:
      mutator_marking_state_.write_barrier_worklist().Push(&header);
      break;
    case MarkerBase::WriteBarrierType::kSteele:
      mutator_marking_state_.retrace_marked_objects_worklist().Push(&header);
      break;
  }
}

}  // namespace internal
}  // namespace cppgc

#endif  // V8_HEAP_CPPGC_MARKER_H_
