// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/incremental-marking.h"

#include "src/code-stubs.h"
#include "src/compilation-cache.h"
#include "src/conversions.h"
#include "src/heap/gc-idle-time-handler.h"
#include "src/heap/gc-tracer.h"
#include "src/heap/mark-compact-inl.h"
#include "src/heap/objects-visiting.h"
#include "src/heap/objects-visiting-inl.h"
#include "src/tracing/trace-event.h"
#include "src/v8.h"

namespace v8 {
namespace internal {

IncrementalMarking::StepActions IncrementalMarking::IdleStepActions() {
  return StepActions(IncrementalMarking::NO_GC_VIA_STACK_GUARD,
                     IncrementalMarking::FORCE_MARKING,
                     IncrementalMarking::DO_NOT_FORCE_COMPLETION);
}

IncrementalMarking::IncrementalMarking(Heap* heap)
    : heap_(heap),
      observer_(*this, kAllocatedThreshold),
      state_(STOPPED),
      is_compacting_(false),
      steps_count_(0),
      old_generation_space_available_at_start_of_incremental_(0),
      old_generation_space_used_at_start_of_incremental_(0),
      bytes_rescanned_(0),
      should_hurry_(false),
      marking_speed_(0),
      bytes_scanned_(0),
      allocated_(0),
      write_barriers_invoked_since_last_step_(0),
      idle_marking_delay_counter_(0),
      no_marking_scope_depth_(0),
      unscanned_bytes_of_large_object_(0),
      was_activated_(false),
      finalize_marking_completed_(false),
      incremental_marking_finalization_rounds_(0),
      request_type_(COMPLETE_MARKING) {}

bool IncrementalMarking::BaseRecordWrite(HeapObject* obj, Object* value) {
  HeapObject* value_heap_obj = HeapObject::cast(value);
  MarkBit value_bit = Marking::MarkBitFrom(value_heap_obj);
  DCHECK(!Marking::IsImpossible(value_bit));

  MarkBit obj_bit = Marking::MarkBitFrom(obj);
  DCHECK(!Marking::IsImpossible(obj_bit));
  bool is_black = Marking::IsBlack(obj_bit);

  if (is_black && Marking::IsWhite(value_bit)) {
    WhiteToGreyAndPush(value_heap_obj, value_bit);
    RestartIfNotMarking();
  }
  return is_compacting_ && is_black;
}


void IncrementalMarking::RecordWriteSlow(HeapObject* obj, Object** slot,
                                         Object* value) {
  if (BaseRecordWrite(obj, value) && slot != NULL) {
    // Object is not going to be rescanned we need to record the slot.
    heap_->mark_compact_collector()->RecordSlot(obj, slot, value);
  }
}


void IncrementalMarking::RecordWriteFromCode(HeapObject* obj, Object** slot,
                                             Isolate* isolate) {
  DCHECK(obj->IsHeapObject());
  IncrementalMarking* marking = isolate->heap()->incremental_marking();

  MemoryChunk* chunk = MemoryChunk::FromAddress(obj->address());
  int counter = chunk->write_barrier_counter();
  if (counter < (MemoryChunk::kWriteBarrierCounterGranularity / 2)) {
    marking->write_barriers_invoked_since_last_step_ +=
        MemoryChunk::kWriteBarrierCounterGranularity -
        chunk->write_barrier_counter();
    chunk->set_write_barrier_counter(
        MemoryChunk::kWriteBarrierCounterGranularity);
  }

  marking->RecordWrite(obj, slot, *slot);
}

// static
void IncrementalMarking::RecordWriteOfCodeEntryFromCode(JSFunction* host,
                                                        Object** slot,
                                                        Isolate* isolate) {
  DCHECK(host->IsJSFunction());
  IncrementalMarking* marking = isolate->heap()->incremental_marking();
  Code* value = Code::cast(
      Code::GetObjectFromEntryAddress(reinterpret_cast<Address>(slot)));
  marking->RecordWriteOfCodeEntry(host, slot, value);
}

void IncrementalMarking::RecordCodeTargetPatch(Code* host, Address pc,
                                               HeapObject* value) {
  if (IsMarking()) {
    RelocInfo rinfo(heap_->isolate(), pc, RelocInfo::CODE_TARGET, 0, host);
    RecordWriteIntoCode(host, &rinfo, value);
  }
}


void IncrementalMarking::RecordCodeTargetPatch(Address pc, HeapObject* value) {
  if (IsMarking()) {
    Code* host = heap_->isolate()
                     ->inner_pointer_to_code_cache()
                     ->GcSafeFindCodeForInnerPointer(pc);
    RelocInfo rinfo(heap_->isolate(), pc, RelocInfo::CODE_TARGET, 0, host);
    RecordWriteIntoCode(host, &rinfo, value);
  }
}


void IncrementalMarking::RecordWriteOfCodeEntrySlow(JSFunction* host,
                                                    Object** slot,
                                                    Code* value) {
  if (BaseRecordWrite(host, value)) {
    DCHECK(slot != NULL);
    heap_->mark_compact_collector()->RecordCodeEntrySlot(
        host, reinterpret_cast<Address>(slot), value);
  }
}


void IncrementalMarking::RecordWriteIntoCodeSlow(HeapObject* obj,
                                                 RelocInfo* rinfo,
                                                 Object* value) {
  if (BaseRecordWrite(obj, value)) {
      // Object is not going to be rescanned.  We need to record the slot.
      heap_->mark_compact_collector()->RecordRelocSlot(rinfo, value);
  }
}


void IncrementalMarking::RecordWrites(HeapObject* obj) {
  if (IsMarking()) {
    MarkBit obj_bit = Marking::MarkBitFrom(obj);
    if (Marking::IsBlack(obj_bit)) {
      MemoryChunk* chunk = MemoryChunk::FromAddress(obj->address());
      if (chunk->IsFlagSet(MemoryChunk::HAS_PROGRESS_BAR)) {
        chunk->set_progress_bar(0);
      }
      BlackToGreyAndUnshift(obj, obj_bit);
      RestartIfNotMarking();
    }
  }
}


void IncrementalMarking::BlackToGreyAndUnshift(HeapObject* obj,
                                               MarkBit mark_bit) {
  DCHECK(Marking::MarkBitFrom(obj) == mark_bit);
  DCHECK(obj->Size() >= 2 * kPointerSize);
  DCHECK(IsMarking());
  Marking::BlackToGrey(mark_bit);
  int obj_size = obj->Size();
  MemoryChunk::IncrementLiveBytesFromGC(obj, -obj_size);
  bytes_scanned_ -= obj_size;
  int64_t old_bytes_rescanned = bytes_rescanned_;
  bytes_rescanned_ = old_bytes_rescanned + obj_size;
  if ((bytes_rescanned_ >> 20) != (old_bytes_rescanned >> 20)) {
    if (bytes_rescanned_ > 2 * heap_->PromotedSpaceSizeOfObjects()) {
      // If we have queued twice the heap size for rescanning then we are
      // going around in circles, scanning the same objects again and again
      // as the program mutates the heap faster than we can incrementally
      // trace it.  In this case we switch to non-incremental marking in
      // order to finish off this marking phase.
      if (FLAG_trace_incremental_marking) {
        PrintIsolate(
            heap()->isolate(),
            "Hurrying incremental marking because of lack of progress\n");
      }
      marking_speed_ = kMaxMarkingSpeed;
    }
  }

  heap_->mark_compact_collector()->marking_deque()->Unshift(obj);
}


void IncrementalMarking::WhiteToGreyAndPush(HeapObject* obj, MarkBit mark_bit) {
  Marking::WhiteToGrey(mark_bit);
  heap_->mark_compact_collector()->marking_deque()->Push(obj);
}


static void MarkObjectGreyDoNotEnqueue(Object* obj) {
  if (obj->IsHeapObject()) {
    HeapObject* heap_obj = HeapObject::cast(obj);
    MarkBit mark_bit = Marking::MarkBitFrom(HeapObject::cast(obj));
    if (Marking::IsBlack(mark_bit)) {
      MemoryChunk::IncrementLiveBytesFromGC(heap_obj, -heap_obj->Size());
    }
    Marking::AnyToGrey(mark_bit);
  }
}


static inline void MarkBlackOrKeepBlack(HeapObject* heap_object,
                                        MarkBit mark_bit, int size) {
  DCHECK(!Marking::IsImpossible(mark_bit));
  if (Marking::IsBlack(mark_bit)) return;
  Marking::MarkBlack(mark_bit);
  MemoryChunk::IncrementLiveBytesFromGC(heap_object, size);
}


class IncrementalMarkingMarkingVisitor
    : public StaticMarkingVisitor<IncrementalMarkingMarkingVisitor> {
 public:
  static void Initialize() {
    StaticMarkingVisitor<IncrementalMarkingMarkingVisitor>::Initialize();
    table_.Register(kVisitFixedArray, &VisitFixedArrayIncremental);
    table_.Register(kVisitNativeContext, &VisitNativeContextIncremental);
    table_.Register(kVisitJSRegExp, &VisitJSRegExp);
  }

  static const int kProgressBarScanningChunk = 32 * 1024;

  static void VisitFixedArrayIncremental(Map* map, HeapObject* object) {
    MemoryChunk* chunk = MemoryChunk::FromAddress(object->address());
    // TODO(mstarzinger): Move setting of the flag to the allocation site of
    // the array. The visitor should just check the flag.
    if (FLAG_use_marking_progress_bar &&
        chunk->owner()->identity() == LO_SPACE) {
      chunk->SetFlag(MemoryChunk::HAS_PROGRESS_BAR);
    }
    if (chunk->IsFlagSet(MemoryChunk::HAS_PROGRESS_BAR)) {
      Heap* heap = map->GetHeap();
      // When using a progress bar for large fixed arrays, scan only a chunk of
      // the array and try to push it onto the marking deque again until it is
      // fully scanned. Fall back to scanning it through to the end in case this
      // fails because of a full deque.
      int object_size = FixedArray::BodyDescriptor::SizeOf(map, object);
      int start_offset =
          Max(FixedArray::BodyDescriptor::kStartOffset, chunk->progress_bar());
      int end_offset =
          Min(object_size, start_offset + kProgressBarScanningChunk);
      int already_scanned_offset = start_offset;
      bool scan_until_end = false;
      do {
        VisitPointers(heap, object, HeapObject::RawField(object, start_offset),
                      HeapObject::RawField(object, end_offset));
        start_offset = end_offset;
        end_offset = Min(object_size, end_offset + kProgressBarScanningChunk);
        scan_until_end =
            heap->mark_compact_collector()->marking_deque()->IsFull();
      } while (scan_until_end && start_offset < object_size);
      chunk->set_progress_bar(start_offset);
      if (start_offset < object_size) {
        if (Marking::IsGrey(Marking::MarkBitFrom(object))) {
          heap->mark_compact_collector()->marking_deque()->Unshift(object);
        } else {
          DCHECK(Marking::IsBlack(Marking::MarkBitFrom(object)));
          heap->mark_compact_collector()->UnshiftBlack(object);
        }
        heap->incremental_marking()->NotifyIncompleteScanOfObject(
            object_size - (start_offset - already_scanned_offset));
      }
    } else {
      FixedArrayVisitor::Visit(map, object);
    }
  }

  static void VisitNativeContextIncremental(Map* map, HeapObject* object) {
    Context* context = Context::cast(object);

    // We will mark cache black with a separate pass when we finish marking.
    // Note that GC can happen when the context is not fully initialized,
    // so the cache can be undefined.
    Object* cache = context->get(Context::NORMALIZED_MAP_CACHE_INDEX);
    if (!cache->IsUndefined()) {
      MarkObjectGreyDoNotEnqueue(cache);
    }
    VisitNativeContext(map, context);
  }

  INLINE(static void VisitPointer(Heap* heap, HeapObject* object, Object** p)) {
    Object* target = *p;
    if (target->IsHeapObject()) {
      heap->mark_compact_collector()->RecordSlot(object, p, target);
      MarkObject(heap, target);
    }
  }

  INLINE(static void VisitPointers(Heap* heap, HeapObject* object,
                                   Object** start, Object** end)) {
    for (Object** p = start; p < end; p++) {
      Object* target = *p;
      if (target->IsHeapObject()) {
        heap->mark_compact_collector()->RecordSlot(object, p, target);
        MarkObject(heap, target);
      }
    }
  }

  // Marks the object grey and pushes it on the marking stack.
  INLINE(static void MarkObject(Heap* heap, Object* obj)) {
    IncrementalMarking::MarkObject(heap, HeapObject::cast(obj));
  }

  // Marks the object black without pushing it on the marking stack.
  // Returns true if object needed marking and false otherwise.
  INLINE(static bool MarkObjectWithoutPush(Heap* heap, Object* obj)) {
    HeapObject* heap_object = HeapObject::cast(obj);
    MarkBit mark_bit = Marking::MarkBitFrom(heap_object);
    if (Marking::IsWhite(mark_bit)) {
      Marking::MarkBlack(mark_bit);
      MemoryChunk::IncrementLiveBytesFromGC(heap_object, heap_object->Size());
      return true;
    }
    return false;
  }
};


class IncrementalMarkingRootMarkingVisitor : public ObjectVisitor {
 public:
  explicit IncrementalMarkingRootMarkingVisitor(
      IncrementalMarking* incremental_marking)
      : heap_(incremental_marking->heap()) {}

