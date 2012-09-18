// Copyright 2012 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "v8.h"

#include "incremental-marking.h"

#include "code-stubs.h"
#include "compilation-cache.h"
#include "objects-visiting.h"
#include "objects-visiting-inl.h"
#include "v8conversions.h"

namespace v8 {
namespace internal {


IncrementalMarking::IncrementalMarking(Heap* heap)
    : heap_(heap),
      state_(STOPPED),
      marking_deque_memory_(NULL),
      marking_deque_memory_committed_(false),
      marker_(this, heap->mark_compact_collector()),
      steps_count_(0),
      steps_took_(0),
      longest_step_(0.0),
      old_generation_space_available_at_start_of_incremental_(0),
      old_generation_space_used_at_start_of_incremental_(0),
      steps_count_since_last_gc_(0),
      steps_took_since_last_gc_(0),
      should_hurry_(false),
      allocation_marking_factor_(0),
      allocated_(0),
      no_marking_scope_depth_(0) {
}


void IncrementalMarking::TearDown() {
  delete marking_deque_memory_;
}


void IncrementalMarking::RecordWriteSlow(HeapObject* obj,
                                         Object** slot,
                                         Object* value) {
  if (BaseRecordWrite(obj, slot, value) && is_compacting_ && slot != NULL) {
    MarkBit obj_bit = Marking::MarkBitFrom(obj);
    if (Marking::IsBlack(obj_bit)) {
      // Object is not going to be rescanned we need to record the slot.
      heap_->mark_compact_collector()->RecordSlot(
          HeapObject::RawField(obj, 0), slot, value);
    }
  }
}


void IncrementalMarking::RecordWriteFromCode(HeapObject* obj,
                                             Object* value,
                                             Isolate* isolate) {
  ASSERT(obj->IsHeapObject());

  // Fast cases should already be covered by RecordWriteStub.
  ASSERT(value->IsHeapObject());
  ASSERT(!value->IsHeapNumber());
  ASSERT(!value->IsString() ||
         value->IsConsString() ||
         value->IsSlicedString());
  ASSERT(Marking::IsWhite(Marking::MarkBitFrom(HeapObject::cast(value))));

  IncrementalMarking* marking = isolate->heap()->incremental_marking();
  ASSERT(!marking->is_compacting_);
  marking->RecordWrite(obj, NULL, value);
}


void IncrementalMarking::RecordWriteForEvacuationFromCode(HeapObject* obj,
                                                          Object** slot,
                                                          Isolate* isolate) {
  IncrementalMarking* marking = isolate->heap()->incremental_marking();
  ASSERT(marking->is_compacting_);
  marking->RecordWrite(obj, slot, *slot);
}


void IncrementalMarking::RecordCodeTargetPatch(Code* host,
                                               Address pc,
                                               HeapObject* value) {
  if (IsMarking()) {
    RelocInfo rinfo(pc, RelocInfo::CODE_TARGET, 0, host);
    RecordWriteIntoCode(host, &rinfo, value);
  }
}


void IncrementalMarking::RecordCodeTargetPatch(Address pc, HeapObject* value) {
  if (IsMarking()) {
    Code* host = heap_->isolate()->inner_pointer_to_code_cache()->
        GcSafeFindCodeForInnerPointer(pc);
    RelocInfo rinfo(pc, RelocInfo::CODE_TARGET, 0, host);
    RecordWriteIntoCode(host, &rinfo, value);
  }
}


void IncrementalMarking::RecordWriteOfCodeEntrySlow(JSFunction* host,
                                                Object** slot,
                                                Code* value) {
  if (BaseRecordWrite(host, slot, value) && is_compacting_) {
    ASSERT(slot != NULL);
    heap_->mark_compact_collector()->
        RecordCodeEntrySlot(reinterpret_cast<Address>(slot), value);
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


class IncrementalMarkingMarkingVisitor
    : public StaticMarkingVisitor<IncrementalMarkingMarkingVisitor> {
 public:
  static void Initialize() {
    StaticMarkingVisitor<IncrementalMarkingMarkingVisitor>::Initialize();

    table_.Register(kVisitSharedFunctionInfo, &VisitSharedFunctionInfo);

    table_.Register(kVisitJSFunction, &VisitJSFunction);

    table_.Register(kVisitJSRegExp, &VisitJSRegExp);
  }

  static void VisitJSWeakMap(Map* map, HeapObject* object) {
    Heap* heap = map->GetHeap();
    VisitPointers(heap,
                  HeapObject::RawField(object, JSWeakMap::kPropertiesOffset),
                  HeapObject::RawField(object, JSWeakMap::kSize));
  }

  static void VisitSharedFunctionInfo(Map* map, HeapObject* object) {
    Heap* heap = map->GetHeap();
    SharedFunctionInfo* shared = SharedFunctionInfo::cast(object);
    if (shared->ic_age() != heap->global_ic_age()) {
      shared->ResetForNewContext(heap->global_ic_age());
    }
    FixedBodyVisitor<IncrementalMarkingMarkingVisitor,
                     SharedFunctionInfo::BodyDescriptor,
                     void>::Visit(map, object);
  }

  static inline void VisitJSFunction(Map* map, HeapObject* object) {
    Heap* heap = map->GetHeap();
    // Iterate over all fields in the body but take care in dealing with
    // the code entry and skip weak fields.
    VisitPointers(heap,
                  HeapObject::RawField(object, JSFunction::kPropertiesOffset),
                  HeapObject::RawField(object, JSFunction::kCodeEntryOffset));
    VisitCodeEntry(heap, object->address() + JSFunction::kCodeEntryOffset);
    VisitPointers(heap,
                  HeapObject::RawField(object,
                      JSFunction::kCodeEntryOffset + kPointerSize),
                  HeapObject::RawField(object,
                      JSFunction::kNonWeakFieldsEndOffset));
  }

  INLINE(static void VisitPointer(Heap* heap, Object** p)) {
    Object* obj = *p;
    if (obj->NonFailureIsHeapObject()) {
      heap->mark_compact_collector()->RecordSlot(p, p, obj);
      MarkObject(heap, obj);
    }
  }

  INLINE(static void VisitPointers(Heap* heap, Object** start, Object** end)) {
    for (Object** p = start; p < end; p++) {
      Object* obj = *p;
      if (obj->NonFailureIsHeapObject()) {
        heap->mark_compact_collector()->RecordSlot(start, p, obj);
        MarkObject(heap, obj);
      }
    }
  }

  INLINE(static void MarkObject(Heap* heap, Object* obj)) {
    HeapObject* heap_object = HeapObject::cast(obj);
    MarkBit mark_bit = Marking::MarkBitFrom(heap_object);
    if (mark_bit.data_only()) {
      if (heap->incremental_marking()->MarkBlackOrKeepGrey(mark_bit)) {
        MemoryChunk::IncrementLiveBytesFromGC(heap_object->address(),
                                              heap_object->Size());
      }
    } else if (Marking::IsWhite(mark_bit)) {
      heap->incremental_marking()->WhiteToGreyAndPush(heap_object, mark_bit);
    }
  }
};


class IncrementalMarkingRootMarkingVisitor : public ObjectVisitor {
 public:
  IncrementalMarkingRootMarkingVisitor(Heap* heap,
                                       IncrementalMarking* incremental_marking)
      : heap_(heap),
        incremental_marking_(incremental_marking) {
  }

  void VisitPointer(Object** p) {
    MarkObjectByPointer(p);
  }

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
      if (incremental_marking_->MarkBlackOrKeepGrey(mark_bit)) {
          MemoryChunk::IncrementLiveBytesFromGC(heap_object->address(),
                                                heap_object->Size());
      }
    } else {
      if (Marking::IsWhite(mark_bit)) {
        incremental_marking_->WhiteToGreyAndPush(heap_object, mark_bit);
      }
    }
  }

  Heap* heap_;
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
        chunk->size() > static_cast<size_t>(Page::kPageSize) &&
        is_compacting) {
      chunk->SetFlag(MemoryChunk::RESCAN_ON_EVACUATION);
    }
  } else if (chunk->owner()->identity() == CELL_SPACE ||
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
  ActivateIncrementalWriteBarrier(heap_->map_space());
  ActivateIncrementalWriteBarrier(heap_->code_space());
  ActivateIncrementalWriteBarrier(heap_->new_space());

  LargePage* lop = heap_->lo_space()->first_page();
  while (lop->is_valid()) {
    SetOldSpacePageFlags(lop, true, is_compacting_);
    lop = lop->next_page();
  }
}


bool IncrementalMarking::WorthActivating() {
#ifndef DEBUG
  static const intptr_t kActivationThreshold = 8 * MB;
#else
  // TODO(gc) consider setting this to some low level so that some
  // debug tests run with incremental marking and some without.
  static const intptr_t kActivationThreshold = 0;
#endif

  return !FLAG_expose_gc &&
      FLAG_incremental_marking &&
      !Serializer::enabled() &&
      heap_->PromotedSpaceSizeOfObjects() > kActivationThreshold;
}


void IncrementalMarking::ActivateGeneratedStub(Code* stub) {
  ASSERT(RecordWriteStub::GetMode(stub) ==
         RecordWriteStub::STORE_BUFFER_ONLY);

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

      if (CodeStub::MajorKeyFromKey(key) ==
          CodeStub::RecordWrite) {
        Object* e = stubs->ValueAt(i);
        if (e->IsCode()) {
          RecordWriteStub::Patch(Code::cast(e), mode);
        }
      }
    }
  }
}


