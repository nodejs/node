// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/incremental-marking.h"

#include "src/code-stubs.h"
#include "src/compilation-cache.h"
#include "src/conversions.h"
#include "src/heap/concurrent-marking.h"
#include "src/heap/gc-idle-time-handler.h"
#include "src/heap/gc-tracer.h"
#include "src/heap/heap-inl.h"
#include "src/heap/incremental-marking-inl.h"
#include "src/heap/mark-compact-inl.h"
#include "src/heap/object-stats.h"
#include "src/heap/objects-visiting-inl.h"
#include "src/heap/objects-visiting.h"
#include "src/heap/sweeper.h"
#include "src/objects/hash-table-inl.h"
#include "src/tracing/trace-event.h"
#include "src/v8.h"
#include "src/visitors.h"
#include "src/vm-state-inl.h"

namespace v8 {
namespace internal {

using IncrementalMarkingMarkingVisitor =
    MarkingVisitor<FixedArrayVisitationMode::kIncremental,
                   TraceRetainingPathMode::kDisabled,
                   IncrementalMarking::MarkingState>;

void IncrementalMarking::Observer::Step(int bytes_allocated, Address addr,
                                        size_t size) {
  Heap* heap = incremental_marking_.heap();
  VMState<GC> state(heap->isolate());
  RuntimeCallTimerScope runtime_timer(
      heap->isolate(),
      RuntimeCallCounterId::kGC_Custom_IncrementalMarkingObserver);
  incremental_marking_.AdvanceIncrementalMarkingOnAllocation();
  if (incremental_marking_.black_allocation() && addr != kNullAddress) {
    // AdvanceIncrementalMarkingOnAllocation can start black allocation.
    // Ensure that the new object is marked black.
    HeapObject* object = HeapObject::FromAddress(addr);
    if (incremental_marking_.marking_state()->IsWhite(object) &&
        !(Heap::InNewSpace(object) || heap->new_lo_space()->Contains(object))) {
      if (heap->lo_space()->Contains(object)) {
        incremental_marking_.marking_state()->WhiteToBlack(object);
      } else {
        Page::FromAddress(addr)->CreateBlackArea(addr, addr + size);
      }
    }
  }
}

IncrementalMarking::IncrementalMarking(
    Heap* heap, MarkCompactCollector::MarkingWorklist* marking_worklist,
    WeakObjects* weak_objects)
    : heap_(heap),
      marking_worklist_(marking_worklist),
      weak_objects_(weak_objects),
      initial_old_generation_size_(0),
      bytes_marked_ahead_of_schedule_(0),
      bytes_marked_concurrently_(0),
      unscanned_bytes_of_large_object_(0),
      is_compacting_(false),
      should_hurry_(false),
      was_activated_(false),
      black_allocation_(false),
      finalize_marking_completed_(false),
      trace_wrappers_toggle_(false),
      request_type_(NONE),
      new_generation_observer_(*this, kYoungGenerationAllocatedThreshold),
      old_generation_observer_(*this, kOldGenerationAllocatedThreshold) {
  DCHECK_NOT_NULL(marking_worklist_);
  SetState(STOPPED);
}

bool IncrementalMarking::BaseRecordWrite(HeapObject* obj, Object* value) {
  HeapObject* value_heap_obj = HeapObject::cast(value);
  DCHECK(!marking_state()->IsImpossible(value_heap_obj));
  DCHECK(!marking_state()->IsImpossible(obj));
#ifdef V8_CONCURRENT_MARKING
  // The write barrier stub generated with V8_CONCURRENT_MARKING does not
  // check the color of the source object.
  const bool need_recording = true;
#else
  const bool need_recording = marking_state()->IsBlack(obj);
#endif

  if (need_recording && WhiteToGreyAndPush(value_heap_obj)) {
    RestartIfNotMarking();
  }
  return is_compacting_ && need_recording;
}

void IncrementalMarking::RecordWriteSlow(HeapObject* obj,
                                         HeapObjectReference** slot,
                                         Object* value) {
  if (BaseRecordWrite(obj, value) && slot != nullptr) {
    // Object is not going to be rescanned we need to record the slot.
    heap_->mark_compact_collector()->RecordSlot(obj, slot,
                                                HeapObject::cast(value));
  }
}

int IncrementalMarking::RecordWriteFromCode(HeapObject* obj, MaybeObject** slot,
                                            Isolate* isolate) {
  DCHECK(obj->IsHeapObject());
  isolate->heap()->incremental_marking()->RecordMaybeWeakWrite(obj, slot,
                                                               *slot);
  // Called by RecordWriteCodeStubAssembler, which doesnt accept void type
  return 0;
}

void IncrementalMarking::RecordWriteIntoCode(Code* host, RelocInfo* rinfo,
                                             HeapObject* value) {
  DCHECK(IsMarking());
  if (BaseRecordWrite(host, value)) {
    // Object is not going to be rescanned.  We need to record the slot.
    heap_->mark_compact_collector()->RecordRelocSlot(host, rinfo, value);
  }
}

bool IncrementalMarking::WhiteToGreyAndPush(HeapObject* obj) {
  if (marking_state()->WhiteToGrey(obj)) {
    marking_worklist()->Push(obj);
    return true;
  }
  return false;
}

void IncrementalMarking::MarkBlackAndPush(HeapObject* obj) {
  // Marking left-trimmable fixed array black is unsafe because left-trimming
  // re-pushes only grey arrays onto the marking worklist.
  DCHECK(!obj->IsFixedArrayBase());
  // Color the object black and push it into the bailout deque.
  marking_state()->WhiteToGrey(obj);
  if (marking_state()->GreyToBlack(obj)) {
    if (FLAG_concurrent_marking) {
      marking_worklist()->PushBailout(obj);
    } else {
      marking_worklist()->Push(obj);
    }
  }
}

void IncrementalMarking::NotifyLeftTrimming(HeapObject* from, HeapObject* to) {
  DCHECK(IsMarking());
  DCHECK(MemoryChunk::FromAddress(from->address())->SweepingDone());
  DCHECK_EQ(MemoryChunk::FromAddress(from->address()),
            MemoryChunk::FromAddress(to->address()));
  DCHECK_NE(from, to);

  MarkBit old_mark_bit = marking_state()->MarkBitFrom(from);
  MarkBit new_mark_bit = marking_state()->MarkBitFrom(to);

  if (black_allocation() && Marking::IsBlack<kAtomicity>(new_mark_bit)) {
    // Nothing to do if the object is in black area.
    return;
  }

  bool marked_black_due_to_left_trimming = false;
  if (FLAG_concurrent_marking) {
    // We need to mark the array black before overwriting its map and length
    // so that the concurrent marker does not observe inconsistent state.
    Marking::WhiteToGrey<kAtomicity>(old_mark_bit);
    if (Marking::GreyToBlack<kAtomicity>(old_mark_bit)) {
      // The concurrent marker will not mark the array. We need to push the
      // new array start in marking deque to ensure that it will be marked.
      marked_black_due_to_left_trimming = true;
    }
    DCHECK(Marking::IsBlack<kAtomicity>(old_mark_bit));
  }

  if (Marking::IsBlack<kAtomicity>(old_mark_bit) &&
      !marked_black_due_to_left_trimming) {
    // The array was black before left trimming or was marked black by the
    // concurrent marker. Simply transfer the color.
    if (from->address() + kPointerSize == to->address()) {
      // The old and the new markbits overlap. The |to| object has the
      // grey color. To make it black, we need to set the second bit.
      DCHECK(new_mark_bit.Get<kAtomicity>());
      new_mark_bit.Next().Set<kAtomicity>();
    } else {
      bool success = Marking::WhiteToBlack<kAtomicity>(new_mark_bit);
      DCHECK(success);
      USE(success);
    }
  } else if (Marking::IsGrey<kAtomicity>(old_mark_bit) ||
             marked_black_due_to_left_trimming) {
    // The array was already grey or was marked black by this function.
    // Mark the new array grey and push it to marking deque.
    if (from->address() + kPointerSize == to->address()) {
      // The old and the new markbits overlap. The |to| object is either white
      // or grey.  Set the first bit to make sure that it is grey.
      new_mark_bit.Set<kAtomicity>();
      DCHECK(!new_mark_bit.Next().Get<kAtomicity>());
    } else {
      bool success = Marking::WhiteToGrey<kAtomicity>(new_mark_bit);
      DCHECK(success);
      USE(success);
    }
    // Subsequent left-trimming will re-push only grey arrays.
    // Ensure that this array is grey.
    DCHECK(Marking::IsGrey<kAtomicity>(new_mark_bit));
    marking_worklist()->PushBailout(to);
    RestartIfNotMarking();
  }
}

class IncrementalMarkingRootMarkingVisitor : public RootVisitor {
 public:
  explicit IncrementalMarkingRootMarkingVisitor(
      IncrementalMarking* incremental_marking)
      : heap_(incremental_marking->heap()) {}

