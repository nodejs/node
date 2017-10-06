// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_SCAVENGER_INL_H_
#define V8_HEAP_SCAVENGER_INL_H_

#include "src/heap/scavenger.h"
#include "src/objects/map.h"

namespace v8 {
namespace internal {

namespace {

// White list for objects that for sure only contain data.
bool ContainsOnlyData(VisitorId visitor_id) {
  switch (visitor_id) {
    case kVisitSeqOneByteString:
      return true;
    case kVisitSeqTwoByteString:
      return true;
    case kVisitByteArray:
      return true;
    case kVisitFixedDoubleArray:
      return true;
    case kVisitDataObject:
      return true;
    default:
      break;
  }
  return false;
}

}  // namespace

void Scavenger::MigrateObject(Map* map, HeapObject* source, HeapObject* target,
                              int size) {
  // Copy the content of source to target.
  heap()->CopyBlock(target->address(), source->address(), size);

  // Set the forwarding address.
  source->set_map_word(MapWord::FromForwardingAddress(target));

  if (V8_UNLIKELY(is_logging_)) {
    // Update NewSpace stats if necessary.
    RecordCopiedObject(target);
    heap()->OnMoveEvent(target, source, size);
  }

  if (is_incremental_marking_) {
    heap()->incremental_marking()->TransferColor(source, target);
  }
  heap()->UpdateAllocationSite<Heap::kCached>(map, source,
                                              &local_pretenuring_feedback_);
}

bool Scavenger::SemiSpaceCopyObject(Map* map, HeapObject** slot,
                                    HeapObject* object, int object_size) {
  DCHECK(heap()->AllowedToBeMigrated(object, NEW_SPACE));
  AllocationAlignment alignment = object->RequiredAlignment();
  AllocationResult allocation =
      allocator_.Allocate<NEW_SPACE>(object_size, alignment);

  HeapObject* target = nullptr;
  if (allocation.To(&target)) {
    DCHECK(ObjectMarking::IsWhite(
        target, heap()->mark_compact_collector()->marking_state(target)));
    MigrateObject(map, object, target, object_size);
    *slot = target;

    copied_list_.Insert(target, object_size);
    copied_size_ += object_size;
    return true;
  }
  return false;
}

bool Scavenger::PromoteObject(Map* map, HeapObject** slot, HeapObject* object,
                              int object_size) {
  AllocationAlignment alignment = object->RequiredAlignment();
  AllocationResult allocation =
      allocator_.Allocate<OLD_SPACE>(object_size, alignment);

  HeapObject* target = nullptr;
  if (allocation.To(&target)) {
    DCHECK(ObjectMarking::IsWhite(
        target, heap()->mark_compact_collector()->marking_state(target)));
    MigrateObject(map, object, target, object_size);
    *slot = target;

    if (!ContainsOnlyData(static_cast<VisitorId>(map->visitor_id()))) {
      promotion_list_.Push(ObjectAndSize(target, object_size));
    }
    promoted_size_ += object_size;
    return true;
  }
  return false;
}

void Scavenger::EvacuateObjectDefault(Map* map, HeapObject** slot,
                                      HeapObject* object, int object_size) {
  SLOW_DCHECK(object_size <= Page::kAllocatableMemory);
  SLOW_DCHECK(object->SizeFromMap(map) == object_size);

  if (!heap()->ShouldBePromoted(object->address())) {
    // A semi-space copy may fail due to fragmentation. In that case, we
    // try to promote the object.
    if (SemiSpaceCopyObject(map, slot, object, object_size)) {
      return;
    }
  }

  if (PromoteObject(map, slot, object, object_size)) {
    return;
  }

  // If promotion failed, we try to copy the object to the other semi-space
  if (SemiSpaceCopyObject(map, slot, object, object_size)) return;

  FatalProcessOutOfMemory("Scavenger: semi-space copy\n");
}

void Scavenger::EvacuateThinString(Map* map, HeapObject** slot,
                                   ThinString* object, int object_size) {
  if (!is_incremental_marking_) {
    HeapObject* actual = object->actual();
    *slot = actual;
    // ThinStrings always refer to internalized strings, which are
    // always in old space.
    DCHECK(!heap()->InNewSpace(actual));
    object->set_map_word(MapWord::FromForwardingAddress(actual));
    return;
  }

  EvacuateObjectDefault(map, slot, object, object_size);
}

void Scavenger::EvacuateShortcutCandidate(Map* map, HeapObject** slot,
                                          ConsString* object, int object_size) {
  DCHECK(IsShortcutCandidate(map->instance_type()));
  if (!is_incremental_marking_ &&
      object->unchecked_second() == heap()->empty_string()) {
    HeapObject* first = HeapObject::cast(object->unchecked_first());

    *slot = first;

    if (!heap()->InNewSpace(first)) {
      object->set_map_word(MapWord::FromForwardingAddress(first));
      return;
    }

    MapWord first_word = first->map_word();
    if (first_word.IsForwardingAddress()) {
      HeapObject* target = first_word.ToForwardingAddress();

      *slot = target;
      object->set_map_word(MapWord::FromForwardingAddress(target));
      return;
    }

    EvacuateObject(slot, first_word.ToMap(), first);
    object->set_map_word(MapWord::FromForwardingAddress(*slot));
    return;
  }

  EvacuateObjectDefault(map, slot, object, object_size);
}

void Scavenger::EvacuateObject(HeapObject** slot, Map* map,
                               HeapObject* source) {
  SLOW_DCHECK(heap_->InFromSpace(source));
  SLOW_DCHECK(!MapWord::FromMap(map).IsForwardingAddress());
  int size = source->SizeFromMap(map);
  switch (static_cast<VisitorId>(map->visitor_id())) {
    case kVisitThinString:
      EvacuateThinString(map, slot, ThinString::cast(source), size);
      break;
    case kVisitShortcutCandidate:
      EvacuateShortcutCandidate(map, slot, ConsString::cast(source), size);
      break;
    default:
      EvacuateObjectDefault(map, slot, source, size);
      break;
  }
}

void Scavenger::ScavengeObject(HeapObject** p, HeapObject* object) {
  DCHECK(heap()->InFromSpace(object));

  // We use the first word (where the map pointer usually is) of a heap
  // object to record the forwarding pointer.  A forwarding pointer can
  // point to an old space, the code space, or the to space of the new
  // generation.
  MapWord first_word = object->map_word();

  // If the first word is a forwarding address, the object has already been
  // copied.
  if (first_word.IsForwardingAddress()) {
    HeapObject* dest = first_word.ToForwardingAddress();
    DCHECK(object->GetIsolate()->heap()->InFromSpace(*p));
    *p = dest;
    return;
  }

  Map* map = first_word.ToMap();
  // AllocationMementos are unrooted and shouldn't survive a scavenge
  DCHECK_NE(heap()->allocation_memento_map(), map);
  // Call the slow part of scavenge object.
  EvacuateObject(p, map, object);
}

SlotCallbackResult Scavenger::CheckAndScavengeObject(Heap* heap,
                                                     Address slot_address) {
  Object** slot = reinterpret_cast<Object**>(slot_address);
  Object* object = *slot;
  if (heap->InFromSpace(object)) {
    HeapObject* heap_object = reinterpret_cast<HeapObject*>(object);
    DCHECK(heap_object->IsHeapObject());

    ScavengeObject(reinterpret_cast<HeapObject**>(slot), heap_object);

    object = *slot;
    // If the object was in from space before and is after executing the
    // callback in to space, the object is still live.
    // Unfortunately, we do not know about the slot. It could be in a
    // just freed free space object.
    if (heap->InToSpace(object)) {
      return KEEP_SLOT;
    }
  }
  // Slots can point to "to" space if the slot has been recorded multiple
  // times in the remembered set. We remove the redundant slot now.
  return REMOVE_SLOT;
}

void ScavengeVisitor::VisitPointers(HeapObject* host, Object** start,
                                    Object** end) {
  for (Object** p = start; p < end; p++) {
    Object* object = *p;
    if (!heap_->InNewSpace(object)) continue;
    scavenger_->ScavengeObject(reinterpret_cast<HeapObject**>(p),
                               reinterpret_cast<HeapObject*>(object));
  }
}

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_SCAVENGER_INL_H_