  void VisitPointer(Object** p) override { MarkObjectByPointer(p); }

  void VisitPointers(Object** start, Object** end) override {
    for (Object** p = start; p < end; p++) MarkObjectByPointer(p);
  }

 private:
  void MarkObjectByPointer(Object** p) {
    Object* obj = *p;
    if (!obj->IsHeapObject()) return;

    IncrementalMarking::MarkObject(heap_, HeapObject::cast(obj));
  }

  Heap* heap_;
};


void IncrementalMarking::Initialize() {
  IncrementalMarkingMarkingVisitor::Initialize();
}


void IncrementalMarking::SetOldSpacePageFlags(MemoryChunk* chunk,
                                              bool is_marking,
                                              bool is_compacting) {
  if (is_marking) {
    chunk->SetFlag(MemoryChunk::POINTERS_TO_HERE_ARE_INTERESTING);
    chunk->SetFlag(MemoryChunk::POINTERS_FROM_HERE_ARE_INTERESTING);
  } else {
    chunk->ClearFlag(MemoryChunk::POINTERS_TO_HERE_ARE_INTERESTING);
    chunk->SetFlag(MemoryChunk::POINTERS_FROM_HERE_ARE_INTERESTING);
  }
}


void IncrementalMarking::SetNewSpacePageFlags(MemoryChunk* chunk,
                                              bool is_marking) {
  chunk->SetFlag(MemoryChunk::POINTERS_TO_HERE_ARE_INTERESTING);
  if (is_marking) {
    chunk->SetFlag(MemoryChunk::POINTERS_FROM_HERE_ARE_INTERESTING);
  } else {
    chunk->ClearFlag(MemoryChunk::POINTERS_FROM_HERE_ARE_INTERESTING);
  }
}


void IncrementalMarking::DeactivateIncrementalWriteBarrierForSpace(
    PagedSpace* space) {
  PageIterator it(space);
  while (it.has_next()) {
    Page* p = it.next();
    SetOldSpacePageFlags(p, false, false);
  }
}


void IncrementalMarking::DeactivateIncrementalWriteBarrierForSpace(
    NewSpace* space) {
  NewSpacePageIterator it(space);
  while (it.has_next()) {
    NewSpacePage* p = it.next();
    SetNewSpacePageFlags(p, false);
  }
}


void IncrementalMarking::DeactivateIncrementalWriteBarrier() {
  DeactivateIncrementalWriteBarrierForSpace(heap_->old_space());
  DeactivateIncrementalWriteBarrierForSpace(heap_->map_space());
  DeactivateIncrementalWriteBarrierForSpace(heap_->code_space());
  DeactivateIncrementalWriteBarrierForSpace(heap_->new_space());

  LargePage* lop = heap_->lo_space()->first_page();
  while (LargePage::IsValid(lop)) {
    SetOldSpacePageFlags(lop, false, false);
    lop = lop->next_page();
  }
}


void IncrementalMarking::ActivateIncrementalWriteBarrier(PagedSpace* space) {
  PageIterator it(space);
  while (it.has_next()) {
    Page* p = it.next();
    SetOldSpacePageFlags(p, true, is_compacting_);
  }
}


void IncrementalMarking::ActivateIncrementalWriteBarrier(NewSpace* space) {
  NewSpacePageIterator it(space->ToSpaceStart(), space->ToSpaceEnd());
  while (it.has_next()) {
    NewSpacePage* p = it.next();
    SetNewSpacePageFlags(p, true);
  }
}


void IncrementalMarking::ActivateIncrementalWriteBarrier() {
  ActivateIncrementalWriteBarrier(heap_->old_space());
  ActivateIncrementalWriteBarrier(heap_->map_space());
  ActivateIncrementalWriteBarrier(heap_->code_space());
  ActivateIncrementalWriteBarrier(heap_->new_space());

  LargePage* lop = heap_->lo_space()->first_page();
  while (LargePage::IsValid(lop)) {
    SetOldSpacePageFlags(lop, true, is_compacting_);
    lop = lop->next_page();
  }
}


bool IncrementalMarking::ShouldActivateEvenWithoutIdleNotification() {
#ifndef DEBUG
  static const intptr_t kActivationThreshold = 8 * MB;
#else
  // TODO(gc) consider setting this to some low level so that some
  // debug tests run with incremental marking and some without.
  static const intptr_t kActivationThreshold = 0;
#endif
  // Don't switch on for very small heaps.
  return CanBeActivated() &&
         heap_->PromotedSpaceSizeOfObjects() > kActivationThreshold &&
         heap_->HeapIsFullEnoughToStartIncrementalMarking(
             heap_->old_generation_allocation_limit());
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


void IncrementalMarking::ActivateGeneratedStub(Code* stub) {
  DCHECK(RecordWriteStub::GetMode(stub) == RecordWriteStub::STORE_BUFFER_ONLY);

  if (!IsMarking()) {
    // Initially stub is generated in STORE_BUFFER_ONLY mode thus
    // we don't need to do anything if incremental marking is
    // not active.
  } else if (IsCompacting()) {
    RecordWriteStub::Patch(stub, RecordWriteStub::INCREMENTAL_COMPACTION);
  } else {
    RecordWriteStub::Patch(stub, RecordWriteStub::INCREMENTAL);
  }
}


void IncrementalMarking::NotifyOfHighPromotionRate() {
  if (IsMarking()) {
    if (marking_speed_ < kFastMarking) {
      if (FLAG_trace_gc) {
        PrintIsolate(heap()->isolate(),
                     "Increasing marking speed to %d "
                     "due to high promotion rate\n",
                     static_cast<int>(kFastMarking));
      }
      marking_speed_ = kFastMarking;
    }
  }
}


static void PatchIncrementalMarkingRecordWriteStubs(
    Heap* heap, RecordWriteStub::Mode mode) {
  UnseededNumberDictionary* stubs = heap->code_stubs();

  int capacity = stubs->Capacity();
  for (int i = 0; i < capacity; i++) {
    Object* k = stubs->KeyAt(i);
    if (stubs->IsKey(k)) {
      uint32_t key = NumberToUint32(k);

      if (CodeStub::MajorKeyFromKey(key) == CodeStub::RecordWrite) {
        Object* e = stubs->ValueAt(i);
        if (e->IsCode()) {
          RecordWriteStub::Patch(Code::cast(e), mode);
        }
      }
    }
  }
}


void IncrementalMarking::Start(const char* reason) {
  if (FLAG_trace_incremental_marking) {
    PrintF("[IncrementalMarking] Start (%s)\n",
           (reason == nullptr) ? "unknown reason" : reason);
  }
  DCHECK(FLAG_incremental_marking);
  DCHECK(state_ == STOPPED);
  DCHECK(heap_->gc_state() == Heap::NOT_IN_GC);
  DCHECK(!heap_->isolate()->serializer_enabled());

  HistogramTimerScope incremental_marking_scope(
      heap_->isolate()->counters()->gc_incremental_marking_start());
  TRACE_EVENT0("v8", "V8.GCIncrementalMarkingStart");
  ResetStepCounters();

  was_activated_ = true;

  if (!heap_->mark_compact_collector()->sweeping_in_progress()) {
    StartMarking();
  } else {
    if (FLAG_trace_incremental_marking) {
      PrintF("[IncrementalMarking] Start sweeping.\n");
    }
    state_ = SWEEPING;
  }

  heap_->new_space()->AddAllocationObserver(&observer_);

  incremental_marking_job()->Start(heap_);
}


void IncrementalMarking::StartMarking() {
  if (FLAG_trace_incremental_marking) {
    PrintF("[IncrementalMarking] Start marking\n");
  }

  is_compacting_ = !FLAG_never_compact &&
                   heap_->mark_compact_collector()->StartCompaction(
                       MarkCompactCollector::INCREMENTAL_COMPACTION);

  state_ = MARKING;

  RecordWriteStub::Mode mode = is_compacting_
                                   ? RecordWriteStub::INCREMENTAL_COMPACTION
                                   : RecordWriteStub::INCREMENTAL;

  PatchIncrementalMarkingRecordWriteStubs(heap_, mode);

  heap_->mark_compact_collector()->EnsureMarkingDequeIsCommittedAndInitialize(
      MarkCompactCollector::kMaxMarkingDequeSize);

  ActivateIncrementalWriteBarrier();

// Marking bits are cleared by the sweeper.
#ifdef VERIFY_HEAP
  if (FLAG_verify_heap) {
    heap_->mark_compact_collector()->VerifyMarkbitsAreClean();
  }
#endif

  heap_->CompletelyClearInstanceofCache();
  heap_->isolate()->compilation_cache()->MarkCompactPrologue();

  if (FLAG_cleanup_code_caches_at_gc) {
    // We will mark cache black with a separate pass
    // when we finish marking.
    MarkObjectGreyDoNotEnqueue(heap_->polymorphic_code_cache());
  }

  // Mark strong roots grey.
  IncrementalMarkingRootMarkingVisitor visitor(this);
  heap_->IterateStrongRoots(&visitor, VISIT_ONLY_STRONG);

  // Ready to start incremental marking.
  if (FLAG_trace_incremental_marking) {
    PrintF("[IncrementalMarking] Running\n");
  }
}


void IncrementalMarking::MarkRoots() {
  DCHECK(!finalize_marking_completed_);
  DCHECK(IsMarking());

  IncrementalMarkingRootMarkingVisitor visitor(this);
  heap_->IterateStrongRoots(&visitor, VISIT_ONLY_STRONG);
}


void IncrementalMarking::MarkObjectGroups() {
  DCHECK(!finalize_marking_completed_);
  DCHECK(IsMarking());

  IncrementalMarkingRootMarkingVisitor visitor(this);
  heap_->mark_compact_collector()->MarkImplicitRefGroups(&MarkObject);
  heap_->isolate()->global_handles()->IterateObjectGroups(
      &visitor, &MarkCompactCollector::IsUnmarkedHeapObjectWithHeap);
  heap_->isolate()->global_handles()->RemoveImplicitRefGroups();
  heap_->isolate()->global_handles()->RemoveObjectGroups();
}


void IncrementalMarking::ProcessWeakCells() {
  DCHECK(!finalize_marking_completed_);
  DCHECK(IsMarking());

  Object* the_hole_value = heap()->the_hole_value();
  Object* weak_cell_obj = heap()->encountered_weak_cells();
  Object* weak_cell_head = Smi::FromInt(0);
  WeakCell* prev_weak_cell_obj = NULL;
  while (weak_cell_obj != Smi::FromInt(0)) {
    WeakCell* weak_cell = reinterpret_cast<WeakCell*>(weak_cell_obj);
    // We do not insert cleared weak cells into the list, so the value
    // cannot be a Smi here.
    HeapObject* value = HeapObject::cast(weak_cell->value());
    // Remove weak cells with live objects from the list, they do not need
    // clearing.
    if (MarkCompactCollector::IsMarked(value)) {
      // Record slot, if value is pointing to an evacuation candidate.
      Object** slot = HeapObject::RawField(weak_cell, WeakCell::kValueOffset);
      heap_->mark_compact_collector()->RecordSlot(weak_cell, slot, *slot);
      // Remove entry somewhere after top.
      if (prev_weak_cell_obj != NULL) {
        prev_weak_cell_obj->set_next(weak_cell->next());
      }
      weak_cell_obj = weak_cell->next();
      weak_cell->clear_next(the_hole_value);
    } else {
      if (weak_cell_head == Smi::FromInt(0)) {
        weak_cell_head = weak_cell;
      }
      prev_weak_cell_obj = weak_cell;
      weak_cell_obj = weak_cell->next();
    }
  }
  // Top may have changed.
  heap()->set_encountered_weak_cells(weak_cell_head);
}


bool ShouldRetainMap(Map* map, int age) {
  if (age == 0) {
    // The map has aged. Do not retain this map.
    return false;
  }
  Object* constructor = map->GetConstructor();
  if (!constructor->IsHeapObject() ||
      Marking::IsWhite(Marking::MarkBitFrom(HeapObject::cast(constructor)))) {
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
                                   heap()->ShouldAbortIncrementalMarking() ||
                                   FLAG_retain_maps_for_n_gc == 0;
  ArrayList* retained_maps = heap()->retained_maps();
  int length = retained_maps->Length();
  // The number_of_disposed_maps separates maps in the retained_maps
  // array that were created before and after context disposal.
  // We do not age and retain disposed maps to avoid memory leaks.
  int number_of_disposed_maps = heap()->number_of_disposed_maps_;
  for (int i = 0; i < length; i += 2) {
    DCHECK(retained_maps->Get(i)->IsWeakCell());
    WeakCell* cell = WeakCell::cast(retained_maps->Get(i));
    if (cell->cleared()) continue;
    int age = Smi::cast(retained_maps->Get(i + 1))->value();
    int new_age;
    Map* map = Map::cast(cell->value());
    MarkBit map_mark = Marking::MarkBitFrom(map);
    if (i >= number_of_disposed_maps && !map_retaining_is_disabled &&
        Marking::IsWhite(map_mark)) {
      if (ShouldRetainMap(map, age)) {
        MarkObject(heap(), map);
      }
      Object* prototype = map->prototype();
      if (age > 0 && prototype->IsHeapObject() &&
          Marking::IsWhite(Marking::MarkBitFrom(HeapObject::cast(prototype)))) {
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
      retained_maps->Set(i + 1, Smi::FromInt(new_age));
    }
  }
}


void IncrementalMarking::FinalizeIncrementally() {
  DCHECK(!finalize_marking_completed_);
  DCHECK(IsMarking());

  double start = heap_->MonotonicallyIncreasingTimeInMs();

  int old_marking_deque_top =
      heap_->mark_compact_collector()->marking_deque()->top();

  // After finishing incremental marking, we try to discover all unmarked
  // objects to reduce the marking load in the final pause.
  // 1) We scan and mark the roots again to find all changes to the root set.
  // 2) We mark the object groups.
  // 3) Age and retain maps embedded in optimized code.
  // 4) Remove weak cell with live values from the list of weak cells, they
  // do not need processing during GC.
  MarkRoots();
  MarkObjectGroups();
  if (incremental_marking_finalization_rounds_ == 0) {
    // Map retaining is needed for perfromance, not correctness,
    // so we can do it only once at the beginning of the finalization.
    RetainMaps();
  }
  ProcessWeakCells();

  int marking_progress =
      abs(old_marking_deque_top -
          heap_->mark_compact_collector()->marking_deque()->top());

  double end = heap_->MonotonicallyIncreasingTimeInMs();
  double delta = end - start;
  heap_->tracer()->AddMarkingTime(delta);
  heap_->tracer()->AddIncrementalMarkingFinalizationStep(delta);
  if (FLAG_trace_incremental_marking) {
    PrintF(
        "[IncrementalMarking] Finalize incrementally round %d, "
        "spent %d ms, marking progress %d.\n",
        static_cast<int>(delta), incremental_marking_finalization_rounds_,
        marking_progress);
  }

  ++incremental_marking_finalization_rounds_;
  if ((incremental_marking_finalization_rounds_ >=
       FLAG_max_incremental_marking_finalization_rounds) ||
      (marking_progress <
       FLAG_min_progress_during_incremental_marking_finalization)) {
    finalize_marking_completed_ = true;
  }
}


void IncrementalMarking::UpdateMarkingDequeAfterScavenge() {
  if (!IsMarking()) return;

  MarkingDeque* marking_deque =
      heap_->mark_compact_collector()->marking_deque();
  int current = marking_deque->bottom();
  int mask = marking_deque->mask();
  int limit = marking_deque->top();
  HeapObject** array = marking_deque->array();
  int new_top = current;

  Map* filler_map = heap_->one_pointer_filler_map();

  while (current != limit) {
    HeapObject* obj = array[current];
    DCHECK(obj->IsHeapObject());
    current = ((current + 1) & mask);
    // Only pointers to from space have to be updated.
    if (heap_->InFromSpace(obj)) {
      MapWord map_word = obj->map_word();
      // There may be objects on the marking deque that do not exist anymore,
      // e.g. left trimmed objects or objects from the root set (frames).
      // If these object are dead at scavenging time, their marking deque
      // entries will not point to forwarding addresses. Hence, we can discard
      // them.
      if (map_word.IsForwardingAddress()) {
        HeapObject* dest = map_word.ToForwardingAddress();
        array[new_top] = dest;
        new_top = ((new_top + 1) & mask);
        DCHECK(new_top != marking_deque->bottom());
#ifdef DEBUG
        MarkBit mark_bit = Marking::MarkBitFrom(obj);
        DCHECK(Marking::IsGrey(mark_bit) ||
               (obj->IsFiller() && Marking::IsWhite(mark_bit)));
#endif
      }
    } else if (obj->map() != filler_map) {
      // Skip one word filler objects that appear on the
      // stack when we perform in place array shift.
      array[new_top] = obj;
      new_top = ((new_top + 1) & mask);
      DCHECK(new_top != marking_deque->bottom());
#ifdef DEBUG
      MarkBit mark_bit = Marking::MarkBitFrom(obj);
      MemoryChunk* chunk = MemoryChunk::FromAddress(obj->address());
      DCHECK(Marking::IsGrey(mark_bit) ||
             (obj->IsFiller() && Marking::IsWhite(mark_bit)) ||
             (chunk->IsFlagSet(MemoryChunk::HAS_PROGRESS_BAR) &&
              Marking::IsBlack(mark_bit)));
#endif
    }
  }
  marking_deque->set_top(new_top);
}


void IncrementalMarking::VisitObject(Map* map, HeapObject* obj, int size) {
  MarkObject(heap_, map);

  IncrementalMarkingMarkingVisitor::IterateBody(map, obj);

  MarkBit mark_bit = Marking::MarkBitFrom(obj);
#if ENABLE_SLOW_DCHECKS
  MemoryChunk* chunk = MemoryChunk::FromAddress(obj->address());
  SLOW_DCHECK(Marking::IsGrey(mark_bit) ||
              (obj->IsFiller() && Marking::IsWhite(mark_bit)) ||
              (chunk->IsFlagSet(MemoryChunk::HAS_PROGRESS_BAR) &&
               Marking::IsBlack(mark_bit)));
#endif
  MarkBlackOrKeepBlack(obj, mark_bit, size);
}


void IncrementalMarking::MarkObject(Heap* heap, HeapObject* obj) {
  MarkBit mark_bit = Marking::MarkBitFrom(obj);
  if (Marking::IsWhite(mark_bit)) {
    heap->incremental_marking()->WhiteToGreyAndPush(obj, mark_bit);
  }
}


intptr_t IncrementalMarking::ProcessMarkingDeque(intptr_t bytes_to_process) {
  intptr_t bytes_processed = 0;
  Map* one_pointer_filler_map = heap_->one_pointer_filler_map();
  Map* two_pointer_filler_map = heap_->two_pointer_filler_map();
  MarkingDeque* marking_deque =
      heap_->mark_compact_collector()->marking_deque();
  while (!marking_deque->IsEmpty() && bytes_processed < bytes_to_process) {
    HeapObject* obj = marking_deque->Pop();

    // Explicitly skip one and two word fillers. Incremental markbit patterns
    // are correct only for objects that occupy at least two words.
    // Moreover, slots filtering for left-trimmed arrays works only when
    // the distance between the old array start and the new array start
    // is greater than two if both starts are marked.
    Map* map = obj->map();
    if (map == one_pointer_filler_map || map == two_pointer_filler_map)
      continue;

    int size = obj->SizeFromMap(map);
    unscanned_bytes_of_large_object_ = 0;
    VisitObject(map, obj, size);
    bytes_processed += size - unscanned_bytes_of_large_object_;
  }
  return bytes_processed;
}


void IncrementalMarking::ProcessMarkingDeque() {
  Map* filler_map = heap_->one_pointer_filler_map();
  MarkingDeque* marking_deque =
      heap_->mark_compact_collector()->marking_deque();
  while (!marking_deque->IsEmpty()) {
    HeapObject* obj = marking_deque->Pop();

    // Explicitly skip one word fillers. Incremental markbit patterns are
    // correct only for objects that occupy at least two words.
    Map* map = obj->map();
    if (map == filler_map) continue;

    VisitObject(map, obj, obj->SizeFromMap(map));
  }
}


void IncrementalMarking::Hurry() {
  if (state() == MARKING) {
    double start = 0.0;
    if (FLAG_trace_incremental_marking || FLAG_print_cumulative_gc_stat) {
      start = heap_->MonotonicallyIncreasingTimeInMs();
      if (FLAG_trace_incremental_marking) {
        PrintF("[IncrementalMarking] Hurry\n");
      }
    }
    // TODO(gc) hurry can mark objects it encounters black as mutator
    // was stopped.
    ProcessMarkingDeque();
    state_ = COMPLETE;
    if (FLAG_trace_incremental_marking || FLAG_print_cumulative_gc_stat) {
      double end = heap_->MonotonicallyIncreasingTimeInMs();
      double delta = end - start;
      heap_->tracer()->AddMarkingTime(delta);
      if (FLAG_trace_incremental_marking) {
        PrintF("[IncrementalMarking] Complete (hurry), spent %d ms.\n",
               static_cast<int>(delta));
      }
    }
  }

  if (FLAG_cleanup_code_caches_at_gc) {
    PolymorphicCodeCache* poly_cache = heap_->polymorphic_code_cache();
    Marking::GreyToBlack(Marking::MarkBitFrom(poly_cache));
    MemoryChunk::IncrementLiveBytesFromGC(poly_cache,
                                          PolymorphicCodeCache::kSize);
  }

  Object* context = heap_->native_contexts_list();
  while (!context->IsUndefined()) {
    // GC can happen when the context is not fully initialized,
    // so the cache can be undefined.
    HeapObject* cache = HeapObject::cast(
        Context::cast(context)->get(Context::NORMALIZED_MAP_CACHE_INDEX));
    if (!cache->IsUndefined()) {
      MarkBit mark_bit = Marking::MarkBitFrom(cache);
      if (Marking::IsGrey(mark_bit)) {
        Marking::GreyToBlack(mark_bit);
        MemoryChunk::IncrementLiveBytesFromGC(cache, cache->Size());
      }
    }
    context = Context::cast(context)->get(Context::NEXT_CONTEXT_LINK);
  }
}


void IncrementalMarking::Stop() {
  if (IsStopped()) return;
  if (FLAG_trace_incremental_marking) {
    PrintF("[IncrementalMarking] Stopping.\n");
  }

  heap_->new_space()->RemoveAllocationObserver(&observer_);
  IncrementalMarking::set_should_hurry(false);
  ResetStepCounters();
  if (IsMarking()) {
    PatchIncrementalMarkingRecordWriteStubs(heap_,
                                            RecordWriteStub::STORE_BUFFER_ONLY);
    DeactivateIncrementalWriteBarrier();
  }
  heap_->isolate()->stack_guard()->ClearGC();
  state_ = STOPPED;
  is_compacting_ = false;
}


void IncrementalMarking::Finalize() {
  Hurry();
  Stop();
}


void IncrementalMarking::FinalizeMarking(CompletionAction action) {
  DCHECK(!finalize_marking_completed_);
  if (FLAG_trace_incremental_marking) {
    PrintF(
        "[IncrementalMarking] requesting finalization of incremental "
        "marking.\n");
  }
  request_type_ = FINALIZATION;
  if (action == GC_VIA_STACK_GUARD) {
    heap_->isolate()->stack_guard()->RequestGC();
  }
}


void IncrementalMarking::MarkingComplete(CompletionAction action) {
  state_ = COMPLETE;
  // We will set the stack guard to request a GC now.  This will mean the rest
  // of the GC gets performed as soon as possible (we can't do a GC here in a
  // record-write context).  If a few things get allocated between now and then
  // that shouldn't make us do a scavenge and keep being incremental, so we set
  // the should-hurry flag to indicate that there can't be much work left to do.
  set_should_hurry(true);
  if (FLAG_trace_incremental_marking) {
    PrintF("[IncrementalMarking] Complete (normal).\n");
  }
  request_type_ = COMPLETE_MARKING;
  if (action == GC_VIA_STACK_GUARD) {
    heap_->isolate()->stack_guard()->RequestGC();
  }
}


void IncrementalMarking::Epilogue() {
  was_activated_ = false;
  finalize_marking_completed_ = false;
  incremental_marking_finalization_rounds_ = 0;
}


double IncrementalMarking::AdvanceIncrementalMarking(
    intptr_t step_size_in_bytes, double deadline_in_ms,
    IncrementalMarking::StepActions step_actions) {
  DCHECK(!IsStopped());

  if (step_size_in_bytes == 0) {
    step_size_in_bytes = GCIdleTimeHandler::EstimateMarkingStepSize(
        static_cast<size_t>(GCIdleTimeHandler::kIncrementalMarkingStepTimeInMs),
        static_cast<size_t>(
            heap()
                ->tracer()
                ->FinalIncrementalMarkCompactSpeedInBytesPerMillisecond()));
  }

  double remaining_time_in_ms = 0.0;
  do {
    Step(step_size_in_bytes, step_actions.completion_action,
         step_actions.force_marking, step_actions.force_completion);
    remaining_time_in_ms =
        deadline_in_ms - heap()->MonotonicallyIncreasingTimeInMs();
  } while (remaining_time_in_ms >=
               2.0 * GCIdleTimeHandler::kIncrementalMarkingStepTimeInMs &&
           !IsComplete() &&
           !heap()->mark_compact_collector()->marking_deque()->IsEmpty());
  return remaining_time_in_ms;
}


void IncrementalMarking::OldSpaceStep(intptr_t allocated) {
  if (IsStopped() && ShouldActivateEvenWithoutIdleNotification()) {
    heap()->StartIncrementalMarking(Heap::kNoGCFlags, kNoGCCallbackFlags,
                                    "old space step");
  } else {
    Step(allocated * kFastMarking / kInitialMarkingSpeed, GC_VIA_STACK_GUARD);
  }
}


void IncrementalMarking::SpeedUp() {
  bool speed_up = false;

  if ((steps_count_ % kMarkingSpeedAccellerationInterval) == 0) {
    if (FLAG_trace_incremental_marking) {
      PrintIsolate(heap()->isolate(), "Speed up marking after %d steps\n",
                   static_cast<int>(kMarkingSpeedAccellerationInterval));
    }
    speed_up = true;
  }

  bool space_left_is_very_small =
      (old_generation_space_available_at_start_of_incremental_ < 10 * MB);

  bool only_1_nth_of_space_that_was_available_still_left =
      (SpaceLeftInOldSpace() * (marking_speed_ + 1) <
       old_generation_space_available_at_start_of_incremental_);

  if (space_left_is_very_small ||
      only_1_nth_of_space_that_was_available_still_left) {
    if (FLAG_trace_incremental_marking)
      PrintIsolate(heap()->isolate(),
                   "Speed up marking because of low space left\n");
    speed_up = true;
  }

  bool size_of_old_space_multiplied_by_n_during_marking =
      (heap_->PromotedTotalSize() >
       (marking_speed_ + 1) *
           old_generation_space_used_at_start_of_incremental_);
  if (size_of_old_space_multiplied_by_n_during_marking) {
    speed_up = true;
    if (FLAG_trace_incremental_marking) {
      PrintIsolate(heap()->isolate(),
                   "Speed up marking because of heap size increase\n");
    }
  }

  int64_t promoted_during_marking =
      heap_->PromotedTotalSize() -
      old_generation_space_used_at_start_of_incremental_;
  intptr_t delay = marking_speed_ * MB;
  intptr_t scavenge_slack = heap_->MaxSemiSpaceSize();

  // We try to scan at at least twice the speed that we are allocating.
  if (promoted_during_marking > bytes_scanned_ / 2 + scavenge_slack + delay) {
    if (FLAG_trace_incremental_marking) {
      PrintIsolate(heap()->isolate(),
                   "Speed up marking because marker was not keeping up\n");
    }
    speed_up = true;
  }

  if (speed_up) {
    if (state_ != MARKING) {
      if (FLAG_trace_incremental_marking) {
        PrintIsolate(heap()->isolate(),
                     "Postponing speeding up marking until marking starts\n");
      }
    } else {
      marking_speed_ += kMarkingSpeedAccelleration;
      marking_speed_ = static_cast<int>(
          Min(kMaxMarkingSpeed, static_cast<intptr_t>(marking_speed_ * 1.3)));
      if (FLAG_trace_incremental_marking) {
        PrintIsolate(heap()->isolate(), "Marking speed increased to %d\n",
                     marking_speed_);
      }
    }
  }
}


intptr_t IncrementalMarking::Step(intptr_t allocated_bytes,
                                  CompletionAction action,
                                  ForceMarkingAction marking,
                                  ForceCompletionAction completion) {
  DCHECK(allocated_bytes >= 0);

  if (heap_->gc_state() != Heap::NOT_IN_GC || !FLAG_incremental_marking ||
      (state_ != SWEEPING && state_ != MARKING)) {
    return 0;
  }

  allocated_ += allocated_bytes;

  if (marking == DO_NOT_FORCE_MARKING && allocated_ < kAllocatedThreshold &&
      write_barriers_invoked_since_last_step_ <
          kWriteBarriersInvokedThreshold) {
    return 0;
  }

  // If an idle notification happened recently, we delay marking steps.
  if (marking == DO_NOT_FORCE_MARKING &&
      heap_->RecentIdleNotificationHappened()) {
    return 0;
  }

  if (state_ == MARKING && no_marking_scope_depth_ > 0) return 0;

  intptr_t bytes_processed = 0;
  {
    HistogramTimerScope incremental_marking_scope(
        heap_->isolate()->counters()->gc_incremental_marking());
    TRACE_EVENT0("v8", "V8.GCIncrementalMarking");
    double start = heap_->MonotonicallyIncreasingTimeInMs();

    // The marking speed is driven either by the allocation rate or by the rate
    // at which we are having to check the color of objects in the write
    // barrier.
    // It is possible for a tight non-allocating loop to run a lot of write
    // barriers before we get here and check them (marking can only take place
    // on
    // allocation), so to reduce the lumpiness we don't use the write barriers
    // invoked since last step directly to determine the amount of work to do.
    intptr_t bytes_to_process =
        marking_speed_ *
        Max(allocated_, write_barriers_invoked_since_last_step_);
    allocated_ = 0;
    write_barriers_invoked_since_last_step_ = 0;

    bytes_scanned_ += bytes_to_process;

    if (state_ == SWEEPING) {
      if (heap_->mark_compact_collector()->sweeping_in_progress() &&
          (heap_->mark_compact_collector()->IsSweepingCompleted() ||
           !FLAG_concurrent_sweeping)) {
        heap_->mark_compact_collector()->EnsureSweepingCompleted();
      }
      if (!heap_->mark_compact_collector()->sweeping_in_progress()) {
        bytes_scanned_ = 0;
        StartMarking();
      }
    } else if (state_ == MARKING) {
      bytes_processed = ProcessMarkingDeque(bytes_to_process);
      if (heap_->mark_compact_collector()->marking_deque()->IsEmpty()) {
        if (completion == FORCE_COMPLETION ||
            IsIdleMarkingDelayCounterLimitReached()) {
          if (!finalize_marking_completed_) {
            FinalizeMarking(action);
          } else {
            MarkingComplete(action);
          }
        } else {
          IncrementIdleMarkingDelayCounter();
        }
      }
    }

    steps_count_++;

    // Speed up marking if we are marking too slow or if we are almost done
    // with marking.
    SpeedUp();

    double end = heap_->MonotonicallyIncreasingTimeInMs();
    double duration = (end - start);
    // Note that we report zero bytes here when sweeping was in progress or
    // when we just started incremental marking. In these cases we did not
    // process the marking deque.
    heap_->tracer()->AddIncrementalMarkingStep(duration, bytes_processed);
  }
  return bytes_processed;
}


void IncrementalMarking::ResetStepCounters() {
  steps_count_ = 0;
  old_generation_space_available_at_start_of_incremental_ =
      SpaceLeftInOldSpace();
  old_generation_space_used_at_start_of_incremental_ =
      heap_->PromotedTotalSize();
  bytes_rescanned_ = 0;
  marking_speed_ = kInitialMarkingSpeed;
  bytes_scanned_ = 0;
  write_barriers_invoked_since_last_step_ = 0;
}


int64_t IncrementalMarking::SpaceLeftInOldSpace() {
  return heap_->MaxOldGenerationSize() - heap_->PromotedSpaceSizeOfObjects();
}


bool IncrementalMarking::IsIdleMarkingDelayCounterLimitReached() {
  return idle_marking_delay_counter_ > kMaxIdleMarkingDelayCounter;
}


void IncrementalMarking::IncrementIdleMarkingDelayCounter() {
  idle_marking_delay_counter_++;
}


void IncrementalMarking::ClearIdleMarkingDelayCounter() {
  idle_marking_delay_counter_ = 0;
}
}  // namespace internal
}  // namespace v8