  void VisitRootPointer(Root root, const char* description,
                        Object** p) override {
    MarkObjectByPointer(p);
  }

  void VisitRootPointers(Root root, const char* description, Object** start,
                         Object** end) override {
    for (Object** p = start; p < end; p++) MarkObjectByPointer(p);
  }

 private:
  void MarkObjectByPointer(Object** p) {
    Object* obj = *p;
    if (!obj->IsHeapObject()) return;

    heap_->incremental_marking()->WhiteToGreyAndPush(HeapObject::cast(obj));
  }

  Heap* heap_;
};

void IncrementalMarking::DeactivateIncrementalWriteBarrierForSpace(
    PagedSpace* space) {
  for (Page* p : *space) {
    p->SetOldGenerationPageFlags(false);
  }
}


void IncrementalMarking::DeactivateIncrementalWriteBarrierForSpace(
    NewSpace* space) {
  for (Page* p : *space) {
    p->SetYoungGenerationPageFlags(false);
  }
}


void IncrementalMarking::DeactivateIncrementalWriteBarrier() {
  DeactivateIncrementalWriteBarrierForSpace(heap_->old_space());
  DeactivateIncrementalWriteBarrierForSpace(heap_->map_space());
  DeactivateIncrementalWriteBarrierForSpace(heap_->code_space());
  DeactivateIncrementalWriteBarrierForSpace(heap_->new_space());

  for (LargePage* p : *heap_->lo_space()) {
    p->SetOldGenerationPageFlags(false);
  }
}


void IncrementalMarking::ActivateIncrementalWriteBarrier(PagedSpace* space) {
  for (Page* p : *space) {
    p->SetOldGenerationPageFlags(true);
  }
}


void IncrementalMarking::ActivateIncrementalWriteBarrier(NewSpace* space) {
  for (Page* p : *space) {
    p->SetYoungGenerationPageFlags(true);
  }
}


void IncrementalMarking::ActivateIncrementalWriteBarrier() {
  ActivateIncrementalWriteBarrier(heap_->old_space());
  ActivateIncrementalWriteBarrier(heap_->map_space());
  ActivateIncrementalWriteBarrier(heap_->code_space());
  ActivateIncrementalWriteBarrier(heap_->new_space());

  for (LargePage* p : *heap_->lo_space()) {
    p->SetOldGenerationPageFlags(true);
  }
}


bool IncrementalMarking::WasActivated() { return was_activated_; }


bool IncrementalMarking::CanBeActivated() {
  // Only start incremental marking in a safe state: 1) when incremental
  // marking is turned on, 2) when we are currently not in a GC, and
  // 3) when we are currently not serializing or deserializing the heap.
  return FLAG_incremental_marking && heap_->gc_state() == Heap::NOT_IN_GC &&
         heap_->deserialization_complete() &&
         !heap_->isolate()->serializer_enabled();
}


void IncrementalMarking::Deactivate() {
  DeactivateIncrementalWriteBarrier();
}

void IncrementalMarking::Start(GarbageCollectionReason gc_reason) {
  if (FLAG_trace_incremental_marking) {
    int old_generation_size_mb =
        static_cast<int>(heap()->OldGenerationSizeOfObjects() / MB);
    int old_generation_limit_mb =
        static_cast<int>(heap()->old_generation_allocation_limit() / MB);
    heap()->isolate()->PrintWithTimestamp(
        "[IncrementalMarking] Start (%s): old generation %dMB, limit %dMB, "
        "slack %dMB\n",
        Heap::GarbageCollectionReasonToString(gc_reason),
        old_generation_size_mb, old_generation_limit_mb,
        Max(0, old_generation_limit_mb - old_generation_size_mb));
  }
  DCHECK(FLAG_incremental_marking);
  DCHECK(state_ == STOPPED);
  DCHECK(heap_->gc_state() == Heap::NOT_IN_GC);
  DCHECK(!heap_->isolate()->serializer_enabled());

  Counters* counters = heap_->isolate()->counters();

  counters->incremental_marking_reason()->AddSample(
      static_cast<int>(gc_reason));
  HistogramTimerScope incremental_marking_scope(
      counters->gc_incremental_marking_start());
  TRACE_EVENT0("v8", "V8.GCIncrementalMarkingStart");
  TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_INCREMENTAL_START);
  heap_->tracer()->NotifyIncrementalMarkingStart();

