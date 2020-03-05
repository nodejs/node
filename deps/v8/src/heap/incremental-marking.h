// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_INCREMENTAL_MARKING_H_
#define V8_HEAP_INCREMENTAL_MARKING_H_

#include "src/heap/heap.h"
#include "src/heap/incremental-marking-job.h"
#include "src/heap/mark-compact.h"
#include "src/tasks/cancelable-task.h"

namespace v8 {
namespace internal {

class HeapObject;
class MarkBit;
class Map;
class Object;
class PagedSpace;

enum class StepOrigin { kV8, kTask };
enum class StepResult {
  kNoImmediateWork,
  kMoreWorkRemaining,
  kWaitingForFinalization
};

class V8_EXPORT_PRIVATE IncrementalMarking {
 public:
  enum State { STOPPED, SWEEPING, MARKING, COMPLETE };

  enum CompletionAction { GC_VIA_STACK_GUARD, NO_GC_VIA_STACK_GUARD };

  enum GCRequestType { NONE, COMPLETE_MARKING, FINALIZATION };

  using MarkingState = MarkCompactCollector::MarkingState;
  using AtomicMarkingState = MarkCompactCollector::AtomicMarkingState;
  using NonAtomicMarkingState = MarkCompactCollector::NonAtomicMarkingState;

  class PauseBlackAllocationScope {
   public:
    explicit PauseBlackAllocationScope(IncrementalMarking* marking)
        : marking_(marking), paused_(false) {
      if (marking_->black_allocation()) {
        paused_ = true;
        marking_->PauseBlackAllocation();
      }
    }

    ~PauseBlackAllocationScope() {
      if (paused_) {
        marking_->StartBlackAllocation();
      }
    }

   private:
    IncrementalMarking* marking_;
    bool paused_;
  };

  // It's hard to know how much work the incremental marker should do to make
  // progress in the face of the mutator creating new work for it.  We start
  // of at a moderate rate of work and gradually increase the speed of the
  // incremental marker until it completes.
  // Do some marking every time this much memory has been allocated or that many
  // heavy (color-checking) write barriers have been invoked.
  static const size_t kYoungGenerationAllocatedThreshold = 64 * KB;
  static const size_t kOldGenerationAllocatedThreshold = 256 * KB;
  static const size_t kMinStepSizeInBytes = 64 * KB;

  static constexpr double kStepSizeInMs = 1;
  static constexpr double kMaxStepSizeInMs = 5;

#ifndef DEBUG
  static constexpr size_t kV8ActivationThreshold = 8 * MB;
  static constexpr size_t kGlobalActivationThreshold = 16 * MB;
#else
  static constexpr size_t kV8ActivationThreshold = 0;
  static constexpr size_t kGlobalActivationThreshold = 0;
#endif

#ifdef V8_CONCURRENT_MARKING
  static const AccessMode kAtomicity = AccessMode::ATOMIC;
#else
  static const AccessMode kAtomicity = AccessMode::NON_ATOMIC;
#endif

  IncrementalMarking(Heap* heap, WeakObjects* weak_objects);

  MarkingState* marking_state() { return &marking_state_; }

  AtomicMarkingState* atomic_marking_state() { return &atomic_marking_state_; }

  NonAtomicMarkingState* non_atomic_marking_state() {
    return &non_atomic_marking_state_;
  }

  void NotifyLeftTrimming(HeapObject from, HeapObject to);

  V8_INLINE void TransferColor(HeapObject from, HeapObject to);

  State state() const {
    DCHECK(state_ == STOPPED || FLAG_incremental_marking);
    return state_;
  }

  bool should_hurry() const { return should_hurry_; }
  void set_should_hurry(bool val) { should_hurry_ = val; }

  bool finalize_marking_completed() const {
    return finalize_marking_completed_;
  }

  void SetWeakClosureWasOverApproximatedForTesting(bool val) {
    finalize_marking_completed_ = val;
  }

  inline bool IsStopped() const { return state() == STOPPED; }

  inline bool IsSweeping() const { return state() == SWEEPING; }

  inline bool IsMarking() const { return state() >= MARKING; }

  inline bool IsMarkingIncomplete() const { return state() == MARKING; }

  inline bool IsComplete() const { return state() == COMPLETE; }

  inline bool IsReadyToOverApproximateWeakClosure() const {
    return request_type_ == FINALIZATION && !finalize_marking_completed_;
  }

  inline bool NeedsFinalization() {
    return IsMarking() &&
           (request_type_ == FINALIZATION || request_type_ == COMPLETE_MARKING);
  }

  GCRequestType request_type() const { return request_type_; }

  void reset_request_type() { request_type_ = NONE; }

  bool CanBeActivated();

  bool WasActivated();

  void Start(GarbageCollectionReason gc_reason);

  void FinalizeIncrementally();

  void UpdateMarkingWorklistAfterScavenge();
  void UpdateWeakReferencesAfterScavenge();
  void UpdateMarkedBytesAfterScavenge(size_t dead_bytes_in_new_space);

  void Hurry();

  void Finalize();

  void Stop();

  void FinalizeMarking(CompletionAction action);

  void MarkingComplete(CompletionAction action);

  void Epilogue();

  // Performs incremental marking steps and returns before the deadline_in_ms is
  // reached. It may return earlier if the marker is already ahead of the
  // marking schedule, which is indicated with StepResult::kDone.
  StepResult AdvanceWithDeadline(double deadline_in_ms,
                                 CompletionAction completion_action,
                                 StepOrigin step_origin);

  void FinalizeSweeping();

  StepResult V8Step(double max_step_size_in_ms, CompletionAction action,
                    StepOrigin step_origin);

