// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_INCREMENTAL_MARKING_H_
#define V8_HEAP_INCREMENTAL_MARKING_H_


#include "src/execution.h"
#include "src/heap/mark-compact.h"
#include "src/objects.h"

namespace v8 {
namespace internal {


class IncrementalMarking {
 public:
  enum State { STOPPED, SWEEPING, MARKING, COMPLETE };

  enum CompletionAction { GC_VIA_STACK_GUARD, NO_GC_VIA_STACK_GUARD };

  enum ForceMarkingAction { FORCE_MARKING, DO_NOT_FORCE_MARKING };

  enum ForceCompletionAction { FORCE_COMPLETION, DO_NOT_FORCE_COMPLETION };

  enum GCRequestType { COMPLETE_MARKING, OVERAPPROXIMATION };

  explicit IncrementalMarking(Heap* heap);

  static void Initialize();

  State state() {
    DCHECK(state_ == STOPPED || FLAG_incremental_marking);
    return state_;
  }

  bool should_hurry() { return should_hurry_; }
  void set_should_hurry(bool val) { should_hurry_ = val; }

  bool weak_closure_was_overapproximated() const {
    return weak_closure_was_overapproximated_;
  }

  void SetWeakClosureWasOverApproximatedForTesting(bool val) {
    weak_closure_was_overapproximated_ = val;
  }

  inline bool IsStopped() { return state() == STOPPED; }

  INLINE(bool IsMarking()) { return state() >= MARKING; }

  inline bool IsMarkingIncomplete() { return state() == MARKING; }

  inline bool IsComplete() { return state() == COMPLETE; }

  inline bool IsReadyToOverApproximateWeakClosure() const {
    return request_type_ == OVERAPPROXIMATION &&
           !weak_closure_was_overapproximated_;
  }

  GCRequestType request_type() const { return request_type_; }

  bool CanBeActivated();

  bool ShouldActivateEvenWithoutIdleNotification();

  bool WasActivated();

  void Start();

  void Stop();

  void MarkObjectGroups();

  void PrepareForScavenge();

  void UpdateMarkingDequeAfterScavenge();

  void Hurry();

  void Finalize();

  void Abort();

  void OverApproximateWeakClosure(CompletionAction action);

  void MarkingComplete(CompletionAction action);

  void Epilogue();

  // It's hard to know how much work the incremental marker should do to make
  // progress in the face of the mutator creating new work for it.  We start
  // of at a moderate rate of work and gradually increase the speed of the
  // incremental marker until it completes.
  // Do some marking every time this much memory has been allocated or that many
  // heavy (color-checking) write barriers have been invoked.
  static const intptr_t kAllocatedThreshold = 65536;
  static const intptr_t kWriteBarriersInvokedThreshold = 32768;
  // Start off by marking this many times more memory than has been allocated.
  static const intptr_t kInitialMarkingSpeed = 1;
  // But if we are promoting a lot of data we need to mark faster to keep up
  // with the data that is entering the old space through promotion.
  static const intptr_t kFastMarking = 3;
  // After this many steps we increase the marking/allocating factor.
  static const intptr_t kMarkingSpeedAccellerationInterval = 1024;
  // This is how much we increase the marking/allocating factor by.
  static const intptr_t kMarkingSpeedAccelleration = 2;
  static const intptr_t kMaxMarkingSpeed = 1000;

  // This is the upper bound for how many times we allow finalization of
  // incremental marking to be postponed.
  static const size_t kMaxIdleMarkingDelayCounter = 3;

  void OldSpaceStep(intptr_t allocated);

  intptr_t Step(intptr_t allocated, CompletionAction action,
                ForceMarkingAction marking = DO_NOT_FORCE_MARKING,
                ForceCompletionAction completion = FORCE_COMPLETION);

  inline void RestartIfNotMarking() {
    if (state_ == COMPLETE) {
      state_ = MARKING;
      if (FLAG_trace_incremental_marking) {
        PrintF("[IncrementalMarking] Restarting (new grey objects)\n");
      }
    }
  }

  static void RecordWriteFromCode(HeapObject* obj, Object** slot,
                                  Isolate* isolate);