  start_time_ms_ = heap()->MonotonicallyIncreasingTimeInMs();
  initial_old_generation_size_ = heap_->OldGenerationSizeOfObjects();
  old_generation_allocation_counter_ = heap_->OldGenerationAllocationCounter();
  bytes_allocated_ = 0;
  bytes_marked_ahead_of_schedule_ = 0;
  bytes_marked_concurrently_ = 0;
  should_hurry_ = false;
  was_activated_ = true;

  if (!heap_->mark_compact_collector()->sweeping_in_progress()) {
    StartMarking();
  } else {
    if (FLAG_trace_incremental_marking) {
      heap()->isolate()->PrintWithTimestamp(
          "[IncrementalMarking] Start sweeping.\n");
    }
    SetState(SWEEPING);
  }

  heap_->AddAllocationObserversToAllSpaces(&old_generation_observer_,
                                           &new_generation_observer_);
  incremental_marking_job()->Start(heap_);
}


void IncrementalMarking::StartMarking() {
  if (heap_->isolate()->serializer_enabled()) {
    // Black allocation currently starts when we start incremental marking,
    // but we cannot enable black allocation while deserializing. Hence, we
    // have to delay the start of incremental marking in that case.
    if (FLAG_trace_incremental_marking) {
      heap()->isolate()->PrintWithTimestamp(
          "[IncrementalMarking] Start delayed - serializer\n");
    }
    return;
  }
  if (FLAG_trace_incremental_marking) {
    heap()->isolate()->PrintWithTimestamp(
        "[IncrementalMarking] Start marking\n");
  }

  is_compacting_ =
      !FLAG_never_compact && heap_->mark_compact_collector()->StartCompaction();

  SetState(MARKING);

  {
    TRACE_GC(heap()->tracer(),
             GCTracer::Scope::MC_INCREMENTAL_WRAPPER_PROLOGUE);
    heap_->local_embedder_heap_tracer()->TracePrologue();
  }

  ActivateIncrementalWriteBarrier();

// Marking bits are cleared by the sweeper.
#ifdef VERIFY_HEAP
  if (FLAG_verify_heap) {
    heap_->mark_compact_collector()->VerifyMarkbitsAreClean();
  }
#endif

  heap_->isolate()->compilation_cache()->MarkCompactPrologue();

#ifdef V8_CONCURRENT_MARKING
  // The write-barrier does not check the color of the source object.
  // Start black allocation earlier to ensure faster marking progress.
  if (!black_allocation_) {
    StartBlackAllocation();
  }
#endif

  // Mark strong roots grey.
  IncrementalMarkingRootMarkingVisitor visitor(this);
  heap_->IterateStrongRoots(&visitor, VISIT_ONLY_STRONG);

  if (FLAG_concurrent_marking && !heap_->IsTearingDown()) {
    heap_->concurrent_marking()->ScheduleTasks();
  }

  // Ready to start incremental marking.
  if (FLAG_trace_incremental_marking) {
    heap()->isolate()->PrintWithTimestamp("[IncrementalMarking] Running\n");
  }
}

void IncrementalMarking::StartBlackAllocation() {
  DCHECK(FLAG_black_allocation);
  DCHECK(!black_allocation_);
  DCHECK(IsMarking());
  black_allocation_ = true;
  heap()->old_space()->MarkLinearAllocationAreaBlack();
  heap()->map_space()->MarkLinearAllocationAreaBlack();
  heap()->code_space()->MarkLinearAllocationAreaBlack();
  if (FLAG_trace_incremental_marking) {
    heap()->isolate()->PrintWithTimestamp(
        "[IncrementalMarking] Black allocation started\n");
  }
}

void IncrementalMarking::PauseBlackAllocation() {
  DCHECK(FLAG_black_allocation);
  DCHECK(IsMarking());
  heap()->old_space()->UnmarkLinearAllocationArea();
  heap()->map_space()->UnmarkLinearAllocationArea();
  heap()->code_space()->UnmarkLinearAllocationArea();
  if (FLAG_trace_incremental_marking) {
    heap()->isolate()->PrintWithTimestamp(
        "[IncrementalMarking] Black allocation paused\n");
  }
  black_allocation_ = false;
}

void IncrementalMarking::FinishBlackAllocation() {
  if (black_allocation_) {
    black_allocation_ = false;
    if (FLAG_trace_incremental_marking) {
      heap()->isolate()->PrintWithTimestamp(
          "[IncrementalMarking] Black allocation finished\n");
    }
  }
}

void IncrementalMarking::MarkRoots() {
  DCHECK(!finalize_marking_completed_);
  DCHECK(IsMarking());

  IncrementalMarkingRootMarkingVisitor visitor(this);
  heap_->IterateStrongRoots(&visitor, VISIT_ONLY_STRONG);
}

bool IncrementalMarking::ShouldRetainMap(Map* map, int age) {
  if (age == 0) {
    // The map has aged. Do not retain this map.
    return false;
  }
  Object* constructor = map->GetConstructor();
  if (!constructor->IsHeapObject() ||
      marking_state()->IsWhite(HeapObject::cast(constructor))) {
    // The constructor is dead, no new objects with this map can
    // be created. Do not retain this map.
    return false;
  }
  return true;
}


void IncrementalMarking::RetainMaps() {
  // Do not retain dead maps if flag disables it or there is
  // - memory pressure (reduce_memory_footprint_),
  // - GC is requested by tests or dev-tools (abort_incremental_marking_).
  bool map_retaining_is_disabled = heap()->ShouldReduceMemory() ||
                                   FLAG_retain_maps_for_n_gc == 0;
  WeakArrayList* retained_maps = heap()->retained_maps();
  int length = retained_maps->length();
  // The number_of_disposed_maps separates maps in the retained_maps
  // array that were created before and after context disposal.
  // We do not age and retain disposed maps to avoid memory leaks.
  int number_of_disposed_maps = heap()->number_of_disposed_maps_;
  for (int i = 0; i < length; i += 2) {
    MaybeObject* value = retained_maps->Get(i);
    HeapObject* map_heap_object;
    if (!value->GetHeapObjectIfWeak(&map_heap_object)) {
      continue;
    }
    int age = Smi::ToInt(retained_maps->Get(i + 1)->cast<Smi>());
    int new_age;
    Map* map = Map::cast(map_heap_object);
    if (i >= number_of_disposed_maps && !map_retaining_is_disabled &&
        marking_state()->IsWhite(map)) {
      if (ShouldRetainMap(map, age)) {
        WhiteToGreyAndPush(map);
      }
      Object* prototype = map->prototype();
      if (age > 0 && prototype->IsHeapObject() &&
          marking_state()->IsWhite(HeapObject::cast(prototype))) {
        // The prototype is not marked, age the map.
        new_age = age - 1;
      } else {
        // The prototype and the constructor are marked, this map keeps only
        // transition tree alive, not JSObjects. Do not age the map.
        new_age = age;
      }
    } else {
      new_age = FLAG_retain_maps_for_n_gc;
    }
    // Compact the array and update the age.
    if (new_age != age) {
      retained_maps->Set(i + 1, MaybeObject::FromSmi(Smi::FromInt(new_age)));
    }
  }
}

void IncrementalMarking::FinalizeIncrementally() {
  TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_INCREMENTAL_FINALIZE_BODY);
  DCHECK(!finalize_marking_completed_);
  DCHECK(IsMarking());

