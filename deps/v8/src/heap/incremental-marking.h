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

// This marking state is used when concurrent marking is running.
class IncrementalMarkingState final
    : public MarkingStateBase<IncrementalMarkingState, AccessMode::ATOMIC> {
 public:
  Bitmap* bitmap(const MemoryChunk* chunk) const {
    return Bitmap::FromAddress(chunk->address() + MemoryChunk::kHeaderSize);
  }

  // Concurrent marking uses local live bytes.
  void IncrementLiveBytes(MemoryChunk* chunk, intptr_t by) {
    chunk->live_byte_count_ += by;
  }

  intptr_t live_bytes(MemoryChunk* chunk) const {
    return chunk->live_byte_count_;
  }

  void SetLiveBytes(MemoryChunk* chunk, intptr_t value) {
    chunk->live_byte_count_ = value;
  }
};

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
#endif
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

  explicit IncrementalMarking(Heap* heap);

  MarkingState* marking_state() { return &marking_state_; }

  AtomicMarkingState* atomic_marking_state() { return &atomic_marking_state_; }

  NonAtomicMarkingState* non_atomic_marking_state() {
    return &non_atomic_marking_state_;
  }

  void NotifyLeftTrimming(HeapObject* from, HeapObject* to);

  V8_INLINE void TransferColor(HeapObject* from, HeapObject* to) {
    if (atomic_marking_state()->IsBlack(to)) {
      DCHECK(black_allocation());
      return;
    }

    DCHECK(atomic_marking_state()->IsWhite(to));
    if (atomic_marking_state()->IsGrey(from)) {
      bool success = atomic_marking_state()->WhiteToGrey(to);
      DCHECK(success);
      USE(success);
    } else if (atomic_marking_state()->IsBlack(from)) {
      bool success = atomic_marking_state()->WhiteToBlack(to);
      DCHECK(success);
      USE(success);
    }
  }


  State state() {
    DCHECK(state_ == STOPPED || FLAG_incremental_marking);
    return state_;
  }

  bool should_hurry() { return should_hurry_; }
  void set_should_hurry(bool val) { should_hurry_ = val; }

  bool finalize_marking_completed() const {
    return finalize_marking_completed_;
  }

  void SetWeakClosureWasOverApproximatedForTesting(bool val) {
    finalize_marking_completed_ = val;
  }

  inline bool IsStopped() { return state() == STOPPED; }

  inline bool IsSweeping() { return state() == SWEEPING; }

  INLINE(bool IsMarking()) { return state() >= MARKING; }

  inline bool IsMarkingIncomplete() { return state() == MARKING; }

  inline bool IsComplete() { return state() == COMPLETE; }

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
                                   ForceCompletionAction force_completion,
                                   StepOrigin step_origin);

  // It's hard to know how much work the incremental marker should do to make
  // progress in the face of the mutator creating new work for it.  We start
  // of at a moderate rate of work and gradually increase the speed of the
  // incremental marker until it completes.
  // Do some marking every time this much memory has been allocated or that many
  // heavy (color-checking) write barriers have been invoked.
  static const size_t kAllocatedThreshold = 64 * KB;

  static const int kStepSizeInMs = 1;
  static const int kMaxStepSizeInMs = 5;

  // This is the upper bound for how many times we allow finalization of
  // incremental marking to be postponed.
  static const int kMaxIdleMarkingDelayCounter = 3;

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

  void FinalizeSweeping();

  size_t Step(size_t bytes_to_process, CompletionAction action,
              ForceCompletionAction completion, StepOrigin step_origin);

  inline void RestartIfNotMarking();

  static int RecordWriteFromCode(HeapObject* obj, Object** slot,
                                 Isolate* isolate);

  // Record a slot for compaction.  Returns false for objects that are
  // guaranteed to be rescanned or not guaranteed to survive.
  //
  // No slots in white objects should be recorded, as some slots are typed and
  // cannot be interpreted correctly if the underlying object does not survive
  // the incremental cycle (stays white).
  INLINE(bool BaseRecordWrite(HeapObject* obj, Object* value));
  INLINE(void RecordWrite(HeapObject* obj, Object** slot, Object* value));
  INLINE(void RecordWriteIntoCode(Code* host, RelocInfo* rinfo, Object* value));
  INLINE(void RecordWrites(HeapObject* obj));

  void RecordWriteSlow(HeapObject* obj, Object** slot, Object* value);
  void RecordWriteIntoCodeSlow(Code* host, RelocInfo* rinfo, Object* value);

  // Returns true if the function succeeds in transitioning the object
  // from white to grey.
  bool WhiteToGreyAndPush(HeapObject* obj);

  // This function is used to color the object black before it undergoes an
  // unsafe layout change. This is a part of synchronization protocol with
  // the concurrent marker.
  void MarkBlackAndPush(HeapObject* obj);

  inline void SetOldSpacePageFlags(MemoryChunk* chunk) {
    SetOldSpacePageFlags(chunk, IsMarking(), IsCompacting());
  }

  inline void SetNewSpacePageFlags(Page* chunk) {
    SetNewSpacePageFlags(chunk, IsMarking());
  }

  bool IsCompacting() { return IsMarking() && is_compacting_; }

  void ActivateGeneratedStub(Code* stub);

  void NotifyIncompleteScanOfObject(int unscanned_bytes) {
    unscanned_bytes_of_large_object_ = unscanned_bytes;
  }

  void ClearIdleMarkingDelayCounter();

  bool IsIdleMarkingDelayCounterLimitReached();

  void ProcessBlackAllocatedObject(HeapObject* obj);

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

  void AbortBlackAllocation();

  MarkCompactCollector::MarkingWorklist* marking_worklist() {
    SLOW_DCHECK(marking_worklist_ != nullptr);
    return marking_worklist_;
  }

  void set_marking_worklist(
      MarkCompactCollector::MarkingWorklist* marking_worklist) {
    marking_worklist_ = marking_worklist;
  }

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

  int64_t SpaceLeftInOldSpace();

  void StartMarking();

  void StartBlackAllocation();
  void PauseBlackAllocation();
  void FinishBlackAllocation();

  void MarkRoots();
  // Retain dying maps for <FLAG_retain_maps_for_n_gc> garbage collections to
  // increase chances of reusing of map transition tree in future.
  void RetainMaps();

  void ActivateIncrementalWriteBarrier(PagedSpace* space);
  static void ActivateIncrementalWriteBarrier(NewSpace* space);
  void ActivateIncrementalWriteBarrier();

  static void DeactivateIncrementalWriteBarrierForSpace(PagedSpace* space);
  static void DeactivateIncrementalWriteBarrierForSpace(NewSpace* space);
  void DeactivateIncrementalWriteBarrier();

  static void SetOldSpacePageFlags(MemoryChunk* chunk, bool is_marking,
                                   bool is_compacting);

  static void SetNewSpacePageFlags(MemoryChunk* chunk, bool is_marking);

  INLINE(intptr_t ProcessMarkingWorklist(
      intptr_t bytes_to_process,
      ForceCompletionAction completion = DO_NOT_FORCE_COMPLETION));

  INLINE(bool IsFixedArrayWithProgressBar(HeapObject* object));
  // Visits the object and returns its size.
  INLINE(int VisitObject(Map* map, HeapObject* obj));

  void RevisitObject(HeapObject* obj);

  void IncrementIdleMarkingDelayCounter();

  void AdvanceIncrementalMarkingOnAllocation();

  size_t StepSizeToKeepUpWithAllocations();
  size_t StepSizeToMakeProgress();

  Heap* heap_;
  MarkCompactCollector::MarkingWorklist* marking_worklist_;

  double start_time_ms_;
  size_t initial_old_generation_size_;
  size_t old_generation_allocation_counter_;
  size_t bytes_allocated_;
  size_t bytes_marked_ahead_of_schedule_;
  size_t unscanned_bytes_of_large_object_;

  void SetState(State s) {
    state_ = s;
    heap_->SetIsMarkingFlag(s >= MARKING);
  }

  // Must use SetState() above to update state_
  State state_;

  int idle_marking_delay_counter_;
  int incremental_marking_finalization_rounds_;

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