  bool ShouldDoEmbedderStep();
  StepResult EmbedderStep(double duration);

  V8_INLINE void RestartIfNotMarking();

  // {raw_obj} and {slot_address} are raw Address values instead of a
  // HeapObject and a MaybeObjectSlot because this is called from
  // generated code via ExternalReference.
  static int RecordWriteFromCode(Address raw_obj, Address slot_address,
                                 Isolate* isolate);

  // Record a slot for compaction.  Returns false for objects that are
  // guaranteed to be rescanned or not guaranteed to survive.
  //
  // No slots in white objects should be recorded, as some slots are typed and
  // cannot be interpreted correctly if the underlying object does not survive
  // the incremental cycle (stays white).
  V8_INLINE bool BaseRecordWrite(HeapObject obj, HeapObject value);
  template <typename TSlot>
  V8_INLINE void RecordWrite(HeapObject obj, TSlot slot,
                             typename TSlot::TObject value);
  void RecordWriteSlow(HeapObject obj, HeapObjectSlot slot, HeapObject value);
  void RecordWriteIntoCode(Code host, RelocInfo* rinfo, HeapObject value);

  // Returns true if the function succeeds in transitioning the object
  // from white to grey.
  V8_INLINE bool WhiteToGreyAndPush(HeapObject obj);

  // This function is used to color the object black before it undergoes an
  // unsafe layout change. This is a part of synchronization protocol with
  // the concurrent marker.
  void MarkBlackAndVisitObjectDueToLayoutChange(HeapObject obj);

  bool IsCompacting() { return IsMarking() && is_compacting_; }

  void ProcessBlackAllocatedObject(HeapObject obj);

  Heap* heap() const { return heap_; }

  IncrementalMarkingJob* incremental_marking_job() {
    return &incremental_marking_job_;
  }

  bool black_allocation() { return black_allocation_; }

  void StartBlackAllocationForTesting() {
    if (!black_allocation_) {
      StartBlackAllocation();
    }
  }

  MarkingWorklists* marking_worklists() const {
    return collector_->marking_worklists();
  }

  void Deactivate();

  // Ensures that the given region is black allocated if it is in the old
  // generation.
  void EnsureBlackAllocated(Address allocated, size_t size);

  bool IsBelowActivationThresholds() const;

 private:
  class Observer : public AllocationObserver {
   public:
    Observer(IncrementalMarking* incremental_marking, intptr_t step_size)
        : AllocationObserver(step_size),
          incremental_marking_(incremental_marking) {}

    void Step(int bytes_allocated, Address, size_t) override;

   private:
    IncrementalMarking* incremental_marking_;
  };

  void StartMarking();

  void StartBlackAllocation();
  void PauseBlackAllocation();
  void FinishBlackAllocation();

  void MarkRoots();
  bool ShouldRetainMap(Map map, int age);
  // Retain dying maps for <FLAG_retain_maps_for_n_gc> garbage collections to
  // increase chances of reusing of map transition tree in future.
  void RetainMaps();

  void ActivateIncrementalWriteBarrier(PagedSpace* space);
  void ActivateIncrementalWriteBarrier(NewSpace* space);
  void ActivateIncrementalWriteBarrier();

  void DeactivateIncrementalWriteBarrierForSpace(PagedSpace* space);
  void DeactivateIncrementalWriteBarrierForSpace(NewSpace* space);
  void DeactivateIncrementalWriteBarrier();

  // Updates scheduled_bytes_to_mark_ to ensure marking progress based on
  // time.
  void ScheduleBytesToMarkBasedOnTime(double time_ms);
  // Updates scheduled_bytes_to_mark_ to ensure marking progress based on
  // allocations.
  void ScheduleBytesToMarkBasedOnAllocation();
  // Helper functions for ScheduleBytesToMarkBasedOnAllocation.
  size_t StepSizeToKeepUpWithAllocations();
  size_t StepSizeToMakeProgress();
  void AddScheduledBytesToMark(size_t bytes_to_mark);

  // Schedules more bytes to mark so that the marker is no longer ahead
  // of schedule.
  void FastForwardSchedule();
  void FastForwardScheduleIfCloseToFinalization();

  // Fetches marked byte counters from the concurrent marker.
  void FetchBytesMarkedConcurrently();

  // Returns the bytes to mark in the current step based on the scheduled
  // bytes and already marked bytes.
  size_t ComputeStepSizeInBytes(StepOrigin step_origin);

  void AdvanceOnAllocation();

  void SetState(State s) {
    state_ = s;
    heap_->SetIsMarkingFlag(s >= MARKING);
  }

  Heap* const heap_;
  MarkCompactCollector* const collector_;
  WeakObjects* weak_objects_;

  double start_time_ms_;
  size_t initial_old_generation_size_;
  size_t old_generation_allocation_counter_;
  size_t bytes_marked_;
  size_t scheduled_bytes_to_mark_;
  double schedule_update_time_ms_;
  // A sample of concurrent_marking()->TotalMarkedBytes() at the last
  // incremental marking step. It is used for updating
  // bytes_marked_ahead_of_schedule_ with contribution of concurrent marking.
  size_t bytes_marked_concurrently_;

  // Must use SetState() above to update state_
  State state_;

  bool is_compacting_;
  bool should_hurry_;
  bool was_activated_;
  bool black_allocation_;
  bool finalize_marking_completed_;
  IncrementalMarkingJob incremental_marking_job_;

  GCRequestType request_type_;

  Observer new_generation_observer_;
  Observer old_generation_observer_;

  MarkingState marking_state_;
  AtomicMarkingState atomic_marking_state_;
  NonAtomicMarkingState non_atomic_marking_state_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(IncrementalMarking);
};
}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_INCREMENTAL_MARKING_H_