  double start = heap_->MonotonicallyIncreasingTimeInMs();

  // After finishing incremental marking, we try to discover all unmarked
  // objects to reduce the marking load in the final pause.
  // 1) We scan and mark the roots again to find all changes to the root set.
  // 2) Age and retain maps embedded in optimized code.
  MarkRoots();

  // Map retaining is needed for perfromance, not correctness,
  // so we can do it only once at the beginning of the finalization.
  RetainMaps();

  finalize_marking_completed_ = true;

  if (FLAG_black_allocation && !heap()->ShouldReduceMemory() &&
      !black_allocation_) {
    // TODO(hpayer): Move to an earlier point as soon as we make faster marking
    // progress.
    StartBlackAllocation();
  }

  if (FLAG_trace_incremental_marking) {
    double end = heap_->MonotonicallyIncreasingTimeInMs();
    double delta = end - start;
    heap()->isolate()->PrintWithTimestamp(
        "[IncrementalMarking] Finalize incrementally spent %.1f ms.\n", delta);
  }
}

void IncrementalMarking::UpdateMarkingWorklistAfterScavenge() {
  if (!IsMarking()) return;

  Map* filler_map = ReadOnlyRoots(heap_).one_pointer_filler_map();

#ifdef ENABLE_MINOR_MC
  MinorMarkCompactCollector::MarkingState* minor_marking_state =
      heap()->minor_mark_compact_collector()->marking_state();
#else
  void* minor_marking_state = nullptr;
#endif  // ENABLE_MINOR_MC

  marking_worklist()->Update([this, filler_map, minor_marking_state](
                                 HeapObject* obj, HeapObject** out) -> bool {
    DCHECK(obj->IsHeapObject());
    // Only pointers to from space have to be updated.
    if (Heap::InFromSpace(obj)) {
      MapWord map_word = obj->map_word();
      if (!map_word.IsForwardingAddress()) {
        // There may be objects on the marking deque that do not exist anymore,
        // e.g. left trimmed objects or objects from the root set (frames).
        // If these object are dead at scavenging time, their marking deque
        // entries will not point to forwarding addresses. Hence, we can discard
        // them.
        return false;
      }
      HeapObject* dest = map_word.ToForwardingAddress();
      DCHECK_IMPLIES(marking_state()->IsWhite(obj), obj->IsFiller());
      *out = dest;
      return true;
    } else if (Heap::InToSpace(obj)) {
      // The object may be on a page that was moved in new space.
      DCHECK(
          Page::FromAddress(obj->address())->IsFlagSet(Page::SWEEP_TO_ITERATE));
#ifdef ENABLE_MINOR_MC
      if (minor_marking_state->IsGrey(obj)) {
        *out = obj;
        return true;
      }
#endif  // ENABLE_MINOR_MC
      return false;
    } else {
      // The object may be on a page that was moved from new to old space. Only
      // applicable during minor MC garbage collections.
      if (Page::FromAddress(obj->address())
              ->IsFlagSet(Page::SWEEP_TO_ITERATE)) {
#ifdef ENABLE_MINOR_MC
        if (minor_marking_state->IsGrey(obj)) {
          *out = obj;
          return true;
        }
#endif  // ENABLE_MINOR_MC
        return false;
      }
      DCHECK_IMPLIES(marking_state()->IsWhite(obj), obj->IsFiller());
      // Skip one word filler objects that appear on the
      // stack when we perform in place array shift.
      if (obj->map() != filler_map) {
        *out = obj;
        return true;
      }
      return false;
    }
  });

  UpdateWeakReferencesAfterScavenge();
}

