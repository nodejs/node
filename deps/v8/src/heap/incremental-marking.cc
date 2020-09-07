// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/incremental-marking.h"

#include "src/codegen/compilation-cache.h"
#include "src/execution/vm-state-inl.h"
#include "src/heap/array-buffer-sweeper.h"
#include "src/heap/concurrent-marking.h"
#include "src/heap/embedder-tracing.h"
#include "src/heap/gc-idle-time-handler.h"
#include "src/heap/gc-tracer.h"
#include "src/heap/heap-inl.h"
#include "src/heap/incremental-marking-inl.h"
#include "src/heap/mark-compact-inl.h"
#include "src/heap/marking-visitor-inl.h"
#include "src/heap/marking-visitor.h"
#include "src/heap/memory-chunk.h"
#include "src/heap/object-stats.h"
#include "src/heap/objects-visiting-inl.h"
#include "src/heap/objects-visiting.h"
#include "src/heap/safepoint.h"
#include "src/heap/sweeper.h"
#include "src/init/v8.h"
#include "src/numbers/conversions.h"
#include "src/objects/data-handler-inl.h"
#include "src/objects/embedder-data-array-inl.h"
#include "src/objects/hash-table-inl.h"
#include "src/objects/slots-inl.h"
#include "src/objects/transitions-inl.h"
#include "src/objects/visitors.h"
#include "src/tracing/trace-event.h"
#include "src/utils/utils.h"

namespace v8 {
namespace internal {

void IncrementalMarking::Observer::Step(int bytes_allocated, Address addr,
                                        size_t size) {
  Heap* heap = incremental_marking_->heap();
  VMState<GC> state(heap->isolate());
  RuntimeCallTimerScope runtime_timer(
      heap->isolate(),
      RuntimeCallCounterId::kGC_Custom_IncrementalMarkingObserver);
  incremental_marking_->AdvanceOnAllocation();
  // AdvanceIncrementalMarkingOnAllocation can start incremental marking.
  incremental_marking_->EnsureBlackAllocated(addr, size);
}

IncrementalMarking::IncrementalMarking(Heap* heap,
                                       WeakObjects* weak_objects)
    : heap_(heap),
      collector_(heap->mark_compact_collector()),
      weak_objects_(weak_objects),
      new_generation_observer_(this, kYoungGenerationAllocatedThreshold),
      old_generation_observer_(this, kOldGenerationAllocatedThreshold) {
  SetState(STOPPED);
}

void IncrementalMarking::RecordWriteSlow(HeapObject obj, HeapObjectSlot slot,
                                         HeapObject value) {
  if (BaseRecordWrite(obj, value) && slot.address() != kNullAddress) {
    // Object is not going to be rescanned we need to record the slot.
    collector_->RecordSlot(obj, slot, value);
  }
}

int IncrementalMarking::RecordWriteFromCode(Address raw_obj,
                                            Address slot_address,
                                            Isolate* isolate) {
  HeapObject obj = HeapObject::cast(Object(raw_obj));
  MaybeObjectSlot slot(slot_address);
  isolate->heap()->incremental_marking()->RecordWrite(obj, slot, *slot);
  // Called by RecordWriteCodeStubAssembler, which doesnt accept void type
  return 0;
}

void IncrementalMarking::RecordWriteIntoCode(Code host, RelocInfo* rinfo,
                                             HeapObject value) {
  DCHECK(IsMarking());
  if (BaseRecordWrite(host, value)) {
    // Object is not going to be rescanned.  We need to record the slot.
    collector_->RecordRelocSlot(host, rinfo, value);
  }
}

void IncrementalMarking::MarkBlackAndVisitObjectDueToLayoutChange(
    HeapObject obj) {
  TRACE_EVENT0("v8", "V8.GCIncrementalMarkingLayoutChange");
  TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_INCREMENTAL_LAYOUT_CHANGE);
  marking_state()->WhiteToGrey(obj);
  collector_->VisitObject(obj);
}

void IncrementalMarking::MarkBlackBackground(HeapObject obj, int object_size) {
  MarkBit mark_bit = atomic_marking_state()->MarkBitFrom(obj);
  Marking::MarkBlack<AccessMode::ATOMIC>(mark_bit);
  MemoryChunk* chunk = MemoryChunk::FromHeapObject(obj);
  IncrementLiveBytesBackground(chunk, static_cast<intptr_t>(object_size));
}

void IncrementalMarking::NotifyLeftTrimming(HeapObject from, HeapObject to) {
  DCHECK(IsMarking());
  DCHECK(MemoryChunk::FromHeapObject(from)->SweepingDone());
  DCHECK_EQ(MemoryChunk::FromHeapObject(from), MemoryChunk::FromHeapObject(to));
  DCHECK_NE(from, to);

  MarkBit new_mark_bit = marking_state()->MarkBitFrom(to);

  if (black_allocation() && Marking::IsBlack<kAtomicity>(new_mark_bit)) {
    // Nothing to do if the object is in black area.
    return;
  }
  MarkBlackAndVisitObjectDueToLayoutChange(from);
  DCHECK(marking_state()->IsBlack(from));
  // Mark the new address as black.
  if (from.address() + kTaggedSize == to.address()) {
    // The old and the new markbits overlap. The |to| object has the
    // grey color. To make it black, we need to set the second bit.
    DCHECK(new_mark_bit.Get<kAtomicity>());
    new_mark_bit.Next().Set<kAtomicity>();
  } else {
    bool success = Marking::WhiteToBlack<kAtomicity>(new_mark_bit);
    DCHECK(success);
    USE(success);
  }
  DCHECK(marking_state()->IsBlack(to));
}

class IncrementalMarkingRootMarkingVisitor : public RootVisitor {
 public:
  explicit IncrementalMarkingRootMarkingVisitor(
      IncrementalMarking* incremental_marking)
      : heap_(incremental_marking->heap()) {}