void IncrementalMarking::EnsureMarkingDequeIsCommitted() {
  if (marking_deque_memory_ == NULL) {
    marking_deque_memory_ = new VirtualMemory(4 * MB);
  }
  if (!marking_deque_memory_committed_) {
    bool success = marking_deque_memory_->Commit(
        reinterpret_cast<Address>(marking_deque_memory_->address()),
        marking_deque_memory_->size(),
        false);  // Not executable.
    CHECK(success);
    marking_deque_memory_committed_ = true;
  }
}

void IncrementalMarking::UncommitMarkingDeque() {
  if (state_ == STOPPED && marking_deque_memory_committed_) {
    bool success = marking_deque_memory_->Uncommit(
        reinterpret_cast<Address>(marking_deque_memory_->address()),
        marking_deque_memory_->size());
    CHECK(success);
    marking_deque_memory_committed_ = false;
  }
}


void IncrementalMarking::Start() {
  if (FLAG_trace_incremental_marking) {
    PrintF("[IncrementalMarking] Start\n");
  }
  ASSERT(FLAG_incremental_marking);
  ASSERT(state_ == STOPPED);

  ResetStepCounters();

  if (heap_->old_pointer_space()->IsSweepingComplete() &&
      heap_->old_data_space()->IsSweepingComplete()) {
    StartMarking(ALLOW_COMPACTION);
  } else {
    if (FLAG_trace_incremental_marking) {
      PrintF("[IncrementalMarking] Start sweeping.\n");
    }
    state_ = SWEEPING;
  }

  heap_->new_space()->LowerInlineAllocationLimit(kAllocatedThreshold);
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


void IncrementalMarking::StartMarking(CompactionFlag flag) {
  if (FLAG_trace_incremental_marking) {
    PrintF("[IncrementalMarking] Start marking\n");
  }

  is_compacting_ = !FLAG_never_compact && (flag == ALLOW_COMPACTION) &&
      heap_->mark_compact_collector()->StartCompaction(
          MarkCompactCollector::INCREMENTAL_COMPACTION);

  state_ = MARKING;

  RecordWriteStub::Mode mode = is_compacting_ ?
      RecordWriteStub::INCREMENTAL_COMPACTION : RecordWriteStub::INCREMENTAL;

  PatchIncrementalMarkingRecordWriteStubs(heap_, mode);

  EnsureMarkingDequeIsCommitted();

  // Initialize marking stack.
  Address addr = static_cast<Address>(marking_deque_memory_->address());
  size_t size = marking_deque_memory_->size();
  if (FLAG_force_marking_deque_overflows) size = 64 * kPointerSize;
  marking_deque_.Initialize(addr, addr + size);

  ActivateIncrementalWriteBarrier();

#ifdef DEBUG
  // Marking bits are cleared by the sweeper.
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
  IncrementalMarkingRootMarkingVisitor visitor(heap_, this);
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

  int current = marking_deque_.bottom();
  int mask = marking_deque_.mask();
  int limit = marking_deque_.top();
  HeapObject** array = marking_deque_.array();
  int new_top = current;

  Map* filler_map = heap_->one_pointer_filler_map();

  while (current != limit) {
    HeapObject* obj = array[current];
    ASSERT(obj->IsHeapObject());
    current = ((current + 1) & mask);
    if (heap_->InNewSpace(obj)) {
      MapWord map_word = obj->map_word();
      if (map_word.IsForwardingAddress()) {
        HeapObject* dest = map_word.ToForwardingAddress();
        array[new_top] = dest;
        new_top = ((new_top + 1) & mask);
        ASSERT(new_top != marking_deque_.bottom());
#ifdef DEBUG
        MarkBit mark_bit = Marking::MarkBitFrom(obj);
        ASSERT(Marking::IsGrey(mark_bit) ||
               (obj->IsFiller() && Marking::IsWhite(mark_bit)));
#endif
      }
    } else if (obj->map() != filler_map) {
      // Skip one word filler objects that appear on the
      // stack when we perform in place array shift.
      array[new_top] = obj;
      new_top = ((new_top + 1) & mask);
      ASSERT(new_top != marking_deque_.bottom());
#ifdef DEBUG
        MarkBit mark_bit = Marking::MarkBitFrom(obj);
        ASSERT(Marking::IsGrey(mark_bit) ||
               (obj->IsFiller() && Marking::IsWhite(mark_bit)));
#endif
    }
  }
  marking_deque_.set_top(new_top);

  steps_took_since_last_gc_ = 0;
  steps_count_since_last_gc_ = 0;
  longest_step_ = 0.0;
}


void IncrementalMarking::Hurry() {
  if (state() == MARKING) {
    double start = 0.0;
    if (FLAG_trace_incremental_marking) {
      PrintF("[IncrementalMarking] Hurry\n");
      start = OS::TimeCurrentMillis();
    }
    // TODO(gc) hurry can mark objects it encounters black as mutator
    // was stopped.
    Map* filler_map = heap_->one_pointer_filler_map();
    Map* native_context_map = heap_->native_context_map();
    while (!marking_deque_.IsEmpty()) {
      HeapObject* obj = marking_deque_.Pop();

      // Explicitly skip one word fillers. Incremental markbit patterns are
      // correct only for objects that occupy at least two words.
      Map* map = obj->map();
      if (map == filler_map) {
        continue;
      } else if (map == native_context_map) {
        // Native contexts have weak fields.
        IncrementalMarkingMarkingVisitor::VisitNativeContext(map, obj);
      } else if (map->instance_type() == MAP_TYPE) {
        Map* map = Map::cast(obj);
        heap_->ClearCacheOnMap(map);

        // When map collection is enabled we have to mark through map's
        // transitions and back pointers in a special way to make these links
        // weak.  Only maps for subclasses of JSReceiver can have transitions.
        STATIC_ASSERT(LAST_TYPE == LAST_JS_RECEIVER_TYPE);
        if (FLAG_collect_maps &&
            map->instance_type() >= FIRST_JS_RECEIVER_TYPE) {
          marker_.MarkMapContents(map);
        } else {
          IncrementalMarkingMarkingVisitor::VisitPointers(
              heap_,
              HeapObject::RawField(map, Map::kPointerFieldsBeginOffset),
              HeapObject::RawField(map, Map::kPointerFieldsEndOffset));
        }
      } else {
        MarkBit map_mark_bit = Marking::MarkBitFrom(map);
        if (Marking::IsWhite(map_mark_bit)) {
          WhiteToGreyAndPush(map, map_mark_bit);
        }
        IncrementalMarkingMarkingVisitor::IterateBody(map, obj);
      }

      MarkBit mark_bit = Marking::MarkBitFrom(obj);
      ASSERT(!Marking::IsBlack(mark_bit));
      Marking::MarkBlack(mark_bit);
      MemoryChunk::IncrementLiveBytesFromGC(obj->address(), obj->Size());
    }
    state_ = COMPLETE;
    if (FLAG_trace_incremental_marking) {
      double end = OS::TimeCurrentMillis();
      PrintF("[IncrementalMarking] Complete (hurry), spent %d ms.\n",
             static_cast<int>(end - start));
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
  heap_->isolate()->stack_guard()->Continue(GC_REQUEST);
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
  ASSERT(marking_deque_.IsEmpty());
  heap_->isolate()->stack_guard()->Continue(GC_REQUEST);
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
    heap_->isolate()->stack_guard()->RequestGC();
  }
}


void IncrementalMarking::Step(intptr_t allocated_bytes,
                              CompletionAction action) {
  if (heap_->gc_state() != Heap::NOT_IN_GC ||
      !FLAG_incremental_marking ||
      !FLAG_incremental_marking_steps ||
      (state_ != SWEEPING && state_ != MARKING)) {
    return;
  }

  allocated_ += allocated_bytes;

  if (allocated_ < kAllocatedThreshold) return;

  if (state_ == MARKING && no_marking_scope_depth_ > 0) return;

  intptr_t bytes_to_process = allocated_ * allocation_marking_factor_;
  bytes_scanned_ += bytes_to_process;

  double start = 0;

  if (FLAG_trace_incremental_marking || FLAG_trace_gc) {
    start = OS::TimeCurrentMillis();
  }

  if (state_ == SWEEPING) {
    if (heap_->AdvanceSweepers(static_cast<int>(bytes_to_process))) {
      bytes_scanned_ = 0;
      StartMarking(PREVENT_COMPACTION);
    }
  } else if (state_ == MARKING) {
    Map* filler_map = heap_->one_pointer_filler_map();
    Map* native_context_map = heap_->native_context_map();
    while (!marking_deque_.IsEmpty() && bytes_to_process > 0) {
      HeapObject* obj = marking_deque_.Pop();

      // Explicitly skip one word fillers. Incremental markbit patterns are
      // correct only for objects that occupy at least two words.
      Map* map = obj->map();
      if (map == filler_map) continue;

      int size = obj->SizeFromMap(map);
      bytes_to_process -= size;
      MarkBit map_mark_bit = Marking::MarkBitFrom(map);
      if (Marking::IsWhite(map_mark_bit)) {
        WhiteToGreyAndPush(map, map_mark_bit);
      }

      // TODO(gc) switch to static visitor instead of normal visitor.
      if (map == native_context_map) {
        // Native contexts have weak fields.
        Context* ctx = Context::cast(obj);

        // We will mark cache black with a separate pass
        // when we finish marking.
        MarkObjectGreyDoNotEnqueue(ctx->normalized_map_cache());

        IncrementalMarkingMarkingVisitor::VisitNativeContext(map, ctx);
      } else if (map->instance_type() == MAP_TYPE) {
        Map* map = Map::cast(obj);
        heap_->ClearCacheOnMap(map);

        // When map collection is enabled we have to mark through map's
        // transitions and back pointers in a special way to make these links
        // weak.  Only maps for subclasses of JSReceiver can have transitions.
        STATIC_ASSERT(LAST_TYPE == LAST_JS_RECEIVER_TYPE);
        if (FLAG_collect_maps &&
            map->instance_type() >= FIRST_JS_RECEIVER_TYPE) {
          marker_.MarkMapContents(map);
        } else {
          IncrementalMarkingMarkingVisitor::VisitPointers(
              heap_,
              HeapObject::RawField(map, Map::kPointerFieldsBeginOffset),
              HeapObject::RawField(map, Map::kPointerFieldsEndOffset));
        }
      } else {
        IncrementalMarkingMarkingVisitor::IterateBody(map, obj);
      }

      MarkBit obj_mark_bit = Marking::MarkBitFrom(obj);
      SLOW_ASSERT(Marking::IsGrey(obj_mark_bit) ||
                  (obj->IsFiller() && Marking::IsWhite(obj_mark_bit)));
      Marking::MarkBlack(obj_mark_bit);
      MemoryChunk::IncrementLiveBytesFromGC(obj->address(), size);
    }
    if (marking_deque_.IsEmpty()) MarkingComplete(action);
  }

  allocated_ = 0;

  steps_count_++;
  steps_count_since_last_gc_++;

  bool speed_up = false;

  if ((steps_count_ % kAllocationMarkingFactorSpeedupInterval) == 0) {
    if (FLAG_trace_gc) {
      PrintPID("Speed up marking after %d steps\n",
               static_cast<int>(kAllocationMarkingFactorSpeedupInterval));
    }
    speed_up = true;
  }

  bool space_left_is_very_small =
      (old_generation_space_available_at_start_of_incremental_ < 10 * MB);

  bool only_1_nth_of_space_that_was_available_still_left =
      (SpaceLeftInOldSpace() * (allocation_marking_factor_ + 1) <
          old_generation_space_available_at_start_of_incremental_);

  if (space_left_is_very_small ||
      only_1_nth_of_space_that_was_available_still_left) {
    if (FLAG_trace_gc) PrintPID("Speed up marking because of low space left\n");
    speed_up = true;
  }

  bool size_of_old_space_multiplied_by_n_during_marking =
      (heap_->PromotedTotalSize() >
       (allocation_marking_factor_ + 1) *
           old_generation_space_used_at_start_of_incremental_);
  if (size_of_old_space_multiplied_by_n_during_marking) {
    speed_up = true;
    if (FLAG_trace_gc) {
      PrintPID("Speed up marking because of heap size increase\n");
    }
  }

  int64_t promoted_during_marking = heap_->PromotedTotalSize()
      - old_generation_space_used_at_start_of_incremental_;
  intptr_t delay = allocation_marking_factor_ * MB;
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
      allocation_marking_factor_ += kAllocationMarkingFactorSpeedup;
      allocation_marking_factor_ = static_cast<int>(
          Min(kMaxAllocationMarkingFactor,
              static_cast<intptr_t>(allocation_marking_factor_ * 1.3)));
      if (FLAG_trace_gc) {
        PrintPID("Marking speed increased to %d\n", allocation_marking_factor_);
      }
    }
  }

  if (FLAG_trace_incremental_marking || FLAG_trace_gc) {
    double end = OS::TimeCurrentMillis();
    double delta = (end - start);
    longest_step_ = Max(longest_step_, delta);
    steps_took_ += delta;
    steps_took_since_last_gc_ += delta;
  }
}


void IncrementalMarking::ResetStepCounters() {
  steps_count_ = 0;
  steps_took_ = 0;
  longest_step_ = 0.0;
  old_generation_space_available_at_start_of_incremental_ =
      SpaceLeftInOldSpace();
  old_generation_space_used_at_start_of_incremental_ =
      heap_->PromotedTotalSize();
  steps_count_since_last_gc_ = 0;
  steps_took_since_last_gc_ = 0;
  bytes_rescanned_ = 0;
  allocation_marking_factor_ = kInitialAllocationMarkingFactor;
  bytes_scanned_ = 0;
}


int64_t IncrementalMarking::SpaceLeftInOldSpace() {
  return heap_->MaxOldGenerationSize() - heap_->PromotedSpaceSizeOfObjects();
}

} }  // namespace v8::internal