namespace {
template <typename T>
T* ForwardingAddress(T* heap_obj) {
  MapWord map_word = heap_obj->map_word();

  if (map_word.IsForwardingAddress()) {
    return T::cast(map_word.ToForwardingAddress());
  } else if (Heap::InNewSpace(heap_obj)) {
    return nullptr;
  } else {
    return heap_obj;
  }
}
}  // namespace

void IncrementalMarking::UpdateWeakReferencesAfterScavenge() {
  weak_objects_->weak_references.Update(
      [](std::pair<HeapObject*, HeapObjectReference**> slot_in,
         std::pair<HeapObject*, HeapObjectReference**>* slot_out) -> bool {
        HeapObject* heap_obj = slot_in.first;
        HeapObject* forwarded = ForwardingAddress(heap_obj);

        if (forwarded) {
          ptrdiff_t distance_to_slot =
              reinterpret_cast<Address>(slot_in.second) -
              reinterpret_cast<Address>(slot_in.first);
          Address new_slot =
              reinterpret_cast<Address>(forwarded) + distance_to_slot;
          slot_out->first = forwarded;
          slot_out->second = reinterpret_cast<HeapObjectReference**>(new_slot);
          return true;
        }

        return false;
      });
  weak_objects_->weak_objects_in_code.Update(
      [](std::pair<HeapObject*, Code*> slot_in,
         std::pair<HeapObject*, Code*>* slot_out) -> bool {
        HeapObject* heap_obj = slot_in.first;
        HeapObject* forwarded = ForwardingAddress(heap_obj);

        if (forwarded) {
          slot_out->first = forwarded;
          slot_out->second = slot_in.second;
          return true;
        }

        return false;
      });
  weak_objects_->ephemeron_hash_tables.Update(
      [](EphemeronHashTable* slot_in, EphemeronHashTable** slot_out) -> bool {
        EphemeronHashTable* forwarded = ForwardingAddress(slot_in);

        if (forwarded) {
          *slot_out = forwarded;
          return true;
        }

        return false;
      });

  auto ephemeron_updater = [](Ephemeron slot_in, Ephemeron* slot_out) -> bool {
    HeapObject* key = slot_in.key;
    HeapObject* value = slot_in.value;
    HeapObject* forwarded_key = ForwardingAddress(key);
    HeapObject* forwarded_value = ForwardingAddress(value);

    if (forwarded_key && forwarded_value) {
      *slot_out = Ephemeron{forwarded_key, forwarded_value};
      return true;
    }

    return false;
  };

  weak_objects_->current_ephemerons.Update(ephemeron_updater);
  weak_objects_->next_ephemerons.Update(ephemeron_updater);
  weak_objects_->discovered_ephemerons.Update(ephemeron_updater);
}

void IncrementalMarking::UpdateMarkedBytesAfterScavenge(
    size_t dead_bytes_in_new_space) {
  if (!IsMarking()) return;
  bytes_marked_ahead_of_schedule_ -=
      Min(bytes_marked_ahead_of_schedule_, dead_bytes_in_new_space);
}

bool IncrementalMarking::IsFixedArrayWithProgressBar(HeapObject* obj) {
  if (!obj->IsFixedArray()) return false;
  MemoryChunk* chunk = MemoryChunk::FromAddress(obj->address());
  return chunk->IsFlagSet(MemoryChunk::HAS_PROGRESS_BAR);
}

int IncrementalMarking::VisitObject(Map* map, HeapObject* obj) {
  DCHECK(marking_state()->IsGrey(obj) || marking_state()->IsBlack(obj));
  if (!marking_state()->GreyToBlack(obj)) {
    // The object can already be black in these cases:
    // 1. The object is a fixed array with the progress bar.
    // 2. The object is a JSObject that was colored black before
    //    unsafe layout change.
    // 3. The object is a string that was colored black before
    //    unsafe layout change.
    // 4. The object is materizalized by the deoptimizer.
    DCHECK(obj->IsHashTable() || obj->IsPropertyArray() ||
           obj->IsFixedArray() || obj->IsJSObject() || obj->IsString());
  }
  DCHECK(marking_state()->IsBlack(obj));
  WhiteToGreyAndPush(map);
  IncrementalMarkingMarkingVisitor visitor(heap()->mark_compact_collector(),
                                           marking_state());
  return visitor.Visit(map, obj);
}