  void VisitRootPointer(Root root, const char* description,
                        FullObjectSlot p) override {
    MarkObjectByPointer(p);
  }

  void VisitRootPointers(Root root, const char* description,
                         FullObjectSlot start, FullObjectSlot end) override {
    for (FullObjectSlot p = start; p < end; ++p) MarkObjectByPointer(p);
  }

 private:
  void MarkObjectByPointer(FullObjectSlot p) {
    Object obj = *p;
    if (!obj.IsHeapObject()) return;

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

  for (LargePage* p : *heap_->new_lo_space()) {
    p->SetYoungGenerationPageFlags(false);
    DCHECK(p->IsLargePage());
  }

  for (LargePage* p : *heap_->lo_space()) {
    p->SetOldGenerationPageFlags(false);
  }

  for (LargePage* p : *heap_->code_lo_space()) {
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

  for (LargePage* p : *heap_->new_lo_space()) {
    p->SetYoungGenerationPageFlags(true);
    DCHECK(p->IsLargePage());
  }

  for (LargePage* p : *heap_->lo_space()) {
    p->SetOldGenerationPageFlags(true);
  }

  for (LargePage* p : *heap_->code_lo_space()) {
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

bool IncrementalMarking::IsBelowActivationThresholds() const {
  return heap_->OldGenerationSizeOfObjects() <= kV8ActivationThreshold &&
         heap_->GlobalSizeOfObjects() <= kGlobalActivationThreshold;
}

void IncrementalMarking::Deactivate() {
  DeactivateIncrementalWriteBarrier();
}

void IncrementalMarking::Start(GarbageCollectionReason gc_reason) {
  if (FLAG_trace_incremental_marking) {
    const size_t old_generation_size_mb =
        heap()->OldGenerationSizeOfObjects() / MB;
    const size_t old_generation_limit_mb =
        heap()->old_generation_allocation_limit() / MB;
    const size_t global_size_mb = heap()->GlobalSizeOfObjects() / MB;
    const size_t global_limit_mb = heap()->global_allocation_limit() / MB;
    heap()->isolate()->PrintWithTimestamp(
        "[IncrementalMarking] Start (%s): (size/limit/slack) v8: %zuMB / %zuMB "
        "/ %zuMB global: %zuMB / %zuMB / %zuMB\n",
        Heap::GarbageCollectionReasonToString(gc_reason),
        old_generation_size_mb, old_generation_limit_mb,
        old_generation_size_mb > old_generation_limit_mb
            ? 0
            : old_generation_limit_mb - old_generation_size_mb,
        global_size_mb, global_limit_mb,
        global_size_mb > global_limit_mb ? 0
                                         : global_limit_mb - global_size_mb);
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
  time_to_force_completion_ = 0.0;
  initial_old_generation_size_ = heap_->OldGenerationSizeOfObjects();
  old_generation_allocation_counter_ = heap_->OldGenerationAllocationCounter();
  bytes_marked_ = 0;
  scheduled_bytes_to_mark_ = 0;
  schedule_update_time_ms_ = start_time_ms_;
  bytes_marked_concurrently_ = 0;
  was_activated_ = true;

  {
    TRACE_GC(heap()->tracer(),
             GCTracer::Scope::MC_INCREMENTAL_SWEEP_ARRAY_BUFFERS);
    heap_->array_buffer_sweeper()->EnsureFinished();
  }

  if (!collector_->sweeping_in_progress()) {
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

  heap_->InvokeIncrementalMarkingPrologueCallbacks();

  is_compacting_ = !FLAG_never_compact && collector_->StartCompaction();
  collector_->StartMarking();

  SetState(MARKING);

  ActivateIncrementalWriteBarrier();

  heap_->isolate()->compilation_cache()->MarkCompactPrologue();

  StartBlackAllocation();

  MarkRoots();

  if (FLAG_concurrent_marking && !heap_->IsTearingDown()) {
    heap_->concurrent_marking()->ScheduleTasks();
  }

  // Ready to start incremental marking.
  if (FLAG_trace_incremental_marking) {
    heap()->isolate()->PrintWithTimestamp("[IncrementalMarking] Running\n");
  }

  {
    // TracePrologue may call back into V8 in corner cases, requiring that
    // marking (including write barriers) is fully set up.
    TRACE_GC(heap()->tracer(),
             GCTracer::Scope::MC_INCREMENTAL_EMBEDDER_PROLOGUE);
    heap_->local_embedder_heap_tracer()->TracePrologue(
        heap_->flags_for_embedder_tracer());
  }

  heap_->InvokeIncrementalMarkingEpilogueCallbacks();
}

void IncrementalMarking::StartBlackAllocation() {
  DCHECK(!black_allocation_);
  DCHECK(IsMarking());
  black_allocation_ = true;
  heap()->old_space()->MarkLinearAllocationAreaBlack();
  heap()->map_space()->MarkLinearAllocationAreaBlack();
  heap()->code_space()->MarkLinearAllocationAreaBlack();
  if (FLAG_local_heaps) {
    heap()->safepoint()->IterateLocalHeaps([](LocalHeap* local_heap) {
      local_heap->MarkLinearAllocationAreaBlack();
    });
  }
  if (FLAG_trace_incremental_marking) {
    heap()->isolate()->PrintWithTimestamp(
        "[IncrementalMarking] Black allocation started\n");
  }
}

void IncrementalMarking::PauseBlackAllocation() {
  DCHECK(IsMarking());
  heap()->old_space()->UnmarkLinearAllocationArea();
  heap()->map_space()->UnmarkLinearAllocationArea();
  heap()->code_space()->UnmarkLinearAllocationArea();
  if (FLAG_local_heaps) {
    heap()->safepoint()->IterateLocalHeaps([](LocalHeap* local_heap) {
      local_heap->UnmarkLinearAllocationArea();
    });
  }
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

void IncrementalMarking::EnsureBlackAllocated(Address allocated, size_t size) {
  if (black_allocation() && allocated != kNullAddress) {
    HeapObject object = HeapObject::FromAddress(allocated);
    if (marking_state()->IsWhite(object) && !Heap::InYoungGeneration(object)) {
      if (heap_->IsLargeObject(object)) {
        marking_state()->WhiteToBlack(object);
      } else {
        Page::FromAddress(allocated)->CreateBlackArea(allocated,
                                                      allocated + size);
      }
    }
  }
}

void IncrementalMarking::MarkRoots() {
  DCHECK(!finalize_marking_completed_);
  DCHECK(IsMarking());

  IncrementalMarkingRootMarkingVisitor visitor(this);
  heap_->IterateRoots(
      &visitor, base::EnumSet<SkipRoot>{SkipRoot::kStack, SkipRoot::kWeak});
}

bool IncrementalMarking::ShouldRetainMap(Map map, int age) {
  if (age == 0) {
    // The map has aged. Do not retain this map.
    return false;
  }
  Object constructor = map.GetConstructor();
  if (!constructor.IsHeapObject() ||
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
  std::vector<WeakArrayList> retained_maps_list = heap()->FindAllRetainedMaps();

  for (WeakArrayList retained_maps : retained_maps_list) {
    int length = retained_maps.length();

    for (int i = 0; i < length; i += 2) {
      MaybeObject value = retained_maps.Get(i);
      HeapObject map_heap_object;
      if (!value->GetHeapObjectIfWeak(&map_heap_object)) {
        continue;
      }
      int age = retained_maps.Get(i + 1).ToSmi().value();
      int new_age;
      Map map = Map::cast(map_heap_object);
      if (!map_retaining_is_disabled && marking_state()->IsWhite(map)) {
        if (ShouldRetainMap(map, age)) {
          WhiteToGreyAndPush(map);
        }
        Object prototype = map.prototype();
        if (age > 0 && prototype.IsHeapObject() &&
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
        retained_maps.Set(i + 1, MaybeObject::FromSmi(Smi::FromInt(new_age)));
      }
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

  if (FLAG_trace_incremental_marking) {
    double end = heap_->MonotonicallyIncreasingTimeInMs();
    double delta = end - start;
    heap()->isolate()->PrintWithTimestamp(
        "[IncrementalMarking] Finalize incrementally spent %.1f ms.\n", delta);
  }
}

void IncrementalMarking::UpdateMarkingWorklistAfterScavenge() {
  if (!IsMarking()) return;

  Map filler_map = ReadOnlyRoots(heap_).one_pointer_filler_map();

#ifdef ENABLE_MINOR_MC
  MinorMarkCompactCollector::MarkingState* minor_marking_state =
      heap()->minor_mark_compact_collector()->marking_state();
#endif  // ENABLE_MINOR_MC

  collector_->marking_worklists_holder()->Update(
      [
#ifdef DEBUG
          // this is referred inside DCHECK.
          this,
#endif
#ifdef ENABLE_MINOR_MC
          minor_marking_state,
#endif
          filler_map](HeapObject obj, HeapObject* out) -> bool {
        DCHECK(obj.IsHeapObject());
        // Only pointers to from space have to be updated.
        if (Heap::InFromPage(obj)) {
          MapWord map_word = obj.map_word();
          if (!map_word.IsForwardingAddress()) {
            // There may be objects on the marking deque that do not exist
            // anymore, e.g. left trimmed objects or objects from the root set
            // (frames). If these object are dead at scavenging time, their
            // marking deque entries will not point to forwarding addresses.
            // Hence, we can discard them.
            return false;
          }
          HeapObject dest = map_word.ToForwardingAddress();
          DCHECK_IMPLIES(marking_state()->IsWhite(obj),
                         obj.IsFreeSpaceOrFiller());
          *out = dest;
          return true;
        } else if (Heap::InToPage(obj)) {
          // The object may be on a large page or on a page that was moved in
          // new space.
          DCHECK(Heap::IsLargeObject(obj) ||
                 Page::FromHeapObject(obj)->IsFlagSet(Page::SWEEP_TO_ITERATE));
#ifdef ENABLE_MINOR_MC
          if (minor_marking_state->IsWhite(obj)) {
            return false;
          }
#endif  // ENABLE_MINOR_MC
        // Either a large object or an object marked by the minor
        // mark-compactor.
          *out = obj;
          return true;
        } else {
          // The object may be on a page that was moved from new to old space.
          // Only applicable during minor MC garbage collections.
          if (Page::FromHeapObject(obj)->IsFlagSet(Page::SWEEP_TO_ITERATE)) {
#ifdef ENABLE_MINOR_MC
            if (minor_marking_state->IsWhite(obj)) {
              return false;
            }
#endif  // ENABLE_MINOR_MC
            *out = obj;
            return true;
          }
          DCHECK_IMPLIES(marking_state()->IsWhite(obj),
                         obj.IsFreeSpaceOrFiller());
          // Skip one word filler objects that appear on the
          // stack when we perform in place array shift.
          if (obj.map() != filler_map) {
            *out = obj;
            return true;
          }
          return false;
        }
      });

  UpdateWeakReferencesAfterScavenge();
}

void IncrementalMarking::UpdateWeakReferencesAfterScavenge() {
  weak_objects_->weak_references.Update(
      [](std::pair<HeapObject, HeapObjectSlot> slot_in,
         std::pair<HeapObject, HeapObjectSlot>* slot_out) -> bool {
        HeapObject heap_obj = slot_in.first;
        HeapObject forwarded = ForwardingAddress(heap_obj);

        if (!forwarded.is_null()) {
          ptrdiff_t distance_to_slot =
              slot_in.second.address() - slot_in.first.ptr();
          Address new_slot = forwarded.ptr() + distance_to_slot;
          slot_out->first = forwarded;
          slot_out->second = HeapObjectSlot(new_slot);
          return true;
        }

        return false;
      });
  weak_objects_->weak_objects_in_code.Update(
      [](std::pair<HeapObject, Code> slot_in,
         std::pair<HeapObject, Code>* slot_out) -> bool {
        HeapObject heap_obj = slot_in.first;
        HeapObject forwarded = ForwardingAddress(heap_obj);

        if (!forwarded.is_null()) {
          slot_out->first = forwarded;
          slot_out->second = slot_in.second;
          return true;
        }

        return false;
      });
  weak_objects_->ephemeron_hash_tables.Update(
      [](EphemeronHashTable slot_in, EphemeronHashTable* slot_out) -> bool {
        EphemeronHashTable forwarded = ForwardingAddress(slot_in);

        if (!forwarded.is_null()) {
          *slot_out = forwarded;
          return true;
        }

        return false;
      });

  auto ephemeron_updater = [](Ephemeron slot_in, Ephemeron* slot_out) -> bool {
    HeapObject key = slot_in.key;
    HeapObject value = slot_in.value;
    HeapObject forwarded_key = ForwardingAddress(key);
    HeapObject forwarded_value = ForwardingAddress(value);

    if (!forwarded_key.is_null() && !forwarded_value.is_null()) {
      *slot_out = Ephemeron{forwarded_key, forwarded_value};
      return true;
    }

    return false;
  };

  weak_objects_->current_ephemerons.Update(ephemeron_updater);
  weak_objects_->next_ephemerons.Update(ephemeron_updater);
  weak_objects_->discovered_ephemerons.Update(ephemeron_updater);

  weak_objects_->flushed_js_functions.Update(
      [](JSFunction slot_in, JSFunction* slot_out) -> bool {
        JSFunction forwarded = ForwardingAddress(slot_in);

        if (!forwarded.is_null()) {
          *slot_out = forwarded;
          return true;
        }

        return false;
      });
#ifdef DEBUG
  weak_objects_->bytecode_flushing_candidates.Iterate(
      [](SharedFunctionInfo candidate) {
        DCHECK(!Heap::InYoungGeneration(candidate));
      });
#endif

  if (FLAG_harmony_weak_refs) {
    weak_objects_->js_weak_refs.Update(
        [](JSWeakRef js_weak_ref_in, JSWeakRef* js_weak_ref_out) -> bool {
          JSWeakRef forwarded = ForwardingAddress(js_weak_ref_in);

          if (!forwarded.is_null()) {
            *js_weak_ref_out = forwarded;
            return true;
          }

          return false;
        });

#ifdef DEBUG
    // TODO(syg, marja): Support WeakCells in the young generation.
    weak_objects_->weak_cells.Iterate([](WeakCell weak_cell) {
      DCHECK(!Heap::InYoungGeneration(weak_cell));
    });
#endif
  }
}

void IncrementalMarking::UpdateMarkedBytesAfterScavenge(
    size_t dead_bytes_in_new_space) {
  if (!IsMarking()) return;
  bytes_marked_ -= Min(bytes_marked_, dead_bytes_in_new_space);
}

void IncrementalMarking::ProcessBlackAllocatedObject(HeapObject obj) {
  if (IsMarking() && marking_state()->IsBlack(obj)) {
    collector_->RevisitObject(obj);
  }
}

StepResult IncrementalMarking::EmbedderStep(double expected_duration_ms,
                                            double* duration_ms) {
  if (!ShouldDoEmbedderStep()) {
    *duration_ms = 0.0;
    return StepResult::kNoImmediateWork;
  }

  constexpr size_t kObjectsToProcessBeforeDeadlineCheck = 500;

  TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_INCREMENTAL_EMBEDDER_TRACING);
  LocalEmbedderHeapTracer* local_tracer = heap_->local_embedder_heap_tracer();
  const double start = heap_->MonotonicallyIncreasingTimeInMs();
  const double deadline = start + expected_duration_ms;
  bool empty_worklist;
  {
    LocalEmbedderHeapTracer::ProcessingScope scope(local_tracer);
    HeapObject object;
    size_t cnt = 0;
    empty_worklist = true;
    while (marking_worklists()->PopEmbedder(&object)) {
      scope.TracePossibleWrapper(JSObject::cast(object));
      if (++cnt == kObjectsToProcessBeforeDeadlineCheck) {
        if (deadline <= heap_->MonotonicallyIncreasingTimeInMs()) {
          empty_worklist = false;
          break;
        }
        cnt = 0;
      }
    }
  }
  // |deadline - heap_->MonotonicallyIncreasingTimeInMs()| could be negative,
  // which means |local_tracer| won't do any actual tracing, so there is no
  // need to check for |deadline <= heap_->MonotonicallyIncreasingTimeInMs()|.
  bool remote_tracing_done =
      local_tracer->Trace(deadline - heap_->MonotonicallyIncreasingTimeInMs());
  double current = heap_->MonotonicallyIncreasingTimeInMs();
  local_tracer->SetEmbedderWorklistEmpty(empty_worklist);
  *duration_ms = current - start;
  return (empty_worklist && remote_tracing_done)
             ? StepResult::kNoImmediateWork
             : StepResult::kMoreWorkRemaining;
}

void IncrementalMarking::Hurry() {
  if (!marking_worklists()->IsEmpty()) {
    double start = 0.0;
    if (FLAG_trace_incremental_marking) {
      start = heap_->MonotonicallyIncreasingTimeInMs();
      if (FLAG_trace_incremental_marking) {
        heap()->isolate()->PrintWithTimestamp("[IncrementalMarking] Hurry\n");
      }
    }
    collector_->ProcessMarkingWorklist(0);
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
  while (it.HasNext()) {
    Space* space = it.Next();
    if (space == heap_->new_space()) {
      space->RemoveAllocationObserver(&new_generation_observer_);
    } else {
      space->RemoveAllocationObserver(&old_generation_observer_);
    }
  }

  heap_->isolate()->stack_guard()->ClearGC();
  SetState(STOPPED);
  is_compacting_ = false;
  FinishBlackAllocation();

  if (FLAG_local_heaps) {
    // Merge live bytes counters of background threads
    for (auto pair : background_live_bytes_) {
      MemoryChunk* memory_chunk = pair.first;
      intptr_t live_bytes = pair.second;

      if (live_bytes) {
        marking_state()->IncrementLiveBytes(memory_chunk, live_bytes);
      }
    }

    background_live_bytes_.clear();
  }
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

double IncrementalMarking::CurrentTimeToMarkingTask() const {
  const double recorded_time_to_marking_task =
      heap_->tracer()->AverageTimeToIncrementalMarkingTask();
  const double current_time_to_marking_task =
      incremental_marking_job_.CurrentTimeToTask(heap_);
  if (recorded_time_to_marking_task == 0.0) return 0.0;
  return Max(recorded_time_to_marking_task, current_time_to_marking_task);
}

void IncrementalMarking::MarkingComplete(CompletionAction action) {
  // Allowed overshoot percantage of incremental marking walltime.
  constexpr double kAllowedOvershoot = 0.1;
  // Minimum overshoot in ms. This is used to allow moving away from stack when
  // marking was fast.
  constexpr double kMinOvershootMs = 50;

  if (action == GC_VIA_STACK_GUARD) {
    if (time_to_force_completion_ == 0.0) {
      const double now = heap_->MonotonicallyIncreasingTimeInMs();
      const double overshoot_ms =
          Max(kMinOvershootMs, (now - start_time_ms_) * kAllowedOvershoot);
      const double time_to_marking_task = CurrentTimeToMarkingTask();
      if (time_to_marking_task == 0.0 || time_to_marking_task > overshoot_ms) {
        if (FLAG_trace_incremental_marking) {
          heap()->isolate()->PrintWithTimestamp(
              "[IncrementalMarking] Not delaying marking completion. time to "
              "task: %fms allowed overshoot: %fms\n",
              time_to_marking_task, overshoot_ms);
        }
      } else {
        time_to_force_completion_ = now + overshoot_ms;
        if (FLAG_trace_incremental_marking) {
          heap()->isolate()->PrintWithTimestamp(
              "[IncrementalMarking] Delaying GC via stack guard. time to task: "
              "%fms "
              "allowed overshoot: %fms\n",
              time_to_marking_task, overshoot_ms);
        }
        incremental_marking_job_.ScheduleTask(
            heap(), IncrementalMarkingJob::TaskType::kNormal);
        return;
      }
    }
    if (heap()->MonotonicallyIncreasingTimeInMs() < time_to_force_completion_) {
      if (FLAG_trace_incremental_marking) {
        heap()->isolate()->PrintWithTimestamp(
            "[IncrementalMarking] Delaying GC via stack guard. time left: "
            "%fms\n",
            time_to_force_completion_ -
                heap_->MonotonicallyIncreasingTimeInMs());
      }
      return;
    }
  }

  SetState(COMPLETE);
  // We will set the stack guard to request a GC now.  This will mean the rest
  // of the GC gets performed as soon as possible (we can't do a GC here in a
  // record-write context).  If a few things get allocated between now and then
  // that shouldn't make us do a scavenge and keep being incremental.
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

bool IncrementalMarking::ShouldDoEmbedderStep() {
  return state_ == MARKING && FLAG_incremental_marking_wrappers &&
         heap_->local_embedder_heap_tracer()->InUse();
}

void IncrementalMarking::FastForwardSchedule() {
  if (scheduled_bytes_to_mark_ < bytes_marked_) {
    scheduled_bytes_to_mark_ = bytes_marked_;
    if (FLAG_trace_incremental_marking) {
      heap_->isolate()->PrintWithTimestamp(
          "[IncrementalMarking] Fast-forwarded schedule\n");
    }
  }
}

void IncrementalMarking::FastForwardScheduleIfCloseToFinalization() {
  // Consider marking close to finalization if 75% of the initial old
  // generation was marked.
  if (bytes_marked_ > 3 * (initial_old_generation_size_ / 4)) {
    FastForwardSchedule();
  }
}

void IncrementalMarking::ScheduleBytesToMarkBasedOnTime(double time_ms) {
  // Time interval that should be sufficient to complete incremental marking.
  constexpr double kTargetMarkingWallTimeInMs = 500;
  constexpr double kMinTimeBetweenScheduleInMs = 10;
  if (schedule_update_time_ms_ + kMinTimeBetweenScheduleInMs > time_ms) return;
  double delta_ms =
      Min(time_ms - schedule_update_time_ms_, kTargetMarkingWallTimeInMs);
  schedule_update_time_ms_ = time_ms;

  size_t bytes_to_mark =
      (delta_ms / kTargetMarkingWallTimeInMs) * initial_old_generation_size_;
  AddScheduledBytesToMark(bytes_to_mark);

  if (FLAG_trace_incremental_marking) {
    heap_->isolate()->PrintWithTimestamp(
        "[IncrementalMarking] Scheduled %zuKB to mark based on time delta "
        "%.1fms\n",
        bytes_to_mark / KB, delta_ms);
  }
}

namespace {
StepResult CombineStepResults(StepResult a, StepResult b) {
  DCHECK_NE(StepResult::kWaitingForFinalization, a);
  DCHECK_NE(StepResult::kWaitingForFinalization, b);
  if (a == StepResult::kMoreWorkRemaining ||
      b == StepResult::kMoreWorkRemaining)
    return StepResult::kMoreWorkRemaining;
  return StepResult::kNoImmediateWork;
}
}  // anonymous namespace

StepResult IncrementalMarking::AdvanceWithDeadline(
    double deadline_in_ms, CompletionAction completion_action,
    StepOrigin step_origin) {
  HistogramTimerScope incremental_marking_scope(
      heap_->isolate()->counters()->gc_incremental_marking());
  TRACE_EVENT0("v8", "V8.GCIncrementalMarking");
  TRACE_GC(heap_->tracer(), GCTracer::Scope::MC_INCREMENTAL);
  DCHECK(!IsStopped());

  ScheduleBytesToMarkBasedOnTime(heap()->MonotonicallyIncreasingTimeInMs());
  FastForwardScheduleIfCloseToFinalization();
  return Step(kStepSizeInMs, completion_action, step_origin);
}

void IncrementalMarking::FinalizeSweeping() {
  DCHECK(state_ == SWEEPING);
  if (ContinueConcurrentSweeping()) {
    if (FLAG_stress_incremental_marking) {
      // To start concurrent marking a bit earlier, support concurrent sweepers
      // from main thread by sweeping some pages.
      SupportConcurrentSweeping();
    }
    return;
  }

  SafepointScope scope(heap());
  collector_->EnsureSweepingCompleted();
  DCHECK(!collector_->sweeping_in_progress());
#ifdef DEBUG
  heap_->VerifyCountersAfterSweeping();
#endif
  StartMarking();
}

bool IncrementalMarking::ContinueConcurrentSweeping() {
  if (!collector_->sweeping_in_progress()) return false;
  return FLAG_concurrent_sweeping &&
         collector_->sweeper()->AreSweeperTasksRunning();
}

void IncrementalMarking::SupportConcurrentSweeping() {
  collector_->sweeper()->SupportConcurrentSweeping();
}

size_t IncrementalMarking::StepSizeToKeepUpWithAllocations() {
  // Update bytes_allocated_ based on the allocation counter.
  size_t current_counter = heap_->OldGenerationAllocationCounter();
  size_t result = current_counter - old_generation_allocation_counter_;
  old_generation_allocation_counter_ = current_counter;
  return result;
}

size_t IncrementalMarking::StepSizeToMakeProgress() {
  const size_t kTargetStepCount = 256;
  const size_t kTargetStepCountAtOOM = 32;
  const size_t kMaxStepSizeInByte = 256 * KB;
  size_t oom_slack = heap()->new_space()->Capacity() + 64 * MB;

  if (!heap()->CanExpandOldGeneration(oom_slack)) {
    return heap()->OldGenerationSizeOfObjects() / kTargetStepCountAtOOM;
  }

  return Min(Max(initial_old_generation_size_ / kTargetStepCount,
                 IncrementalMarking::kMinStepSizeInBytes),
             kMaxStepSizeInByte);
}

void IncrementalMarking::AddScheduledBytesToMark(size_t bytes_to_mark) {
  if (scheduled_bytes_to_mark_ + bytes_to_mark < scheduled_bytes_to_mark_) {
    // The overflow case.
    scheduled_bytes_to_mark_ = std::numeric_limits<std::size_t>::max();
  } else {
    scheduled_bytes_to_mark_ += bytes_to_mark;
  }
}

void IncrementalMarking::ScheduleBytesToMarkBasedOnAllocation() {
  size_t progress_bytes = StepSizeToMakeProgress();
  size_t allocation_bytes = StepSizeToKeepUpWithAllocations();
  size_t bytes_to_mark = progress_bytes + allocation_bytes;
  AddScheduledBytesToMark(bytes_to_mark);

  if (FLAG_trace_incremental_marking) {
    heap_->isolate()->PrintWithTimestamp(
        "[IncrementalMarking] Scheduled %zuKB to mark based on allocation "
        "(progress=%zuKB, allocation=%zuKB)\n",
        bytes_to_mark / KB, progress_bytes / KB, allocation_bytes / KB);
  }
}

void IncrementalMarking::FetchBytesMarkedConcurrently() {
  if (FLAG_concurrent_marking) {
    size_t current_bytes_marked_concurrently =
        heap()->concurrent_marking()->TotalMarkedBytes();
    // The concurrent_marking()->TotalMarkedBytes() is not monothonic for a
    // short period of time when a concurrent marking task is finishing.
    if (current_bytes_marked_concurrently > bytes_marked_concurrently_) {
      bytes_marked_ +=
          current_bytes_marked_concurrently - bytes_marked_concurrently_;
      bytes_marked_concurrently_ = current_bytes_marked_concurrently;
    }
    if (FLAG_trace_incremental_marking) {
      heap_->isolate()->PrintWithTimestamp(
          "[IncrementalMarking] Marked %zuKB on background threads\n",
          heap_->concurrent_marking()->TotalMarkedBytes() / KB);
    }
  }
}

size_t IncrementalMarking::ComputeStepSizeInBytes(StepOrigin step_origin) {
  FetchBytesMarkedConcurrently();
  if (FLAG_trace_incremental_marking) {
    if (scheduled_bytes_to_mark_ > bytes_marked_) {
      heap_->isolate()->PrintWithTimestamp(
          "[IncrementalMarking] Marker is %zuKB behind schedule\n",
          (scheduled_bytes_to_mark_ - bytes_marked_) / KB);
    } else {
      heap_->isolate()->PrintWithTimestamp(
          "[IncrementalMarking] Marker is %zuKB ahead of schedule\n",
          (bytes_marked_ - scheduled_bytes_to_mark_) / KB);
    }
  }
  // Allow steps on allocation to get behind the schedule by small ammount.
  // This gives higher priority to steps in tasks.
  size_t kScheduleMarginInBytes = step_origin == StepOrigin::kV8 ? 1 * MB : 0;
  if (bytes_marked_ + kScheduleMarginInBytes > scheduled_bytes_to_mark_)
    return 0;
  return scheduled_bytes_to_mark_ - bytes_marked_ - kScheduleMarginInBytes;
}

void IncrementalMarking::AdvanceOnAllocation() {
  // Code using an AlwaysAllocateScope assumes that the GC state does not
  // change; that implies that no marking steps must be performed.
  if (heap_->gc_state() != Heap::NOT_IN_GC || !FLAG_incremental_marking ||
      (state_ != SWEEPING && state_ != MARKING) || heap_->always_allocate()) {
    return;
  }
  HistogramTimerScope incremental_marking_scope(
      heap_->isolate()->counters()->gc_incremental_marking());
  TRACE_EVENT0("v8", "V8.GCIncrementalMarking");
  TRACE_GC(heap_->tracer(), GCTracer::Scope::MC_INCREMENTAL);
  ScheduleBytesToMarkBasedOnAllocation();
  Step(kMaxStepSizeInMs, GC_VIA_STACK_GUARD, StepOrigin::kV8);
}

StepResult IncrementalMarking::Step(double max_step_size_in_ms,
                                    CompletionAction action,
                                    StepOrigin step_origin) {
  double start = heap_->MonotonicallyIncreasingTimeInMs();

  if (state_ == SWEEPING) {
    TRACE_GC(heap_->tracer(), GCTracer::Scope::MC_INCREMENTAL_SWEEPING);
    FinalizeSweeping();
  }

  StepResult combined_result = StepResult::kMoreWorkRemaining;
  size_t bytes_to_process = 0;
  size_t v8_bytes_processed = 0;
  double embedder_duration = 0.0;
  double embedder_deadline = 0.0;
  if (state_ == MARKING) {
    if (FLAG_concurrent_marking) {
      heap_->new_space()->ResetOriginalTop();
      heap_->new_lo_space()->ResetPendingObject();
      // It is safe to merge back all objects that were on hold to the shared
      // work list at Step because we are at a safepoint where all objects
      // are properly initialized.
      marking_worklists()->MergeOnHold();
    }

// Only print marking worklist in debug mode to save ~40KB of code size.
#ifdef DEBUG
    if (FLAG_trace_incremental_marking && FLAG_trace_concurrent_marking &&
        FLAG_trace_gc_verbose) {
      collector_->marking_worklists_holder()->Print();
    }
#endif
    if (FLAG_trace_incremental_marking) {
      heap_->isolate()->PrintWithTimestamp(
          "[IncrementalMarking] Marking speed %.fKB/ms\n",
          heap()->tracer()->IncrementalMarkingSpeedInBytesPerMillisecond());
    }
    // The first step after Scavenge will see many allocated bytes.
    // Cap the step size to distribute the marking work more uniformly.
    const double marking_speed =
        heap()->tracer()->IncrementalMarkingSpeedInBytesPerMillisecond();
    size_t max_step_size = GCIdleTimeHandler::EstimateMarkingStepSize(
        max_step_size_in_ms, marking_speed);
    bytes_to_process = Min(ComputeStepSizeInBytes(step_origin), max_step_size);
    bytes_to_process = Max(bytes_to_process, kMinStepSizeInBytes);

    // Perform a single V8 and a single embedder step. In case both have been
    // observed as empty back to back, we can finalize.
    //
    // This ignores that case where the embedder finds new V8-side objects. The
    // assumption is that large graphs are well connected and can mostly be
    // processed on their own. For small graphs, helping is not necessary.
    v8_bytes_processed = collector_->ProcessMarkingWorklist(bytes_to_process);
    StepResult v8_result = marking_worklists()->IsEmpty()
                               ? StepResult::kNoImmediateWork
                               : StepResult::kMoreWorkRemaining;
    StepResult embedder_result = StepResult::kNoImmediateWork;
    if (heap_->local_embedder_heap_tracer()->InUse()) {
      embedder_deadline =
          Min(max_step_size_in_ms,
              static_cast<double>(bytes_to_process) / marking_speed);
      embedder_result = EmbedderStep(embedder_deadline, &embedder_duration);
    }
    bytes_marked_ += v8_bytes_processed;
    combined_result = CombineStepResults(v8_result, embedder_result);

    if (combined_result == StepResult::kNoImmediateWork) {
      if (!finalize_marking_completed_) {
        FinalizeMarking(action);
        FastForwardSchedule();
        combined_result = StepResult::kWaitingForFinalization;
        incremental_marking_job()->Start(heap_);
      } else {
        MarkingComplete(action);
        combined_result = StepResult::kWaitingForFinalization;
      }
    }
    if (FLAG_concurrent_marking) {
      marking_worklists()->ShareWorkIfGlobalPoolIsEmpty();
      heap_->concurrent_marking()->RescheduleTasksIfNeeded();
    }
  }
  if (state_ == MARKING) {
    // Note that we do not report any marked by in case of finishing sweeping as
    // we did not process the marking worklist.
    const double v8_duration =
        heap_->MonotonicallyIncreasingTimeInMs() - start - embedder_duration;
    heap_->tracer()->AddIncrementalMarkingStep(v8_duration, v8_bytes_processed);
  }
  if (FLAG_trace_incremental_marking) {
    heap_->isolate()->PrintWithTimestamp(
        "[IncrementalMarking] Step %s V8: %zuKB (%zuKB), embedder: %fms (%fms) "
        "in %.1f\n",
        step_origin == StepOrigin::kV8 ? "in v8" : "in task",
        v8_bytes_processed / KB, bytes_to_process / KB, embedder_duration,
        embedder_deadline, heap_->MonotonicallyIncreasingTimeInMs() - start);
  }
  return combined_result;
}

}  // namespace internal
}  // namespace v8
