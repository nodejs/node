// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_VISITING_INL_H_
#define V8_OBJECTS_VISITING_INL_H_

#include "src/heap/array-buffer-tracker.h"
#include "src/heap/objects-visiting.h"
#include "src/ic/ic-state.h"
#include "src/macro-assembler.h"
#include "src/objects-body-descriptors-inl.h"

namespace v8 {
namespace internal {


template <typename Callback>
Callback VisitorDispatchTable<Callback>::GetVisitor(Map* map) {
  return reinterpret_cast<Callback>(callbacks_[map->visitor_id()]);
}


template <typename StaticVisitor>
void StaticNewSpaceVisitor<StaticVisitor>::Initialize() {
  table_.Register(
      kVisitShortcutCandidate,
      &FixedBodyVisitor<StaticVisitor, ConsString::BodyDescriptor, int>::Visit);

  table_.Register(
      kVisitConsString,
      &FixedBodyVisitor<StaticVisitor, ConsString::BodyDescriptor, int>::Visit);

  table_.Register(kVisitSlicedString,
                  &FixedBodyVisitor<StaticVisitor, SlicedString::BodyDescriptor,
                                    int>::Visit);

  table_.Register(
      kVisitSymbol,
      &FixedBodyVisitor<StaticVisitor, Symbol::BodyDescriptor, int>::Visit);

  table_.Register(kVisitFixedArray,
                  &FlexibleBodyVisitor<StaticVisitor,
                                       FixedArray::BodyDescriptor, int>::Visit);

  table_.Register(kVisitFixedDoubleArray, &VisitFixedDoubleArray);
  table_.Register(
      kVisitFixedTypedArray,
      &FlexibleBodyVisitor<StaticVisitor, FixedTypedArrayBase::BodyDescriptor,
                           int>::Visit);

  table_.Register(
      kVisitFixedFloat64Array,
      &FlexibleBodyVisitor<StaticVisitor, FixedTypedArrayBase::BodyDescriptor,
                           int>::Visit);

  table_.Register(
      kVisitNativeContext,
      &FixedBodyVisitor<StaticVisitor, Context::ScavengeBodyDescriptor,
                        int>::Visit);

  table_.Register(kVisitByteArray, &VisitByteArray);

  table_.Register(
      kVisitSharedFunctionInfo,
      &FixedBodyVisitor<StaticVisitor, SharedFunctionInfo::BodyDescriptor,
                        int>::Visit);

  table_.Register(kVisitSeqOneByteString, &VisitSeqOneByteString);

  table_.Register(kVisitSeqTwoByteString, &VisitSeqTwoByteString);

  // Don't visit code entry. We are using this visitor only during scavenges.
  table_.Register(
      kVisitJSFunction,
      &FlexibleBodyVisitor<StaticVisitor, JSFunction::BodyDescriptorWeakCode,
                           int>::Visit);

  table_.Register(
      kVisitJSArrayBuffer,
      &FlexibleBodyVisitor<StaticVisitor, JSArrayBuffer::BodyDescriptor,
                           int>::Visit);

  table_.Register(kVisitFreeSpace, &VisitFreeSpace);

  table_.Register(
      kVisitJSWeakCollection,
      &FlexibleBodyVisitor<StaticVisitor, JSWeakCollection::BodyDescriptor,
                           int>::Visit);

  table_.Register(kVisitJSRegExp, &JSObjectVisitor::Visit);

  table_.template RegisterSpecializations<DataObjectVisitor, kVisitDataObject,
                                          kVisitDataObjectGeneric>();

  table_.template RegisterSpecializations<JSObjectVisitor, kVisitJSObject,
                                          kVisitJSObjectGeneric>();

  // Not using specialized Api object visitor for newspace.
  table_.template RegisterSpecializations<JSObjectVisitor, kVisitJSApiObject,
                                          kVisitJSApiObjectGeneric>();

  table_.template RegisterSpecializations<StructVisitor, kVisitStruct,
                                          kVisitStructGeneric>();

  table_.Register(kVisitBytecodeArray, &UnreachableVisitor);
  table_.Register(kVisitSharedFunctionInfo, &UnreachableVisitor);
}

template <typename StaticVisitor>
void StaticMarkingVisitor<StaticVisitor>::Initialize() {
  table_.Register(kVisitShortcutCandidate,
                  &FixedBodyVisitor<StaticVisitor, ConsString::BodyDescriptor,
                                    void>::Visit);

  table_.Register(kVisitConsString,
                  &FixedBodyVisitor<StaticVisitor, ConsString::BodyDescriptor,
                                    void>::Visit);

  table_.Register(kVisitSlicedString,
                  &FixedBodyVisitor<StaticVisitor, SlicedString::BodyDescriptor,
                                    void>::Visit);

  table_.Register(
      kVisitSymbol,
      &FixedBodyVisitor<StaticVisitor, Symbol::BodyDescriptor, void>::Visit);

  table_.Register(kVisitFixedArray, &FixedArrayVisitor::Visit);

  table_.Register(kVisitFixedDoubleArray, &DataObjectVisitor::Visit);

  table_.Register(
      kVisitFixedTypedArray,
      &FlexibleBodyVisitor<StaticVisitor, FixedTypedArrayBase::BodyDescriptor,
                           void>::Visit);

  table_.Register(
      kVisitFixedFloat64Array,
      &FlexibleBodyVisitor<StaticVisitor, FixedTypedArrayBase::BodyDescriptor,
                           void>::Visit);

  table_.Register(kVisitNativeContext, &VisitNativeContext);

  table_.Register(
      kVisitAllocationSite,
      &FixedBodyVisitor<StaticVisitor, AllocationSite::MarkingBodyDescriptor,
                        void>::Visit);

  table_.Register(kVisitByteArray, &DataObjectVisitor::Visit);

  table_.Register(kVisitBytecodeArray, &VisitBytecodeArray);

  table_.Register(kVisitFreeSpace, &DataObjectVisitor::Visit);

  table_.Register(kVisitSeqOneByteString, &DataObjectVisitor::Visit);

  table_.Register(kVisitSeqTwoByteString, &DataObjectVisitor::Visit);

  table_.Register(kVisitJSWeakCollection, &VisitWeakCollection);

  table_.Register(
      kVisitOddball,
      &FixedBodyVisitor<StaticVisitor, Oddball::BodyDescriptor, void>::Visit);

  table_.Register(kVisitMap, &VisitMap);

  table_.Register(kVisitCode, &VisitCode);

  table_.Register(kVisitSharedFunctionInfo, &VisitSharedFunctionInfo);

  table_.Register(kVisitJSFunction, &VisitJSFunction);

  table_.Register(
      kVisitJSArrayBuffer,
      &FlexibleBodyVisitor<StaticVisitor, JSArrayBuffer::BodyDescriptor,
                           void>::Visit);

  table_.Register(kVisitJSRegExp, &JSObjectVisitor::Visit);

  table_.Register(
      kVisitCell,
      &FixedBodyVisitor<StaticVisitor, Cell::BodyDescriptor, void>::Visit);

  table_.Register(kVisitPropertyCell,
                  &FixedBodyVisitor<StaticVisitor, PropertyCell::BodyDescriptor,
                                    void>::Visit);

  table_.Register(kVisitWeakCell, &VisitWeakCell);

  table_.Register(kVisitTransitionArray, &VisitTransitionArray);

  table_.template RegisterSpecializations<DataObjectVisitor, kVisitDataObject,
                                          kVisitDataObjectGeneric>();

  table_.template RegisterSpecializations<JSObjectVisitor, kVisitJSObject,
                                          kVisitJSObjectGeneric>();

  table_.template RegisterSpecializations<JSApiObjectVisitor, kVisitJSApiObject,
                                          kVisitJSApiObjectGeneric>();

  table_.template RegisterSpecializations<StructObjectVisitor, kVisitStruct,
                                          kVisitStructGeneric>();
}


template <typename StaticVisitor>
void StaticMarkingVisitor<StaticVisitor>::VisitCodeEntry(
    Heap* heap, HeapObject* object, Address entry_address) {
  Code* code = Code::cast(Code::GetObjectFromEntryAddress(entry_address));
  heap->mark_compact_collector()->RecordCodeEntrySlot(object, entry_address,
                                                      code);
  StaticVisitor::MarkObject(heap, code);
}


template <typename StaticVisitor>
void StaticMarkingVisitor<StaticVisitor>::VisitEmbeddedPointer(
    Heap* heap, RelocInfo* rinfo) {
  DCHECK(rinfo->rmode() == RelocInfo::EMBEDDED_OBJECT);
  HeapObject* object = HeapObject::cast(rinfo->target_object());
  Code* host = rinfo->host();
  heap->mark_compact_collector()->RecordRelocSlot(host, rinfo, object);
  // TODO(ulan): It could be better to record slots only for strongly embedded
  // objects here and record slots for weakly embedded object during clearing
  // of non-live references in mark-compact.
  if (!host->IsWeakObject(object)) {
    StaticVisitor::MarkObject(heap, object);
  }
}


template <typename StaticVisitor>
void StaticMarkingVisitor<StaticVisitor>::VisitCell(Heap* heap,
                                                    RelocInfo* rinfo) {
  DCHECK(rinfo->rmode() == RelocInfo::CELL);
  Cell* cell = rinfo->target_cell();
  Code* host = rinfo->host();
  heap->mark_compact_collector()->RecordRelocSlot(host, rinfo, cell);
  if (!host->IsWeakObject(cell)) {
    StaticVisitor::MarkObject(heap, cell);
  }
}


template <typename StaticVisitor>
void StaticMarkingVisitor<StaticVisitor>::VisitDebugTarget(Heap* heap,
                                                           RelocInfo* rinfo) {
  DCHECK(RelocInfo::IsDebugBreakSlot(rinfo->rmode()) &&
         rinfo->IsPatchedDebugBreakSlotSequence());
  Code* target = Code::GetCodeFromTargetAddress(rinfo->debug_call_address());
  Code* host = rinfo->host();
  heap->mark_compact_collector()->RecordRelocSlot(host, rinfo, target);
  StaticVisitor::MarkObject(heap, target);
}


template <typename StaticVisitor>
void StaticMarkingVisitor<StaticVisitor>::VisitCodeTarget(Heap* heap,
                                                          RelocInfo* rinfo) {
  DCHECK(RelocInfo::IsCodeTarget(rinfo->rmode()));
  Code* target = Code::GetCodeFromTargetAddress(rinfo->target_address());
  // Monomorphic ICs are preserved when possible, but need to be flushed
  // when they might be keeping a Context alive, or when the heap is about
  // to be serialized.
  if (FLAG_cleanup_code_caches_at_gc && target->is_inline_cache_stub() &&
      (heap->isolate()->serializer_enabled() ||
       target->ic_age() != heap->global_ic_age())) {
    ICUtility::Clear(heap->isolate(), rinfo->pc(),
                     rinfo->host()->constant_pool());
    target = Code::GetCodeFromTargetAddress(rinfo->target_address());
  }
  Code* host = rinfo->host();
  heap->mark_compact_collector()->RecordRelocSlot(host, rinfo, target);
  StaticVisitor::MarkObject(heap, target);
}

template <typename StaticVisitor>
void StaticMarkingVisitor<StaticVisitor>::VisitCodeAgeSequence(
    Heap* heap, RelocInfo* rinfo) {
  DCHECK(RelocInfo::IsCodeAgeSequence(rinfo->rmode()));
  Code* target = rinfo->code_age_stub();
  DCHECK(target != NULL);
  Code* host = rinfo->host();
  heap->mark_compact_collector()->RecordRelocSlot(host, rinfo, target);
  StaticVisitor::MarkObject(heap, target);
}

template <typename StaticVisitor>
void StaticMarkingVisitor<StaticVisitor>::VisitBytecodeArray(
    Map* map, HeapObject* object) {
  FixedBodyVisitor<StaticVisitor, BytecodeArray::MarkingBodyDescriptor,
                   void>::Visit(map, object);
  BytecodeArray::cast(object)->MakeOlder();
}

template <typename StaticVisitor>
void StaticMarkingVisitor<StaticVisitor>::VisitNativeContext(
    Map* map, HeapObject* object) {
  FixedBodyVisitor<StaticVisitor, Context::MarkCompactBodyDescriptor,
                   void>::Visit(map, object);
}


template <typename StaticVisitor>
void StaticMarkingVisitor<StaticVisitor>::VisitMap(Map* map,
                                                   HeapObject* object) {
  Heap* heap = map->GetHeap();
  Map* map_object = Map::cast(object);

  // Clears the cache of ICs related to this map.
  if (FLAG_cleanup_code_caches_at_gc) {
    map_object->ClearCodeCache(heap);
  }

  // When map collection is enabled we have to mark through map's transitions
  // and back pointers in a special way to make these links weak.
  if (map_object->CanTransition()) {
    MarkMapContents(heap, map_object);
  } else {
    StaticVisitor::VisitPointers(
        heap, object,
        HeapObject::RawField(object, Map::kPointerFieldsBeginOffset),
        HeapObject::RawField(object, Map::kPointerFieldsEndOffset));
  }
}

template <typename StaticVisitor>
void StaticMarkingVisitor<StaticVisitor>::VisitWeakCell(Map* map,
                                                        HeapObject* object) {
  Heap* heap = map->GetHeap();
  WeakCell* weak_cell = reinterpret_cast<WeakCell*>(object);
  // Enqueue weak cell in linked list of encountered weak collections.
  // We can ignore weak cells with cleared values because they will always
  // contain smi zero.
  if (weak_cell->next_cleared() && !weak_cell->cleared()) {
    HeapObject* value = HeapObject::cast(weak_cell->value());
    if (MarkCompactCollector::IsMarked(value)) {
      // Weak cells with live values are directly processed here to reduce
      // the processing time of weak cells during the main GC pause.
      Object** slot = HeapObject::RawField(weak_cell, WeakCell::kValueOffset);
      map->GetHeap()->mark_compact_collector()->RecordSlot(weak_cell, slot,
                                                           *slot);
    } else {
      // If we do not know about liveness of values of weak cells, we have to
      // process them when we know the liveness of the whole transitive
      // closure.
      weak_cell->set_next(heap->encountered_weak_cells(),
                          UPDATE_WEAK_WRITE_BARRIER);
      heap->set_encountered_weak_cells(weak_cell);
    }
  }
}


template <typename StaticVisitor>
void StaticMarkingVisitor<StaticVisitor>::VisitTransitionArray(
    Map* map, HeapObject* object) {
  TransitionArray* array = TransitionArray::cast(object);
  Heap* heap = array->GetHeap();
  // Visit strong references.
  if (array->HasPrototypeTransitions()) {
    StaticVisitor::VisitPointer(heap, array,
                                array->GetPrototypeTransitionsSlot());
  }
  int num_transitions = TransitionArray::NumberOfTransitions(array);
  for (int i = 0; i < num_transitions; ++i) {
    StaticVisitor::VisitPointer(heap, array, array->GetKeySlot(i));
  }
  // Enqueue the array in linked list of encountered transition arrays if it is
  // not already in the list.
  if (array->next_link()->IsUndefined(heap->isolate())) {
    Heap* heap = map->GetHeap();
    array->set_next_link(heap->encountered_transition_arrays(),
                         UPDATE_WEAK_WRITE_BARRIER);
    heap->set_encountered_transition_arrays(array);
  }
}

template <typename StaticVisitor>
void StaticMarkingVisitor<StaticVisitor>::VisitWeakCollection(
    Map* map, HeapObject* object) {
  typedef FlexibleBodyVisitor<StaticVisitor,
                              JSWeakCollection::BodyDescriptorWeak,
                              void> JSWeakCollectionBodyVisitor;
  Heap* heap = map->GetHeap();
  JSWeakCollection* weak_collection =
      reinterpret_cast<JSWeakCollection*>(object);

  // Enqueue weak collection in linked list of encountered weak collections.
  if (weak_collection->next() == heap->undefined_value()) {
    weak_collection->set_next(heap->encountered_weak_collections());
    heap->set_encountered_weak_collections(weak_collection);
  }

  // Skip visiting the backing hash table containing the mappings and the
  // pointer to the other enqueued weak collections, both are post-processed.
  JSWeakCollectionBodyVisitor::Visit(map, object);

  // Partially initialized weak collection is enqueued, but table is ignored.
  if (!weak_collection->table()->IsHashTable()) return;

  // Mark the backing hash table without pushing it on the marking stack.
  Object** slot = HeapObject::RawField(object, JSWeakCollection::kTableOffset);
  HeapObject* obj = HeapObject::cast(*slot);
  heap->mark_compact_collector()->RecordSlot(object, slot, obj);
  StaticVisitor::MarkObjectWithoutPush(heap, obj);
}


template <typename StaticVisitor>
void StaticMarkingVisitor<StaticVisitor>::VisitCode(Map* map,
                                                    HeapObject* object) {
  typedef FlexibleBodyVisitor<StaticVisitor, Code::BodyDescriptor, void>
      CodeBodyVisitor;
  Heap* heap = map->GetHeap();
  Code* code = Code::cast(object);
  if (FLAG_age_code && !heap->isolate()->serializer_enabled()) {
    code->MakeOlder();
  }
  CodeBodyVisitor::Visit(map, object);
}


template <typename StaticVisitor>
void StaticMarkingVisitor<StaticVisitor>::VisitSharedFunctionInfo(
    Map* map, HeapObject* object) {
  Heap* heap = map->GetHeap();
  SharedFunctionInfo* shared = SharedFunctionInfo::cast(object);
  if (shared->ic_age() != heap->global_ic_age()) {
    shared->ResetForNewContext(heap->global_ic_age());
  }
  MarkCompactCollector* collector = heap->mark_compact_collector();
  if (collector->is_code_flushing_enabled()) {
    if (IsFlushable(heap, shared)) {
      // This function's code looks flushable. But we have to postpone
      // the decision until we see all functions that point to the same
      // SharedFunctionInfo because some of them might be optimized.
      // That would also make the non-optimized version of the code
      // non-flushable, because it is required for bailing out from
      // optimized code.
      collector->code_flusher()->AddCandidate(shared);
      // Treat the reference to the code object weakly.
      VisitSharedFunctionInfoWeakCode(map, object);
      return;
    }
  }
  VisitSharedFunctionInfoStrongCode(map, object);
}


template <typename StaticVisitor>
void StaticMarkingVisitor<StaticVisitor>::VisitJSFunction(Map* map,
                                                          HeapObject* object) {
  Heap* heap = map->GetHeap();
  JSFunction* function = JSFunction::cast(object);
  if (FLAG_cleanup_code_caches_at_gc) {
    function->ClearTypeFeedbackInfoAtGCTime();
  }
  MarkCompactCollector* collector = heap->mark_compact_collector();
  if (collector->is_code_flushing_enabled()) {
    if (IsFlushable(heap, function)) {
      // This function's code looks flushable. But we have to postpone
      // the decision until we see all functions that point to the same
      // SharedFunctionInfo because some of them might be optimized.
      // That would also make the non-optimized version of the code
      // non-flushable, because it is required for bailing out from
      // optimized code.
      collector->code_flusher()->AddCandidate(function);
      // Treat the reference to the code object weakly.
      VisitJSFunctionWeakCode(map, object);
      return;
    } else {
      // Visit all unoptimized code objects to prevent flushing them.
      StaticVisitor::MarkObject(heap, function->shared()->code());
    }
  }
  VisitJSFunctionStrongCode(map, object);
}

template <typename StaticVisitor>
void StaticMarkingVisitor<StaticVisitor>::MarkMapContents(Heap* heap,
                                                          Map* map) {
  // Since descriptor arrays are potentially shared, ensure that only the
  // descriptors that belong to this map are marked. The first time a non-empty
  // descriptor array is marked, its header is also visited. The slot holding
  // the descriptor array will be implicitly recorded when the pointer fields of
  // this map are visited.  Prototype maps don't keep track of transitions, so
  // just mark the entire descriptor array.
  if (!map->is_prototype_map()) {
    DescriptorArray* descriptors = map->instance_descriptors();
    if (StaticVisitor::MarkObjectWithoutPush(heap, descriptors) &&
        descriptors->length() > 0) {
      StaticVisitor::VisitPointers(heap, descriptors,
                                   descriptors->GetFirstElementAddress(),
                                   descriptors->GetDescriptorEndSlot(0));
    }
    int start = 0;
    int end = map->NumberOfOwnDescriptors();
    if (start < end) {
      StaticVisitor::VisitPointers(heap, descriptors,
                                   descriptors->GetDescriptorStartSlot(start),
                                   descriptors->GetDescriptorEndSlot(end));
    }
  }

  // Mark the pointer fields of the Map. Since the transitions array has
  // been marked already, it is fine that one of these fields contains a
  // pointer to it.
  StaticVisitor::VisitPointers(
      heap, map, HeapObject::RawField(map, Map::kPointerFieldsBeginOffset),
      HeapObject::RawField(map, Map::kPointerFieldsEndOffset));
}


inline static bool HasSourceCode(Heap* heap, SharedFunctionInfo* info) {
  Object* undefined = heap->undefined_value();
  return (info->script() != undefined) &&
         (reinterpret_cast<Script*>(info->script())->source() != undefined);
}


template <typename StaticVisitor>
bool StaticMarkingVisitor<StaticVisitor>::IsFlushable(Heap* heap,
                                                      JSFunction* function) {
  SharedFunctionInfo* shared_info = function->shared();

  // Code is either on stack, in compilation cache or referenced
  // by optimized version of function.
  MarkBit code_mark = ObjectMarking::MarkBitFrom(function->code());
  if (Marking::IsBlackOrGrey(code_mark)) {
    return false;
  }

  // We do not (yet) flush code for optimized functions.
  if (function->code() != shared_info->code()) {
    return false;
  }

  // Check age of optimized code.
  if (FLAG_age_code && !function->code()->IsOld()) {
    return false;
  }

  return IsFlushable(heap, shared_info);
}


template <typename StaticVisitor>
bool StaticMarkingVisitor<StaticVisitor>::IsFlushable(
    Heap* heap, SharedFunctionInfo* shared_info) {
  // Code is either on stack, in compilation cache or referenced
  // by optimized version of function.
  MarkBit code_mark = ObjectMarking::MarkBitFrom(shared_info->code());
  if (Marking::IsBlackOrGrey(code_mark)) {
    return false;
  }

  // The function must be compiled and have the source code available,
  // to be able to recompile it in case we need the function again.
  if (!(shared_info->is_compiled() && HasSourceCode(heap, shared_info))) {
    return false;
  }

  // We never flush code for API functions.
  if (shared_info->IsApiFunction()) {
    return false;
  }

  // Only flush code for functions.
  if (shared_info->code()->kind() != Code::FUNCTION) {
    return false;
  }

  // Function must be lazy compilable.
  if (!shared_info->allows_lazy_compilation()) {
    return false;
  }

  // We do not (yet?) flush code for generator functions, or async functions,
  // because we don't know if there are still live activations
  // (generator objects) on the heap.
  if (IsResumableFunction(shared_info->kind())) {
    return false;
  }

  // If this is a full script wrapped in a function we do not flush the code.
  if (shared_info->is_toplevel()) {
    return false;
  }

  // The function must be user code.
  if (!shared_info->IsUserJavaScript()) {
    return false;
  }

  // Maintain debug break slots in the code.
  if (shared_info->HasDebugCode()) {
    return false;
  }

  // If this is a function initialized with %SetCode then the one-to-one
  // relation between SharedFunctionInfo and Code is broken.
  if (shared_info->dont_flush()) {
    return false;
  }

  // Check age of code. If code aging is disabled we never flush.
  if (!FLAG_age_code || !shared_info->code()->IsOld()) {
    return false;
  }

  return true;
}

template <typename StaticVisitor>
void StaticMarkingVisitor<StaticVisitor>::VisitSharedFunctionInfoStrongCode(
    Map* map, HeapObject* object) {
  FixedBodyVisitor<StaticVisitor, SharedFunctionInfo::BodyDescriptor,
                   void>::Visit(map, object);
}

template <typename StaticVisitor>
void StaticMarkingVisitor<StaticVisitor>::VisitSharedFunctionInfoWeakCode(
    Map* map, HeapObject* object) {
  // Skip visiting kCodeOffset as it is treated weakly here.
  STATIC_ASSERT(SharedFunctionInfo::kCodeOffset <
                SharedFunctionInfo::BodyDescriptorWeakCode::kStartOffset);
  FixedBodyVisitor<StaticVisitor, SharedFunctionInfo::BodyDescriptorWeakCode,
                   void>::Visit(map, object);
}

template <typename StaticVisitor>
void StaticMarkingVisitor<StaticVisitor>::VisitJSFunctionStrongCode(
    Map* map, HeapObject* object) {
  typedef FlexibleBodyVisitor<StaticVisitor,
                              JSFunction::BodyDescriptorStrongCode,
                              void> JSFunctionStrongCodeBodyVisitor;
  JSFunctionStrongCodeBodyVisitor::Visit(map, object);
}


template <typename StaticVisitor>
void StaticMarkingVisitor<StaticVisitor>::VisitJSFunctionWeakCode(
    Map* map, HeapObject* object) {
  typedef FlexibleBodyVisitor<StaticVisitor, JSFunction::BodyDescriptorWeakCode,
                              void> JSFunctionWeakCodeBodyVisitor;
  JSFunctionWeakCodeBodyVisitor::Visit(map, object);
}


}  // namespace internal
}  // namespace v8

#endif  // V8_OBJECTS_VISITING_INL_H_