void IncrementalMarking::ProcessBlackAllocatedObject(HeapObject* obj) {
  if (IsMarking() && marking_state()->IsBlack(obj)) {
    RevisitObject(obj);
  }
}

void IncrementalMarking::RevisitObject(HeapObject* obj) {
  DCHECK(IsMarking());
  DCHECK(FLAG_concurrent_marking || marking_state()->IsBlack(obj));
  Page* page = Page::FromAddress(obj->address());
  if (page->owner()->identity() == LO_SPACE) {
    page->ResetProgressBar();
  }
  Map* map = obj->map();
  WhiteToGreyAndPush(map);
  IncrementalMarkingMarkingVisitor visitor(heap()->mark_compact_collector(),
                                           marking_state());
  visitor.Visit(map, obj);
}

template <WorklistToProcess worklist_to_process>
intptr_t IncrementalMarking::ProcessMarkingWorklist(
    intptr_t bytes_to_process, ForceCompletionAction completion) {
  intptr_t bytes_processed = 0;
  while (bytes_processed < bytes_to_process || completion == FORCE_COMPLETION) {
    HeapObject* obj;
    if (worklist_to_process == WorklistToProcess::kBailout) {
      obj = marking_worklist()->PopBailout();
    } else {
      obj = marking_worklist()->Pop();
    }
    if (obj == nullptr) break;
    // Left trimming may result in white, grey, or black filler objects on the
    // marking deque. Ignore these objects.
    if (obj->IsFiller()) {
      DCHECK(!marking_state()->IsImpossible(obj));
      continue;
    }
    unscanned_bytes_of_large_object_ = 0;
    int size = VisitObject(obj->map(), obj);
    bytes_processed += size - unscanned_bytes_of_large_object_;
  }
  // Report all found wrappers to the embedder. This is necessary as the
  // embedder could potentially invalidate wrappers as soon as V8 is done
  // with its incremental marking processing. Any cached wrappers could
  // result in broken pointers at this point.
  heap_->local_embedder_heap_tracer()->RegisterWrappersWithRemoteTracer();
  return bytes_processed;
}

void IncrementalMarking::EmbedderStep(double duration_ms) {
  constexpr int kObjectsToProcessBeforeInterrupt = 100;

  TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_INCREMENTAL_WRAPPER_TRACING);

  const double deadline =
      heap_->MonotonicallyIncreasingTimeInMs() + duration_ms;

  HeapObject* object;
  int cnt = 0;
  while (marking_worklist()->embedder()->Pop(0, &object)) {
    heap_->TracePossibleWrapper(JSObject::cast(object));
    if (++cnt == kObjectsToProcessBeforeInterrupt) {
      cnt = 0;
      if (heap_->MonotonicallyIncreasingTimeInMs() > deadline) {
        break;
      }
    }
  }

  heap_->local_embedder_heap_tracer()->RegisterWrappersWithRemoteTracer();
  if (!heap_->local_embedder_heap_tracer()
           ->ShouldFinalizeIncrementalMarking()) {
    heap_->local_embedder_heap_tracer()->Trace(deadline);
  }
}

void IncrementalMarking::Hurry() {
  // A scavenge may have pushed new objects on the marking deque (due to black
  // allocation) even in COMPLETE state. This may happen if scavenges are
  // forced e.g. in tests. It should not happen when COMPLETE was set when
  // incremental marking finished and a regular GC was triggered after that
  // because should_hurry_ will force a full GC.
  if (!marking_worklist()->IsEmpty()) {
    double start = 0.0;
    if (FLAG_trace_incremental_marking) {
      start = heap_->MonotonicallyIncreasingTimeInMs();
      if (FLAG_trace_incremental_marking) {
        heap()->isolate()->PrintWithTimestamp("[IncrementalMarking] Hurry\n");
      }
    }
    // TODO(gc) hurry can mark objects it encounters black as mutator
    // was stopped.
    ProcessMarkingWorklist(0, FORCE_COMPLETION);
    SetState(COMPLETE);
    if (FLAG_trace_incremental_marking) {
      double end = heap_->MonotonicallyIncreasingTimeInMs();
      double delta = end - start;
      if (FLAG_trace_incremental_marking) {
        heap()->isolate()->PrintWithTimestamp(
            "[IncrementalMarking] Complete (hurry), spent %d ms.\n",
            static_cast<int>(delta));
      }
    }
  }
}


void IncrementalMarking::Stop() {
  if (IsStopped()) return;
  if (FLAG_trace_incremental_marking) {
    int old_generation_size_mb =
        static_cast<int>(heap()->OldGenerationSizeOfObjects() / MB);
    int old_generation_limit_mb =
        static_cast<int>(heap()->old_generation_allocation_limit() / MB);
    heap()->isolate()->PrintWithTimestamp(
        "[IncrementalMarking] Stopping: old generation %dMB, limit %dMB, "
        "overshoot %dMB\n",
        old_generation_size_mb, old_generation_limit_mb,
        Max(0, old_generation_size_mb - old_generation_limit_mb));
  }

  SpaceIterator it(heap_);
  while (it.has_next()) {
    Space* space = it.next();
    if (space == heap_->new_space()) {
      space->RemoveAllocationObserver(&new_generation_observer_);
    } else {
      space->RemoveAllocationObserver(&old_generation_observer_);
    }
  }

  IncrementalMarking::set_should_hurry(false);
  heap_->isolate()->stack_guard()->ClearGC();
  SetState(STOPPED);
  is_compacting_ = false;
  FinishBlackAllocation();
}