  // Record a slot for compaction.  Returns false for objects that are
  // guaranteed to be rescanned or not guaranteed to survive.
  //
  // No slots in white objects should be recorded, as some slots are typed and
  // cannot be interpreted correctly if the underlying object does not survive
  // the incremental cycle (stays white).
  INLINE(bool BaseRecordWrite(HeapObject* obj, Object** slot, Object* value));
  INLINE(void RecordWrite(HeapObject* obj, Object** slot, Object* value));
  INLINE(void RecordWriteIntoCode(HeapObject* obj, RelocInfo* rinfo,
                                  Object* value));
  INLINE(void RecordWriteOfCodeEntry(JSFunction* host, Object** slot,
                                     Code* value));


  void RecordWriteSlow(HeapObject* obj, Object** slot, Object* value);
  void RecordWriteIntoCodeSlow(HeapObject* obj, RelocInfo* rinfo,
                               Object* value);
  void RecordWriteOfCodeEntrySlow(JSFunction* host, Object** slot, Code* value);
  void RecordCodeTargetPatch(Code* host, Address pc, HeapObject* value);
  void RecordCodeTargetPatch(Address pc, HeapObject* value);

  inline void RecordWrites(HeapObject* obj);

  inline void BlackToGreyAndUnshift(HeapObject* obj, MarkBit mark_bit);

  inline void WhiteToGreyAndPush(HeapObject* obj, MarkBit mark_bit);

  inline void SetOldSpacePageFlags(MemoryChunk* chunk) {
    SetOldSpacePageFlags(chunk, IsMarking(), IsCompacting());
  }

  inline void SetNewSpacePageFlags(NewSpacePage* chunk) {
    SetNewSpacePageFlags(chunk, IsMarking());
  }

  bool IsCompacting() { return IsMarking() && is_compacting_; }

  void ActivateGeneratedStub(Code* stub);

  void NotifyOfHighPromotionRate();

  void EnterNoMarkingScope() { no_marking_scope_depth_++; }

  void LeaveNoMarkingScope() { no_marking_scope_depth_--; }

  void NotifyIncompleteScanOfObject(int unscanned_bytes) {
    unscanned_bytes_of_large_object_ = unscanned_bytes;
  }

  void ClearIdleMarkingDelayCounter();

  bool IsIdleMarkingDelayCounterLimitReached();

  INLINE(static void MarkObject(Heap* heap, HeapObject* object));

  Heap* heap() const { return heap_; }

 private:
  int64_t SpaceLeftInOldSpace();

  void SpeedUp();

  void ResetStepCounters();

  void StartMarking();

  void ActivateIncrementalWriteBarrier(PagedSpace* space);
  static void ActivateIncrementalWriteBarrier(NewSpace* space);
  void ActivateIncrementalWriteBarrier();

  static void DeactivateIncrementalWriteBarrierForSpace(PagedSpace* space);
  static void DeactivateIncrementalWriteBarrierForSpace(NewSpace* space);
  void DeactivateIncrementalWriteBarrier();

  static void SetOldSpacePageFlags(MemoryChunk* chunk, bool is_marking,
                                   bool is_compacting);

  static void SetNewSpacePageFlags(NewSpacePage* chunk, bool is_marking);

  INLINE(void ProcessMarkingDeque());

  INLINE(intptr_t ProcessMarkingDeque(intptr_t bytes_to_process));

  INLINE(void VisitObject(Map* map, HeapObject* obj, int size));

  void IncrementIdleMarkingDelayCounter();

  Heap* heap_;

  State state_;
  bool is_compacting_;

  int steps_count_;
  int64_t old_generation_space_available_at_start_of_incremental_;
  int64_t old_generation_space_used_at_start_of_incremental_;
  int64_t bytes_rescanned_;
  bool should_hurry_;
  int marking_speed_;
  intptr_t bytes_scanned_;
  intptr_t allocated_;
  intptr_t write_barriers_invoked_since_last_step_;
  size_t idle_marking_delay_counter_;

  int no_marking_scope_depth_;

  int unscanned_bytes_of_large_object_;

  bool was_activated_;

  bool weak_closure_was_overapproximated_;

  int weak_closure_approximation_rounds_;

  GCRequestType request_type_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(IncrementalMarking);
};
}
}  // namespace v8::internal

#endif  // V8_HEAP_INCREMENTAL_MARKING_H_
