// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "src/heap/incremental-marking.h"

#include "src/code-stubs.h"
#include "src/compilation-cache.h"
#include "src/conversions.h"
#include "src/heap/objects-visiting.h"
#include "src/heap/objects-visiting-inl.h"

namespace v8 {
namespace internal {


IncrementalMarking::IncrementalMarking(Heap* heap)
    : heap_(heap),
      state_(STOPPED),
      steps_count_(0),
      old_generation_space_available_at_start_of_incremental_(0),
      old_generation_space_used_at_start_of_incremental_(0),
      should_hurry_(false),
      marking_speed_(0),
      allocated_(0),
      idle_marking_delay_counter_(0),
      no_marking_scope_depth_(0),
      unscanned_bytes_of_large_object_(0),
      was_activated_(false),
      weak_closure_was_overapproximated_(false),
      request_type_(COMPLETE_MARKING) {}


void IncrementalMarking::RecordWriteSlow(HeapObject* obj, Object** slot,
                                         Object* value) {
  if (BaseRecordWrite(obj, slot, value) && slot != NULL) {
    MarkBit obj_bit = Marking::MarkBitFrom(obj);
    if (Marking::IsBlack(obj_bit)) {
      // Object is not going to be rescanned we need to record the slot.
      heap_->mark_compact_collector()->RecordSlot(HeapObject::RawField(obj, 0),
                                                  slot, value);
    }
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


void IncrementalMarking::RecordCodeTargetPatch(Code* host, Address pc,
                                               HeapObject* value) {
  if (IsMarking()) {
    RelocInfo rinfo(pc, RelocInfo::CODE_TARGET, 0, host);
    RecordWriteIntoCode(host, &rinfo, value);
  }
}


void IncrementalMarking::RecordCodeTargetPatch(Address pc, HeapObject* value) {
  if (IsMarking()) {
    Code* host = heap_->isolate()
                     ->inner_pointer_to_code_cache()
                     ->GcSafeFindCodeForInnerPointer(pc);
    RelocInfo rinfo(pc, RelocInfo::CODE_TARGET, 0, host);
    RecordWriteIntoCode(host, &rinfo, value);
  }
}


void IncrementalMarking::RecordWriteOfCodeEntrySlow(JSFunction* host,
                                                    Object** slot,
                                                    Code* value) {
  if (BaseRecordWrite(host, slot, value)) {
    DCHECK(slot != NULL);
    heap_->mark_compact_collector()->RecordCodeEntrySlot(
        reinterpret_cast<Address>(slot), value);
  }
}


void IncrementalMarking::RecordWriteIntoCodeSlow(HeapObject* obj,
                                                 RelocInfo* rinfo,
                                                 Object* value) {
  MarkBit value_bit = Marking::MarkBitFrom(HeapObject::cast(value));
  if (Marking::IsWhite(value_bit)) {
    MarkBit obj_bit = Marking::MarkBitFrom(obj);
    if (Marking::IsBlack(obj_bit)) {
      BlackToGreyAndUnshift(obj, obj_bit);
      RestartIfNotMarking();
    }
    // Object is either grey or white.  It will be scanned if survives.
    return;
  }

  if (is_compacting_) {
    MarkBit obj_bit = Marking::MarkBitFrom(obj);
    if (Marking::IsBlack(obj_bit)) {
      // Object is not going to be rescanned.  We need to record the slot.
      heap_->mark_compact_collector()->RecordRelocSlot(rinfo,
                                                       Code::cast(value));
    }
  }
}


static void MarkObjectGreyDoNotEnqueue(Object* obj) {
  if (obj->IsHeapObject()) {
    HeapObject* heap_obj = HeapObject::cast(obj);
    MarkBit mark_bit = Marking::MarkBitFrom(HeapObject::cast(obj));
    if (Marking::IsBlack(mark_bit)) {
      MemoryChunk::IncrementLiveBytesFromGC(heap_obj->address(),
                                            -heap_obj->Size());
    }
    Marking::AnyToGrey(mark_bit);
  }
}


static inline void MarkBlackOrKeepGrey(HeapObject* heap_object,
                                       MarkBit mark_bit, int size) {
  DCHECK(!Marking::IsImpossible(mark_bit));
  if (mark_bit.Get()) return;
  mark_bit.Set();
  MemoryChunk::IncrementLiveBytesFromGC(heap_object->address(), size);
  DCHECK(Marking::IsBlack(mark_bit));
}


static inline void MarkBlackOrKeepBlack(HeapObject* heap_object,
                                        MarkBit mark_bit, int size) {
  DCHECK(!Marking::IsImpossible(mark_bit));
  if (Marking::IsBlack(mark_bit)) return;
  Marking::MarkBlack(mark_bit);
  MemoryChunk::IncrementLiveBytesFromGC(heap_object->address(), size);
  DCHECK(Marking::IsBlack(mark_bit));
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
        VisitPointersWithAnchor(heap, HeapObject::RawField(object, 0),
                                HeapObject::RawField(object, start_offset),
                                HeapObject::RawField(object, end_offset));
        start_offset = end_offset;
        end_offset = Min(object_size, end_offset + kProgressBarScanningChunk);
        scan_until_end =
            heap->mark_compact_collector()->marking_deque()->IsFull();
      } while (scan_until_end && start_offset < object_size);
      chunk->set_progress_bar(start_offset);
      if (start_offset < object_size) {
        heap->mark_compact_collector()->marking_deque()->UnshiftGrey(object);
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

  INLINE(static void VisitPointer(Heap* heap, Object** p)) {
    Object* obj = *p;
    if (obj->IsHeapObject()) {
      heap->mark_compact_collector()->RecordSlot(p, p, obj);
      MarkObject(heap, obj);
    }
  }

  INLINE(static void VisitPointers(Heap* heap, Object** start, Object** end)) {
    for (Object** p = start; p < end; p++) {
      Object* obj = *p;
      if (obj->IsHeapObject()) {
        heap->mark_compact_collector()->RecordSlot(start, p, obj);
        MarkObject(heap, obj);
      }
    }
  }

  INLINE(static void VisitPointersWithAnchor(Heap* heap, Object** anchor,
                                             Object** start, Object** end)) {
    for (Object** p = start; p < end; p++) {
      Object* obj = *p;
      if (obj->IsHeapObject()) {
        heap->mark_compact_collector()->RecordSlot(anchor, p, obj);
        MarkObject(heap, obj);
      }
    }
  }

  // Marks the object grey and pushes it on the marking stack.
  INLINE(static void MarkObject(Heap* heap, Object* obj)) {
    HeapObject* heap_object = HeapObject::cast(obj);
    MarkBit mark_bit = Marking::MarkBitFrom(heap_object);
    if (mark_bit.data_only()) {
      MarkBlackOrKeepGrey(heap_object, mark_bit, heap_object->Size());
    } else if (Marking::IsWhite(mark_bit)) {
      heap->incremental_marking()->WhiteToGreyAndPush(heap_object, mark_bit);
    }
  }

  // Marks the object black without pushing it on the marking stack.
  // Returns true if object needed marking and false otherwise.
  INLINE(static bool MarkObjectWithoutPush(Heap* heap, Object* obj)) {
    HeapObject* heap_object = HeapObject::cast(obj);
    MarkBit mark_bit = Marking::MarkBitFrom(heap_object);
    if (Marking::IsWhite(mark_bit)) {
      mark_bit.Set();
      MemoryChunk::IncrementLiveBytesFromGC(heap_object->address(),
                                            heap_object->Size());
      return true;
    }
    return false;
  }
};


class IncrementalMarkingRootMarkingVisitor : public ObjectVisitor {
 public:
  explicit IncrementalMarkingRootMarkingVisitor(
      IncrementalMarking* incremental_marking)
      : incremental_marking_(incremental_marking) {}

  void VisitPointer(Object** p) { MarkObjectByPointer(p); }

  void VisitPointers(Object** start, Object** end) {
    for (Object** p = start; p < end; p++) MarkObjectByPointer(p);
  }

 private:
  void MarkObjectByPointer(Object** p) {
    Object* obj = *p;
    if (!obj->IsHeapObject()) return;

    HeapObject* heap_object = HeapObject::cast(obj);
    MarkBit mark_bit = Marking::MarkBitFrom(heap_object);
    if (mark_bit.data_only()) {
      MarkBlackOrKeepGrey(heap_object, mark_bit, heap_object->Size());
    } else {
      if (Marking::IsWhite(mark_bit)) {
        incremental_marking_->WhiteToGreyAndPush(heap_object, mark_bit);
      }
    }
  }

  IncrementalMarking* incremental_marking_;
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

    // It's difficult to filter out slots recorded for large objects.
    if (chunk->owner()->identity() == LO_SPACE &&
        chunk->size() > static_cast<size_t>(Page::kPageSize) && is_compacting) {
      chunk->SetFlag(MemoryChunk::RESCAN_ON_EVACUATION);
    }
  } else if (chunk->owner()->identity() == CELL_SPACE ||
             chunk->owner()->identity() == PROPERTY_CELL_SPACE ||
             chunk->scan_on_scavenge()) {
    chunk->ClearFlag(MemoryChunk::POINTERS_TO_HERE_ARE_INTERESTING);
    chunk->ClearFlag(MemoryChunk::POINTERS_FROM_HERE_ARE_INTERESTING);
  } else {
    chunk->ClearFlag(MemoryChunk::POINTERS_TO_HERE_ARE_INTERESTING);
    chunk->SetFlag(MemoryChunk::POINTERS_FROM_HERE_ARE_INTERESTING);
  }
}


void IncrementalMarking::SetNewSpacePageFlags(NewSpacePage* chunk,
                                              bool is_marking) {
  chunk->SetFlag(MemoryChunk::POINTERS_TO_HERE_ARE_INTERESTING);
  if (is_marking) {
    chunk->SetFlag(MemoryChunk::POINTERS_FROM_HERE_ARE_INTERESTING);
  } else {
    chunk->ClearFlag(MemoryChunk::POINTERS_FROM_HERE_ARE_INTERESTING);
  }
  chunk->SetFlag(MemoryChunk::SCAN_ON_SCAVENGE);
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
  DeactivateIncrementalWriteBarrierForSpace(heap_->old_pointer_space());
  DeactivateIncrementalWriteBarrierForSpace(heap_->old_data_space());
  DeactivateIncrementalWriteBarrierForSpace(heap_->cell_space());
  DeactivateIncrementalWriteBarrierForSpace(heap_->property_cell_space());
  DeactivateIncrementalWriteBarrierForSpace(heap_->map_space());
  DeactivateIncrementalWriteBarrierForSpace(heap_->code_space());
  DeactivateIncrementalWriteBarrierForSpace(heap_->new_space());

  LargePage* lop = heap_->lo_space()->first_page();
  while (lop->is_valid()) {
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
  ActivateIncrementalWriteBarrier(heap_->old_pointer_space());
  ActivateIncrementalWriteBarrier(heap_->old_data_space());
  ActivateIncrementalWriteBarrier(heap_->cell_space());
  ActivateIncrementalWriteBarrier(heap_->property_cell_space());
  ActivateIncrementalWriteBarrier(heap_->map_space());
  ActivateIncrementalWriteBarrier(heap_->code_space());
  ActivateIncrementalWriteBarrier(heap_->new_space());

  LargePage* lop = heap_->lo_space()->first_page();
  while (lop->is_valid()) {
    SetOldSpacePageFlags(lop, true, is_compacting_);
    lop = lop->next_page();
  }
}


bool IncrementalMarking::ShouldActivate() {
  return WorthActivating() && heap_->NextGCIsLikelyToBeFull();
}


bool IncrementalMarking::WasActivated() { return was_activated_; }


bool IncrementalMarking::WorthActivating() {
#ifndef DEBUG
  static const intptr_t kActivationThreshold = 8 * MB;
#else
  // TODO(gc) consider setting this to some low level so that some
  // debug tests run with incremental marking and some without.
  static const intptr_t kActivationThreshold = 0;
#endif
  // Only start incremental marking in a safe state: 1) when incremental
  // marking is turned on, 2) when we are currently not in a GC, and
  // 3) when we are currently not serializing or deserializing the heap.
  return FLAG_incremental_marking && FLAG_incremental_marking_steps &&
         heap_->gc_state() == Heap::NOT_IN_GC &&
         heap_->deserialization_complete() &&
         !heap_->isolate()->serializer_enabled() &&
         heap_->PromotedSpaceSizeOfObjects() > kActivationThreshold;
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


void IncrementalMarking::Start(CompactionFlag flag) {
  if (FLAG_trace_incremental_marking) {
    PrintF("[IncrementalMarking] Start\n");
  }
  DCHECK(FLAG_incremental_marking);
  DCHECK(FLAG_incremental_marking_steps);
  DCHECK(state_ == STOPPED);
  DCHECK(heap_->gc_state() == Heap::NOT_IN_GC);
  DCHECK(!heap_->isolate()->serializer_enabled());

  ResetStepCounters();

  was_activated_ = true;

  if (!heap_->mark_compact_collector()->sweeping_in_progress()) {
    StartMarking(flag);
  } else {
    if (FLAG_trace_incremental_marking) {
      PrintF("[IncrementalMarking] Start sweeping.\n");
    }
    state_ = SWEEPING;
  }

  heap_->new_space()->LowerInlineAllocationLimit(kAllocatedThreshold);
}


void IncrementalMarking::StartMarking(CompactionFlag flag) {
  if (FLAG_trace_incremental_marking) {
    PrintF("[IncrementalMarking] Start marking\n");
  }

  is_compacting_ = !FLAG_never_compact && (flag == ALLOW_COMPACTION) &&
                   heap_->mark_compact_collector()->StartCompaction(
                       MarkCompactCollector::INCREMENTAL_COMPACTION);

  state_ = MARKING;

  RecordWriteStub::Mode mode = is_compacting_
                                   ? RecordWriteStub::INCREMENTAL_COMPACTION
                                   : RecordWriteStub::INCREMENTAL;

  PatchIncrementalMarkingRecordWriteStubs(heap_, mode);

  heap_->mark_compact_collector()->EnsureMarkingDequeIsCommittedAndInitialize();

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


void IncrementalMarking::PrepareForScavenge() {
  if (!IsMarking()) return;
  NewSpacePageIterator it(heap_->new_space()->FromSpaceStart(),
                          heap_->new_space()->FromSpaceEnd());
  while (it.has_next()) {
    Bitmap::Clear(it.next());
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
    if (heap_->InNewSpace(obj)) {
      MapWord map_word = obj->map_word();
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
  MarkBit map_mark_bit = Marking::MarkBitFrom(map);
  if (Marking::IsWhite(map_mark_bit)) {
    WhiteToGreyAndPush(map, map_mark_bit);
  }

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


intptr_t IncrementalMarking::ProcessMarkingDeque(intptr_t bytes_to_process) {
  intptr_t bytes_processed = 0;
  Map* filler_map = heap_->one_pointer_filler_map();
  MarkingDeque* marking_deque =
      heap_->mark_compact_collector()->marking_deque();
  while (!marking_deque->IsEmpty() && bytes_processed < bytes_to_process) {
    HeapObject* obj = marking_deque->Pop();

    // Explicitly skip one word fillers. Incremental markbit patterns are
    // correct only for objects that occupy at least two words.
    Map* map = obj->map();
    if (map == filler_map) continue;

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
      start = base::OS::TimeCurrentMillis();
      if (FLAG_trace_incremental_marking) {
        PrintF("[IncrementalMarking] Hurry\n");
      }
    }
    // TODO(gc) hurry can mark objects it encounters black as mutator
    // was stopped.
    ProcessMarkingDeque();
    state_ = COMPLETE;
    if (FLAG_trace_incremental_marking || FLAG_print_cumulative_gc_stat) {
      double end = base::OS::TimeCurrentMillis();
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
    MemoryChunk::IncrementLiveBytesFromGC(poly_cache->address(),
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
        MemoryChunk::IncrementLiveBytesFromGC(cache->address(), cache->Size());
      }
    }
    context = Context::cast(context)->get(Context::NEXT_CONTEXT_LINK);
  }
}


void IncrementalMarking::Abort() {
  if (IsStopped()) return;
  if (FLAG_trace_incremental_marking) {
    PrintF("[IncrementalMarking] Aborting.\n");
  }
  heap_->new_space()->LowerInlineAllocationLimit(0);
  IncrementalMarking::set_should_hurry(false);
  ResetStepCounters();
  if (IsMarking()) {
    PatchIncrementalMarkingRecordWriteStubs(heap_,
                                            RecordWriteStub::STORE_BUFFER_ONLY);
    DeactivateIncrementalWriteBarrier();

    if (is_compacting_) {
      LargeObjectIterator it(heap_->lo_space());
      for (HeapObject* obj = it.Next(); obj != NULL; obj = it.Next()) {
        Page* p = Page::FromAddress(obj->address());
        if (p->IsFlagSet(Page::RESCAN_ON_EVACUATION)) {
          p->ClearFlag(Page::RESCAN_ON_EVACUATION);
        }
      }
    }
  }
  heap_->isolate()->stack_guard()->ClearGC();
  state_ = STOPPED;
  is_compacting_ = false;
}


void IncrementalMarking::Finalize() {
  Hurry();
  state_ = STOPPED;
  is_compacting_ = false;
  heap_->new_space()->LowerInlineAllocationLimit(0);
  IncrementalMarking::set_should_hurry(false);
  ResetStepCounters();
  PatchIncrementalMarkingRecordWriteStubs(heap_,
                                          RecordWriteStub::STORE_BUFFER_ONLY);
  DeactivateIncrementalWriteBarrier();
  DCHECK(heap_->mark_compact_collector()->marking_deque()->IsEmpty());
  heap_->isolate()->stack_guard()->ClearGC();
}


void IncrementalMarking::OverApproximateWeakClosure() {
  DCHECK(FLAG_overapproximate_weak_closure);
  DCHECK(!weak_closure_was_overapproximated_);
  if (FLAG_trace_incremental_marking) {
    PrintF("[IncrementalMarking] requesting weak closure overapproximation.\n");
  }
  set_should_hurry(true);
  request_type_ = OVERAPPROXIMATION;
  heap_->isolate()->stack_guard()->RequestGC();
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
  if (action == GC_VIA_STACK_GUARD) {
    request_type_ = COMPLETE_MARKING;
    heap_->isolate()->stack_guard()->RequestGC();
  }
}


void IncrementalMarking::Epilogue() {
  was_activated_ = false;
  weak_closure_was_overapproximated_ = false;
}


void IncrementalMarking::OldSpaceStep(intptr_t allocated) {
  if (IsStopped() && ShouldActivate()) {
    // TODO(hpayer): Let's play safe for now, but compaction should be
    // in principle possible.
    Start(PREVENT_COMPACTION);
  } else {
    Step(allocated * kFastMarking / kInitialMarkingSpeed, GC_VIA_STACK_GUARD);
  }
}


void IncrementalMarking::SpeedUp() {
  bool speed_up = false;

  if ((steps_count_ % kMarkingSpeedAccellerationInterval) == 0) {
    if (FLAG_trace_gc) {
      PrintPID("Speed up marking after %d steps\n",
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
    if (FLAG_trace_gc) PrintPID("Speed up marking because of low space left\n");
    speed_up = true;
  }

  bool size_of_old_space_multiplied_by_n_during_marking =
      (heap_->PromotedTotalSize() >
       (marking_speed_ + 1) *
           old_generation_space_used_at_start_of_incremental_);
  if (size_of_old_space_multiplied_by_n_during_marking) {
    speed_up = true;
    if (FLAG_trace_gc) {
      PrintPID("Speed up marking because of heap size increase\n");
    }
  }

  int64_t promoted_during_marking =
      heap_->PromotedTotalSize() -
      old_generation_space_used_at_start_of_incremental_;
  intptr_t delay = marking_speed_ * MB;
  intptr_t scavenge_slack = heap_->MaxSemiSpaceSize();

  // We try to scan at at least twice the speed that we are allocating.
  if (promoted_during_marking > bytes_scanned_ / 2 + scavenge_slack + delay) {
    if (FLAG_trace_gc) {
      PrintPID("Speed up marking because marker was not keeping up\n");
    }
    speed_up = true;
  }

  if (speed_up) {
    if (state_ != MARKING) {
      if (FLAG_trace_gc) {
        PrintPID("Postponing speeding up marking until marking starts\n");
      }
    } else {
      marking_speed_ += kMarkingSpeedAccelleration;
      marking_speed_ = static_cast<int>(
          Min(kMaxMarkingSpeed, static_cast<intptr_t>(marking_speed_ * 1.3)));
      if (FLAG_trace_gc) {
        PrintPID("Marking speed increased to %d\n", marking_speed_);
      }
    }
  }
}


intptr_t IncrementalMarking::Step(intptr_t allocated_bytes,
                                  CompletionAction action,
                                  ForceMarkingAction marking,
                                  ForceCompletionAction completion) {
  if (heap_->gc_state() != Heap::NOT_IN_GC || !FLAG_incremental_marking ||
      !FLAG_incremental_marking_steps ||
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
    double start = base::OS::TimeCurrentMillis();

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
        StartMarking(PREVENT_COMPACTION);
      }
    } else if (state_ == MARKING) {
      bytes_processed = ProcessMarkingDeque(bytes_to_process);
      if (heap_->mark_compact_collector()->marking_deque()->IsEmpty()) {
        if (completion == FORCE_COMPLETION ||
            IsIdleMarkingDelayCounterLimitReached()) {
          if (FLAG_overapproximate_weak_closure &&
              !weak_closure_was_overapproximated_ &&
              action == GC_VIA_STACK_GUARD) {
            OverApproximateWeakClosure();
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

    double end = base::OS::TimeCurrentMillis();
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
}
}  // namespace v8::internal