void IncrementalMarking::Finalize() {
  Hurry();
  Stop();
}


void IncrementalMarking::FinalizeMarking(CompletionAction action) {
  DCHECK(!finalize_marking_completed_);
  if (FLAG_trace_incremental_marking) {
    heap()->isolate()->PrintWithTimestamp(
        "[IncrementalMarking] requesting finalization of incremental "
        "marking.\n");
  }
  request_type_ = FINALIZATION;
  if (action == GC_VIA_STACK_GUARD) {
    heap_->isolate()->stack_guard()->RequestGC();
  }
}


void IncrementalMarking::MarkingComplete(CompletionAction action) {
  SetState(COMPLETE);
  // We will set the stack guard to request a GC now.  This will mean the rest
  // of the GC gets performed as soon as possible (we can't do a GC here in a
  // record-write context).  If a few things get allocated between now and then
  // that shouldn't make us do a scavenge and keep being incremental, so we set
  // the should-hurry flag to indicate that there can't be much work left to do.
  set_should_hurry(true);
  if (FLAG_trace_incremental_marking) {
    heap()->isolate()->PrintWithTimestamp(
        "[IncrementalMarking] Complete (normal).\n");
  }
  request_type_ = COMPLETE_MARKING;
  if (action == GC_VIA_STACK_GUARD) {
    heap_->isolate()->stack_guard()->RequestGC();
  }
}


void IncrementalMarking::Epilogue() {
  was_activated_ = false;
  finalize_marking_completed_ = false;
}

double IncrementalMarking::AdvanceIncrementalMarking(
    double deadline_in_ms, CompletionAction completion_action,
    StepOrigin step_origin) {
  HistogramTimerScope incremental_marking_scope(
      heap_->isolate()->counters()->gc_incremental_marking());
  TRACE_EVENT0("v8", "V8.GCIncrementalMarking");
  TRACE_GC(heap_->tracer(), GCTracer::Scope::MC_INCREMENTAL);
  DCHECK(!IsStopped());
  DCHECK_EQ(
      0, heap_->local_embedder_heap_tracer()->NumberOfCachedWrappersToTrace());

  double remaining_time_in_ms = 0.0;
  intptr_t step_size_in_bytes = GCIdleTimeHandler::EstimateMarkingStepSize(
      kStepSizeInMs,
      heap()->tracer()->IncrementalMarkingSpeedInBytesPerMillisecond());

  const bool incremental_wrapper_tracing =
      state_ == MARKING && FLAG_incremental_marking_wrappers &&
      heap_->local_embedder_heap_tracer()->InUse();
  do {
    if (incremental_wrapper_tracing && trace_wrappers_toggle_) {
      EmbedderStep(kStepSizeInMs);
    } else {
      Step(step_size_in_bytes, completion_action, step_origin);
    }
    trace_wrappers_toggle_ = !trace_wrappers_toggle_;
    remaining_time_in_ms =
        deadline_in_ms - heap()->MonotonicallyIncreasingTimeInMs();
  } while (remaining_time_in_ms >= kStepSizeInMs && !IsComplete() &&
           !marking_worklist()->IsEmpty());
  return remaining_time_in_ms;
}


void IncrementalMarking::FinalizeSweeping() {
  DCHECK(state_ == SWEEPING);
  if (heap_->mark_compact_collector()->sweeping_in_progress() &&
      (!FLAG_concurrent_sweeping ||
       !heap_->mark_compact_collector()->sweeper()->AreSweeperTasksRunning())) {
    heap_->mark_compact_collector()->EnsureSweepingCompleted();
  }
  if (!heap_->mark_compact_collector()->sweeping_in_progress()) {
#ifdef DEBUG
    heap_->VerifyCountersAfterSweeping();
#endif
    StartMarking();
  }
}

size_t IncrementalMarking::StepSizeToKeepUpWithAllocations() {
  // Update bytes_allocated_ based on the allocation counter.
  size_t current_counter = heap_->OldGenerationAllocationCounter();
  bytes_allocated_ += current_counter - old_generation_allocation_counter_;
  old_generation_allocation_counter_ = current_counter;
  return bytes_allocated_;
}

size_t IncrementalMarking::StepSizeToMakeProgress() {
  // We increase step size gradually based on the time passed in order to
  // leave marking work to standalone tasks. The ramp up duration and the
  // target step count are chosen based on benchmarks.
  const int kRampUpIntervalMs = 300;
  const size_t kTargetStepCount = 256;
  const size_t kTargetStepCountAtOOM = 32;
  size_t oom_slack = heap()->new_space()->Capacity() + 64 * MB;

  if (!heap()->CanExpandOldGeneration(oom_slack)) {
    return heap()->OldGenerationSizeOfObjects() / kTargetStepCountAtOOM;
  }

  size_t step_size = Max(initial_old_generation_size_ / kTargetStepCount,
                         IncrementalMarking::kMinStepSizeInBytes);
  double time_passed_ms =
      heap_->MonotonicallyIncreasingTimeInMs() - start_time_ms_;
  double factor = Min(time_passed_ms / kRampUpIntervalMs, 1.0);
  return static_cast<size_t>(factor * step_size);
}

