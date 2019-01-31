// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_INCREMENTAL_MARKING_H_
#define V8_HEAP_INCREMENTAL_MARKING_H_

#include "src/cancelable-task.h"
#include "src/heap/heap.h"
#include "src/heap/incremental-marking-job.h"
#include "src/heap/mark-compact.h"

namespace v8 {
namespace internal {

class HeapObject;
class MarkBit;
class Map;
class Object;
class PagedSpace;

enum class StepOrigin { kV8, kTask };

class V8_EXPORT_PRIVATE IncrementalMarking {
 public:
  enum State { STOPPED, SWEEPING, MARKING, COMPLETE };

  enum CompletionAction { GC_VIA_STACK_GUARD, NO_GC_VIA_STACK_GUARD };

  enum ForceCompletionAction { FORCE_COMPLETION, DO_NOT_FORCE_COMPLETION };

  enum GCRequestType { NONE, COMPLETE_MARKING, FINALIZATION };

#ifdef V8_CONCURRENT_MARKING
  using MarkingState = IncrementalMarkingState;
#else
  using MarkingState = MajorNonAtomicMarkingState;
#endif  // V8_CONCURRENT_MARKING
  using AtomicMarkingState = MajorAtomicMarkingState;
  using NonAtomicMarkingState = MajorNonAtomicMarkingState;

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

  static const int kStepSizeInMs = 1;
  static const int kMaxStepSizeInMs = 5;

#ifndef DEBUG
  static const intptr_t kActivationThreshold = 8 * MB;
#else
  static const intptr_t kActivationThreshold = 0;
#endif

#ifdef V8_CONCURRENT_MARKING
  static const AccessMode kAtomicity = AccessMode::ATOMIC;
#else
  static const AccessMode kAtomicity = AccessMode::NON_ATOMIC;
#endif

  IncrementalMarking(Heap* heap,
                     MarkCompactCollector::MarkingWorklist* marking_worklist,
                     WeakObjects* weak_objects);

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

  // Performs incremental marking steps until deadline_in_ms is reached. It
  // returns the remaining time that cannot be used for incremental marking
  // anymore because a single step would exceed the deadline.
  double AdvanceIncrementalMarking(double deadline_in_ms,
                                   CompletionAction completion_action,
                                   StepOrigin step_origin);

  void FinalizeSweeping();

  size_t Step(size_t bytes_to_process, CompletionAction action,
              StepOrigin step_origin);
  void StepOnAllocation(size_t bytes_to_process, double max_step_size);

  bool ShouldDoEmbedderStep();
  void EmbedderStep(double duration);

  inline void RestartIfNotMarking();

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
  V8_INLINE bool BaseRecordWrite(HeapObject obj, Object value);
  V8_INLINE void RecordWrite(HeapObject obj, ObjectSlot slot, Object value);
  V8_INLINE void RecordMaybeWeakWrite(HeapObject obj, MaybeObjectSlot slot,
                                      MaybeObject value);
  void RevisitObject(HeapObject obj);
  // Ensures that all descriptors int range [0, number_of_own_descripts)
  // are visited.
  void VisitDescriptors(HeapObject host, DescriptorArray array,
                        int number_of_own_descriptors);

  void RecordWriteSlow(HeapObject obj, HeapObjectSlot slot, Object value);
  void RecordWriteIntoCode(Code host, RelocInfo* rinfo, HeapObject value);

  // Returns true if the function succeeds in transitioning the object
  // from white to grey.
  bool WhiteToGreyAndPush(HeapObject obj);

  // This function is used to color the object black before it undergoes an
  // unsafe layout change. This is a part of synchronization protocol with
  // the concurrent marker.
  void MarkBlackAndVisitObjectDueToLayoutChange(HeapObject obj);

  bool IsCompacting() { return IsMarking() && is_compacting_; }

  void NotifyIncompleteScanOfObject(int unscanned_bytes) {
    unscanned_bytes_of_large_object_ = unscanned_bytes;
  }

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

  MarkCompactCollector::MarkingWorklist* marking_worklist() const {
    return marking_worklist_;
  }

  void Deactivate();

  // Ensures that the given region is black allocated if it is in the old
  // generation.
  void EnsureBlackAllocated(Address allocated, size_t size);

 private:
  class Observer : public AllocationObserver {
   public:
    Observer(IncrementalMarking& incremental_marking, intptr_t step_size)
        : AllocationObserver(step_size),
          incremental_marking_(incremental_marking) {}

    void Step(int bytes_allocated, Address, size_t) override;

   private:
    IncrementalMarking& incremental_marking_;
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

  V8_INLINE intptr_t ProcessMarkingWorklist(
      intptr_t bytes_to_process,
      ForceCompletionAction completion = DO_NOT_FORCE_COMPLETION);

  V8_INLINE bool IsFixedArrayWithProgressBar(HeapObject object);

  // Visits the object and returns its size.
  V8_INLINE int VisitObject(Map map, HeapObject obj);

  void IncrementIdleMarkingDelayCounter();

  void AdvanceIncrementalMarkingOnAllocation();

  size_t StepSizeToKeepUpWithAllocations();
  size_t StepSizeToMakeProgress();

  void SetState(State s) {
    state_ = s;
    heap_->SetIsMarkingFlag(s >= MARKING);
  }

  Heap* const heap_;
  MarkCompactCollector::MarkingWorklist* const marking_worklist_;
  WeakObjects* weak_objects_;

  double start_time_ms_;
  size_t initial_old_generation_size_;
  size_t old_generation_allocation_counter_;
  size_t bytes_allocated_;
  size_t bytes_marked_ahead_of_schedule_;
  // A sample of concurrent_marking()->TotalMarkedBytes() at the last
  // incremental marking step. It is used for updating
  // bytes_marked_ahead_of_schedule_ with contribution of concurrent marking.
  size_t bytes_marked_concurrently_;
  size_t unscanned_bytes_of_large_object_;

  // Must use SetState() above to update state_
  State state_;

  bool is_compacting_;
  bool should_hurry_;
  bool was_activated_;
  bool black_allocation_;
  bool finalize_marking_completed_;
  bool trace_wrappers_toggle_;
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
