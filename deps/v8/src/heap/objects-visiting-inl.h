// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_VISITING_INL_H_
#define V8_OBJECTS_VISITING_INL_H_

#include "src/heap/objects-visiting.h"

#include "src/heap/array-buffer-tracker.h"
#include "src/heap/embedder-tracing.h"
#include "src/heap/mark-compact.h"
#include "src/macro-assembler.h"
#include "src/objects-body-descriptors-inl.h"

namespace v8 {
namespace internal {

template <typename ResultType, typename ConcreteVisitor>
ResultType HeapVisitor<ResultType, ConcreteVisitor>::Visit(HeapObject* object) {
  return Visit(object->map(), object);
}

template <typename ResultType, typename ConcreteVisitor>
ResultType HeapVisitor<ResultType, ConcreteVisitor>::Visit(Map* map,
                                                           HeapObject* object) {
  ConcreteVisitor* visitor = static_cast<ConcreteVisitor*>(this);
  switch (static_cast<VisitorId>(map->visitor_id())) {
#define CASE(type)   \
  case kVisit##type: \
    return visitor->Visit##type(map, type::cast(object));
    TYPED_VISITOR_ID_LIST(CASE)
#undef CASE
    case kVisitShortcutCandidate:
      return visitor->VisitShortcutCandidate(map, ConsString::cast(object));
    case kVisitNativeContext:
      return visitor->VisitNativeContext(map, Context::cast(object));
    case kVisitDataObject:
      return visitor->VisitDataObject(map, HeapObject::cast(object));
    case kVisitJSObjectFast:
      return visitor->VisitJSObjectFast(map, JSObject::cast(object));
    case kVisitJSApiObject:
      return visitor->VisitJSApiObject(map, JSObject::cast(object));
    case kVisitStruct:
      return visitor->VisitStruct(map, HeapObject::cast(object));
    case kVisitFreeSpace:
      return visitor->VisitFreeSpace(map, FreeSpace::cast(object));
    case kVisitorIdCount:
      UNREACHABLE();
  }
  UNREACHABLE();
  // Make the compiler happy.
  return ResultType();
}

template <typename ResultType, typename ConcreteVisitor>
void HeapVisitor<ResultType, ConcreteVisitor>::VisitMapPointer(
    HeapObject* host, HeapObject** map) {
  static_cast<ConcreteVisitor*>(this)->VisitPointer(
      host, reinterpret_cast<Object**>(map));
}

#define VISIT(type)                                                 \
  template <typename ResultType, typename ConcreteVisitor>          \
  ResultType HeapVisitor<ResultType, ConcreteVisitor>::Visit##type( \
      Map* map, type* object) {                                     \
    ConcreteVisitor* visitor = static_cast<ConcreteVisitor*>(this); \
    if (!visitor->ShouldVisit(object)) return ResultType();         \
    int size = type::BodyDescriptor::SizeOf(map, object);           \
    if (visitor->ShouldVisitMapPointer())                           \
      visitor->VisitMapPointer(object, object->map_slot());         \
    type::BodyDescriptor::IterateBody(object, size, visitor);       \
    return static_cast<ResultType>(size);                           \
  }
TYPED_VISITOR_ID_LIST(VISIT)
#undef VISIT

template <typename ResultType, typename ConcreteVisitor>
ResultType HeapVisitor<ResultType, ConcreteVisitor>::VisitShortcutCandidate(
    Map* map, ConsString* object) {
  ConcreteVisitor* visitor = static_cast<ConcreteVisitor*>(this);
  if (!visitor->ShouldVisit(object)) return ResultType();
  int size = ConsString::BodyDescriptor::SizeOf(map, object);
  if (visitor->ShouldVisitMapPointer())
    visitor->VisitMapPointer(object, object->map_slot());
  ConsString::BodyDescriptor::IterateBody(object, size, visitor);
  return static_cast<ResultType>(size);
}

template <typename ResultType, typename ConcreteVisitor>
ResultType HeapVisitor<ResultType, ConcreteVisitor>::VisitNativeContext(
    Map* map, Context* object) {
  ConcreteVisitor* visitor = static_cast<ConcreteVisitor*>(this);
  if (!visitor->ShouldVisit(object)) return ResultType();
  int size = Context::BodyDescriptor::SizeOf(map, object);
  if (visitor->ShouldVisitMapPointer())
    visitor->VisitMapPointer(object, object->map_slot());
  Context::BodyDescriptor::IterateBody(object, size, visitor);
  return static_cast<ResultType>(size);
}

template <typename ResultType, typename ConcreteVisitor>
ResultType HeapVisitor<ResultType, ConcreteVisitor>::VisitDataObject(
    Map* map, HeapObject* object) {
  ConcreteVisitor* visitor = static_cast<ConcreteVisitor*>(this);
  if (!visitor->ShouldVisit(object)) return ResultType();
  int size = map->instance_size();
  if (visitor->ShouldVisitMapPointer())
    visitor->VisitMapPointer(object, object->map_slot());
  return static_cast<ResultType>(size);
}

template <typename ResultType, typename ConcreteVisitor>
ResultType HeapVisitor<ResultType, ConcreteVisitor>::VisitJSObjectFast(
    Map* map, JSObject* object) {
  ConcreteVisitor* visitor = static_cast<ConcreteVisitor*>(this);
  if (!visitor->ShouldVisit(object)) return ResultType();
  int size = JSObject::FastBodyDescriptor::SizeOf(map, object);
  if (visitor->ShouldVisitMapPointer())
    visitor->VisitMapPointer(object, object->map_slot());
  JSObject::FastBodyDescriptor::IterateBody(object, size, visitor);
  return static_cast<ResultType>(size);
}

template <typename ResultType, typename ConcreteVisitor>
ResultType HeapVisitor<ResultType, ConcreteVisitor>::VisitJSApiObject(
    Map* map, JSObject* object) {
  ConcreteVisitor* visitor = static_cast<ConcreteVisitor*>(this);
  if (!visitor->ShouldVisit(object)) return ResultType();
  int size = JSObject::BodyDescriptor::SizeOf(map, object);
  if (visitor->ShouldVisitMapPointer())
    visitor->VisitMapPointer(object, object->map_slot());
  JSObject::BodyDescriptor::IterateBody(object, size, visitor);
  return static_cast<ResultType>(size);
}

template <typename ResultType, typename ConcreteVisitor>
ResultType HeapVisitor<ResultType, ConcreteVisitor>::VisitStruct(
    Map* map, HeapObject* object) {
  ConcreteVisitor* visitor = static_cast<ConcreteVisitor*>(this);
  if (!visitor->ShouldVisit(object)) return ResultType();
  int size = map->instance_size();
  if (visitor->ShouldVisitMapPointer())
    visitor->VisitMapPointer(object, object->map_slot());
  StructBodyDescriptor::IterateBody(object, size, visitor);
  return static_cast<ResultType>(size);
}

template <typename ResultType, typename ConcreteVisitor>
ResultType HeapVisitor<ResultType, ConcreteVisitor>::VisitFreeSpace(
    Map* map, FreeSpace* object) {
  ConcreteVisitor* visitor = static_cast<ConcreteVisitor*>(this);
  if (!visitor->ShouldVisit(object)) return ResultType();
  if (visitor->ShouldVisitMapPointer())
    visitor->VisitMapPointer(object, object->map_slot());
  return static_cast<ResultType>(FreeSpace::cast(object)->size());
}

template <typename ConcreteVisitor>
int NewSpaceVisitor<ConcreteVisitor>::VisitJSFunction(Map* map,
                                                      JSFunction* object) {
  ConcreteVisitor* visitor = static_cast<ConcreteVisitor*>(this);
  int size = JSFunction::BodyDescriptorWeak::SizeOf(map, object);
  JSFunction::BodyDescriptorWeak::IterateBody(object, size, visitor);
  return size;
}

template <typename ConcreteVisitor>
int NewSpaceVisitor<ConcreteVisitor>::VisitNativeContext(Map* map,
                                                         Context* object) {
  ConcreteVisitor* visitor = static_cast<ConcreteVisitor*>(this);
  int size = Context::BodyDescriptor::SizeOf(map, object);
  Context::BodyDescriptor::IterateBody(object, size, visitor);
  return size;
}

template <typename ConcreteVisitor>
int NewSpaceVisitor<ConcreteVisitor>::VisitJSApiObject(Map* map,
                                                       JSObject* object) {
  ConcreteVisitor* visitor = static_cast<ConcreteVisitor*>(this);
  return visitor->VisitJSObject(map, object);
}

template <typename ConcreteVisitor>
int MarkingVisitor<ConcreteVisitor>::VisitJSFunction(Map* map,
                                                     JSFunction* object) {
  ConcreteVisitor* visitor = static_cast<ConcreteVisitor*>(this);
  int size = JSFunction::BodyDescriptorWeak::SizeOf(map, object);
  JSFunction::BodyDescriptorWeak::IterateBody(object, size, visitor);
  return size;
}

template <typename ConcreteVisitor>
int MarkingVisitor<ConcreteVisitor>::VisitTransitionArray(
    Map* map, TransitionArray* array) {
  ConcreteVisitor* visitor = static_cast<ConcreteVisitor*>(this);
  // Visit strong references.
  if (array->HasPrototypeTransitions()) {
    visitor->VisitPointer(array, array->GetPrototypeTransitionsSlot());
  }
  int num_transitions = array->number_of_entries();
  for (int i = 0; i < num_transitions; ++i) {
    visitor->VisitPointer(array, array->GetKeySlot(i));
    // A TransitionArray can hold maps or (transitioning StoreIC) handlers.
    // Maps have custom weak handling; handlers (which in turn weakly point
    // to maps) are marked strongly for now, and will be cleared during
    // compaction when the maps they refer to are dead.
    if (!array->GetRawTarget(i)->IsMap()) {
      visitor->VisitPointer(array, array->GetTargetSlot(i));
    }
  }
  collector_->AddTransitionArray(array);
  return TransitionArray::BodyDescriptor::SizeOf(map, array);
}

template <typename ConcreteVisitor>
int MarkingVisitor<ConcreteVisitor>::VisitWeakCell(Map* map,
                                                   WeakCell* weak_cell) {
  // Enqueue weak cell in linked list of encountered weak collections.
  // We can ignore weak cells with cleared values because they will always
  // contain smi zero.
  if (!weak_cell->cleared()) {
    HeapObject* value = HeapObject::cast(weak_cell->value());
    if (heap_->incremental_marking()->marking_state()->IsBlackOrGrey(value)) {
      // Weak cells with live values are directly processed here to reduce
      // the processing time of weak cells during the main GC pause.
      Object** slot = HeapObject::RawField(weak_cell, WeakCell::kValueOffset);
      collector_->RecordSlot(weak_cell, slot, *slot);
    } else {
      // If we do not know about liveness of values of weak cells, we have to
      // process them when we know the liveness of the whole transitive
      // closure.
      collector_->AddWeakCell(weak_cell);
    }
  }
  return WeakCell::BodyDescriptor::SizeOf(map, weak_cell);
}

template <typename ConcreteVisitor>
int MarkingVisitor<ConcreteVisitor>::VisitNativeContext(Map* map,
                                                        Context* context) {
  ConcreteVisitor* visitor = static_cast<ConcreteVisitor*>(this);
  int size = Context::BodyDescriptorWeak::SizeOf(map, context);
  Context::BodyDescriptorWeak::IterateBody(context, size, visitor);
  return size;
}

template <typename ConcreteVisitor>
int MarkingVisitor<ConcreteVisitor>::VisitJSWeakCollection(
    Map* map, JSWeakCollection* weak_collection) {
  ConcreteVisitor* visitor = static_cast<ConcreteVisitor*>(this);

  // Enqueue weak collection in linked list of encountered weak collections.
  if (weak_collection->next() == heap_->undefined_value()) {
    weak_collection->set_next(heap_->encountered_weak_collections());
    heap_->set_encountered_weak_collections(weak_collection);
  }

  // Skip visiting the backing hash table containing the mappings and the
  // pointer to the other enqueued weak collections, both are post-processed.
  int size = JSWeakCollection::BodyDescriptorWeak::SizeOf(map, weak_collection);
  JSWeakCollection::BodyDescriptorWeak::IterateBody(weak_collection, size,
                                                    visitor);

  // Partially initialized weak collection is enqueued, but table is ignored.
  if (!weak_collection->table()->IsHashTable()) return size;

  // Mark the backing hash table without pushing it on the marking stack.
  Object** slot =
      HeapObject::RawField(weak_collection, JSWeakCollection::kTableOffset);
  HeapObject* obj = HeapObject::cast(*slot);
  collector_->RecordSlot(weak_collection, slot, obj);
  visitor->MarkObjectWithoutPush(weak_collection, obj);
  return size;
}

template <typename ConcreteVisitor>
int MarkingVisitor<ConcreteVisitor>::VisitBytecodeArray(Map* map,
                                                        BytecodeArray* array) {
  ConcreteVisitor* visitor = static_cast<ConcreteVisitor*>(this);
  int size = BytecodeArray::BodyDescriptor::SizeOf(map, array);
  BytecodeArray::BodyDescriptor::IterateBody(array, size, visitor);
  array->MakeOlder();
  return size;
}

template <typename ConcreteVisitor>
int MarkingVisitor<ConcreteVisitor>::VisitCode(Map* map, Code* code) {
  ConcreteVisitor* visitor = static_cast<ConcreteVisitor*>(this);
  int size = Code::BodyDescriptor::SizeOf(map, code);
  Code::BodyDescriptor::IterateBody(code, size, visitor);
  return size;
}

template <typename ConcreteVisitor>
void MarkingVisitor<ConcreteVisitor>::MarkMapContents(Map* map) {
  ConcreteVisitor* visitor = static_cast<ConcreteVisitor*>(this);
  // Since descriptor arrays are potentially shared, ensure that only the
  // descriptors that belong to this map are marked. The first time a non-empty
  // descriptor array is marked, its header is also visited. The slot holding
  // the descriptor array will be implicitly recorded when the pointer fields of
  // this map are visited.  Prototype maps don't keep track of transitions, so
  // just mark the entire descriptor array.
  if (!map->is_prototype_map()) {
    DescriptorArray* descriptors = map->instance_descriptors();
    if (visitor->MarkObjectWithoutPush(map, descriptors) &&
        descriptors->length() > 0) {
      visitor->VisitPointers(descriptors, descriptors->GetFirstElementAddress(),
                             descriptors->GetDescriptorEndSlot(0));
    }
    int start = 0;
    int end = map->NumberOfOwnDescriptors();
    if (start < end) {
      visitor->VisitPointers(descriptors,
                             descriptors->GetDescriptorStartSlot(start),
                             descriptors->GetDescriptorEndSlot(end));
    }
  }

  // Mark the pointer fields of the Map. Since the transitions array has
  // been marked already, it is fine that one of these fields contains a
  // pointer to it.
  visitor->VisitPointers(
      map, HeapObject::RawField(map, Map::kPointerFieldsBeginOffset),
      HeapObject::RawField(map, Map::kPointerFieldsEndOffset));
}

template <typename ConcreteVisitor>
int MarkingVisitor<ConcreteVisitor>::VisitMap(Map* map, Map* object) {
  ConcreteVisitor* visitor = static_cast<ConcreteVisitor*>(this);

  // Clears the cache of ICs related to this map.
  if (FLAG_cleanup_code_caches_at_gc) {
    object->ClearCodeCache(heap_);
  }

  // When map collection is enabled we have to mark through map's transitions
  // and back pointers in a special way to make these links weak.
  if (object->CanTransition()) {
    MarkMapContents(object);
  } else {
    visitor->VisitPointers(
        object, HeapObject::RawField(object, Map::kPointerFieldsBeginOffset),
        HeapObject::RawField(object, Map::kPointerFieldsEndOffset));
  }
  return Map::BodyDescriptor::SizeOf(map, object);
}

template <typename ConcreteVisitor>
int MarkingVisitor<ConcreteVisitor>::VisitJSApiObject(Map* map,
                                                      JSObject* object) {
  ConcreteVisitor* visitor = static_cast<ConcreteVisitor*>(this);
  if (heap_->local_embedder_heap_tracer()->InUse()) {
    DCHECK(object->IsJSObject());
    heap_->TracePossibleWrapper(object);
  }
  int size = JSObject::BodyDescriptor::SizeOf(map, object);
  JSObject::BodyDescriptor::IterateBody(object, size, visitor);
  return size;
}

template <typename ConcreteVisitor>
int MarkingVisitor<ConcreteVisitor>::VisitAllocationSite(
    Map* map, AllocationSite* object) {
  ConcreteVisitor* visitor = static_cast<ConcreteVisitor*>(this);
  int size = AllocationSite::BodyDescriptorWeak::SizeOf(map, object);
  AllocationSite::BodyDescriptorWeak::IterateBody(object, size, visitor);
  return size;
}

template <typename ConcreteVisitor>
void MarkingVisitor<ConcreteVisitor>::VisitEmbeddedPointer(Code* host,
                                                           RelocInfo* rinfo) {
  ConcreteVisitor* visitor = static_cast<ConcreteVisitor*>(this);
  DCHECK(rinfo->rmode() == RelocInfo::EMBEDDED_OBJECT);
  HeapObject* object = HeapObject::cast(rinfo->target_object());
  collector_->RecordRelocSlot(host, rinfo, object);
  if (!host->IsWeakObject(object)) {
    visitor->MarkObject(host, object);
  }
}

template <typename ConcreteVisitor>
void MarkingVisitor<ConcreteVisitor>::VisitCodeTarget(Code* host,
                                                      RelocInfo* rinfo) {
  ConcreteVisitor* visitor = static_cast<ConcreteVisitor*>(this);
  DCHECK(RelocInfo::IsCodeTarget(rinfo->rmode()));
  Code* target = Code::GetCodeFromTargetAddress(rinfo->target_address());
  collector_->RecordRelocSlot(host, rinfo, target);
  visitor->MarkObject(host, target);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_OBJECTS_VISITING_INL_H_