void IncrementalMarking::AdvanceIncrementalMarkingOnAllocation() {
  // Code using an AlwaysAllocateScope assumes that the GC state does not
  // change; that implies that no marking steps must be performed.
  if (heap_->gc_state() != Heap::NOT_IN_GC || !FLAG_incremental_marking ||
      (state_ != SWEEPING && state_ != MARKING) || heap_->always_allocate()) {
    return;
  }

  size_t bytes_to_process =
      StepSizeToKeepUpWithAllocations() + StepSizeToMakeProgress();

  if (bytes_to_process >= IncrementalMarking::kMinStepSizeInBytes) {
    HistogramTimerScope incremental_marking_scope(
        heap_->isolate()->counters()->gc_incremental_marking());
    TRACE_EVENT0("v8", "V8.GCIncrementalMarking");
    TRACE_GC(heap_->tracer(), GCTracer::Scope::MC_INCREMENTAL);
    // The first step after Scavenge will see many allocated bytes.
    // Cap the step size to distribute the marking work more uniformly.
    size_t max_step_size = GCIdleTimeHandler::EstimateMarkingStepSize(
        kMaxStepSizeInMs,
        heap()->tracer()->IncrementalMarkingSpeedInBytesPerMillisecond());
    bytes_to_process = Min(bytes_to_process, max_step_size);
    size_t bytes_processed = 0;
    if (FLAG_concurrent_marking) {
      bytes_processed = Step(bytes_to_process, GC_VIA_STACK_GUARD,
                             StepOrigin::kV8, WorklistToProcess::kBailout);
      bytes_to_process = (bytes_processed >= bytes_to_process)
                             ? 0
                             : bytes_to_process - bytes_processed;
      size_t current_bytes_marked_concurrently =
          heap()->concurrent_marking()->TotalMarkedBytes();
      // The concurrent_marking()->TotalMarkedBytes() is not monothonic for a
      // short period of time when a concurrent marking task is finishing.
      if (current_bytes_marked_concurrently > bytes_marked_concurrently_) {
        bytes_marked_ahead_of_schedule_ +=
            current_bytes_marked_concurrently - bytes_marked_concurrently_;
        bytes_marked_concurrently_ = current_bytes_marked_concurrently;
      }
    }
    if (bytes_marked_ahead_of_schedule_ >= bytes_to_process) {
      // Steps performed in tasks and concurrently have put us ahead of
      // schedule. We skip processing of marking dequeue here and thus shift
      // marking time from inside V8 to standalone tasks.
      bytes_marked_ahead_of_schedule_ -= bytes_to_process;
      bytes_processed += bytes_to_process;
      bytes_to_process = IncrementalMarking::kMinStepSizeInBytes;
    }
    bytes_processed += Step(bytes_to_process, GC_VIA_STACK_GUARD,
                            StepOrigin::kV8, WorklistToProcess::kAll);
    bytes_allocated_ -= Min(bytes_allocated_, bytes_processed);
  }
}

size_t IncrementalMarking::Step(size_t bytes_to_process,
                                CompletionAction action, StepOrigin step_origin,
                                WorklistToProcess worklist_to_process) {
  double start = heap_->MonotonicallyIncreasingTimeInMs();

  if (state_ == SWEEPING) {
    TRACE_GC(heap_->tracer(), GCTracer::Scope::MC_INCREMENTAL_SWEEPING);
    FinalizeSweeping();
  }

  size_t bytes_processed = 0;
  if (state_ == MARKING) {
    if (FLAG_concurrent_marking) {
      heap_->new_space()->ResetOriginalTop();
      // It is safe to merge back all objects that were on hold to the shared
      // work list at Step because we are at a safepoint where all objects
      // are properly initialized.
      marking_worklist()->shared()->MergeGlobalPool(
          marking_worklist()->on_hold());
    }

// Only print marking worklist in debug mode to save ~40KB of code size.
#ifdef DEBUG
    if (FLAG_trace_incremental_marking && FLAG_trace_concurrent_marking &&
        FLAG_trace_gc_verbose) {
      marking_worklist()->Print();
    }
#endif

    if (worklist_to_process == WorklistToProcess::kBailout) {
      bytes_processed =
          ProcessMarkingWorklist<WorklistToProcess::kBailout>(bytes_to_process);
    } else {
      bytes_processed =
          ProcessMarkingWorklist<WorklistToProcess::kAll>(bytes_to_process);
    }

    if (step_origin == StepOrigin::kTask) {
      bytes_marked_ahead_of_schedule_ += bytes_processed;
    }

    if (marking_worklist()->IsEmpty()) {
      if (heap_->local_embedder_heap_tracer()
              ->ShouldFinalizeIncrementalMarking()) {
        if (!finalize_marking_completed_) {
          FinalizeMarking(action);
        } else {
          MarkingComplete(action);
        }
      } else {
        heap_->local_embedder_heap_tracer()->NotifyV8MarkingWorklistWasEmpty();
      }
    }
  }
  if (FLAG_concurrent_marking) {
    heap_->concurrent_marking()->RescheduleTasksIfNeeded();
  }

  double end = heap_->MonotonicallyIncreasingTimeInMs();
  double duration = (end - start);
  // Note that we report zero bytes here when sweeping was in progress or
  // when we just started incremental marking. In these cases we did not
  // process the marking deque.
  heap_->tracer()->AddIncrementalMarkingStep(duration, bytes_processed);
  if (FLAG_trace_incremental_marking) {
    heap_->isolate()->PrintWithTimestamp(
        "[IncrementalMarking] Step %s %" PRIuS "KB (%" PRIuS "KB) in %.1f\n",
        step_origin == StepOrigin::kV8 ? "in v8" : "in task",
        bytes_processed / KB, bytes_to_process / KB, duration);
  }
  if (FLAG_trace_concurrent_marking) {
    heap_->isolate()->PrintWithTimestamp(
        "Concurrently marked %" PRIuS "KB\n",
        heap_->concurrent_marking()->TotalMarkedBytes() / KB);
  }
  return bytes_processed;
}

}  // namespace internal
}  // namespace v8
