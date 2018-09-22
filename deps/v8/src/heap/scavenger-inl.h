// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_SCAVENGER_INL_H_
#define V8_HEAP_SCAVENGER_INL_H_

#include "src/heap/scavenger.h"

#include "src/heap/incremental-marking-inl.h"
#include "src/heap/local-allocator-inl.h"
#include "src/objects-inl.h"
#include "src/objects/map.h"

namespace v8 {
namespace internal {

// White list for objects that for sure only contain data.
bool Scavenger::ContainsOnlyData(VisitorId visitor_id) {
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

void Scavenger::PageMemoryFence(MaybeObject* object) {
#ifdef THREAD_SANITIZER
  // Perform a dummy acquire load to tell TSAN that there is no data race
  // with  page initialization.
  HeapObject* heap_object;
  if (object->GetHeapObject(&heap_object)) {
    MemoryChunk* chunk = MemoryChunk::FromAddress(heap_object->address());
    CHECK_NOT_NULL(chunk->synchronized_heap());
  }
#endif
}

bool Scavenger::MigrateObject(Map* map, HeapObject* source, HeapObject* target,
                              int size) {
  // Copy the content of source to target.
  target->set_map_word(MapWord::FromMap(map));
  heap()->CopyBlock(target->address() + kPointerSize,
                    source->address() + kPointerSize, size - kPointerSize);

  HeapObject* old = base::AsAtomicPointer::Release_CompareAndSwap(
      reinterpret_cast<HeapObject**>(source->address()), map,
      MapWord::FromForwardingAddress(target).ToMap());
  if (old != map) {
    // Other task migrated the object.
    return false;
  }

  if (V8_UNLIKELY(is_logging_)) {
    heap()->OnMoveEvent(target, source, size);
  }

  if (is_incremental_marking_) {
    heap()->incremental_marking()->TransferColor(source, target);
  }
  heap()->UpdateAllocationSite(map, source, &local_pretenuring_feedback_);
  return true;
}

CopyAndForwardResult Scavenger::SemiSpaceCopyObject(Map* map,
                                                    HeapObjectReference** slot,
                                                    HeapObject* object,
                                                    int object_size) {
  DCHECK(heap()->AllowedToBeMigrated(object, NEW_SPACE));
  AllocationAlignment alignment = HeapObject::RequiredAlignment(map);
  AllocationResult allocation =
      allocator_.Allocate(NEW_SPACE, object_size, alignment);

  HeapObject* target = nullptr;
  if (allocation.To(&target)) {
    DCHECK(heap()->incremental_marking()->non_atomic_marking_state()->IsWhite(
        target));
    const bool self_success = MigrateObject(map, object, target, object_size);
    if (!self_success) {
      allocator_.FreeLast(NEW_SPACE, target, object_size);
      MapWord map_word = object->synchronized_map_word();
      HeapObjectReference::Update(slot, map_word.ToForwardingAddress());
      DCHECK(!Heap::InFromSpace(*slot));
      return Heap::InToSpace(*slot)
                 ? CopyAndForwardResult::SUCCESS_YOUNG_GENERATION
                 : CopyAndForwardResult::SUCCESS_OLD_GENERATION;
    }
    HeapObjectReference::Update(slot, target);

    copied_list_.Push(ObjectAndSize(target, object_size));
    copied_size_ += object_size;
    return CopyAndForwardResult::SUCCESS_YOUNG_GENERATION;
  }
  return CopyAndForwardResult::FAILURE;
}

CopyAndForwardResult Scavenger::PromoteObject(Map* map,
                                              HeapObjectReference** slot,
                                              HeapObject* object,
                                              int object_size) {
  AllocationAlignment alignment = HeapObject::RequiredAlignment(map);
  AllocationResult allocation =
      allocator_.Allocate(OLD_SPACE, object_size, alignment);

  HeapObject* target = nullptr;
  if (allocation.To(&target)) {
    DCHECK(heap()->incremental_marking()->non_atomic_marking_state()->IsWhite(
        target));
    const bool self_success = MigrateObject(map, object, target, object_size);
    if (!self_success) {
      allocator_.FreeLast(OLD_SPACE, target, object_size);
      MapWord map_word = object->synchronized_map_word();
      HeapObjectReference::Update(slot, map_word.ToForwardingAddress());
      DCHECK(!Heap::InFromSpace(*slot));
      return Heap::InToSpace(*slot)
                 ? CopyAndForwardResult::SUCCESS_YOUNG_GENERATION
                 : CopyAndForwardResult::SUCCESS_OLD_GENERATION;
    }
    HeapObjectReference::Update(slot, target);
    if (!ContainsOnlyData(map->visitor_id())) {
      promotion_list_.Push(ObjectAndSize(target, object_size));
    }
    promoted_size_ += object_size;
    return CopyAndForwardResult::SUCCESS_OLD_GENERATION;
  }
  return CopyAndForwardResult::FAILURE;
}

SlotCallbackResult Scavenger::RememberedSetEntryNeeded(
    CopyAndForwardResult result) {
  DCHECK_NE(CopyAndForwardResult::FAILURE, result);
  return result == CopyAndForwardResult::SUCCESS_YOUNG_GENERATION ? KEEP_SLOT
                                                                  : REMOVE_SLOT;
}

SlotCallbackResult Scavenger::EvacuateObjectDefault(Map* map,
                                                    HeapObjectReference** slot,
                                                    HeapObject* object,
                                                    int object_size) {
  SLOW_DCHECK(object_size <= Page::kAllocatableMemory);
  SLOW_DCHECK(object->SizeFromMap(map) == object_size);
  CopyAndForwardResult result;

  if (!heap()->ShouldBePromoted(object->address())) {
    // A semi-space copy may fail due to fragmentation. In that case, we
    // try to promote the object.
    result = SemiSpaceCopyObject(map, slot, object, object_size);
    if (result != CopyAndForwardResult::FAILURE) {
      return RememberedSetEntryNeeded(result);
    }
  }

  // We may want to promote this object if the object was already semi-space
  // copied in a previes young generation GC or if the semi-space copy above
  // failed.
  result = PromoteObject(map, slot, object, object_size);
  if (result != CopyAndForwardResult::FAILURE) {
    return RememberedSetEntryNeeded(result);
  }

  // If promotion failed, we try to copy the object to the other semi-space.
  result = SemiSpaceCopyObject(map, slot, object, object_size);
  if (result != CopyAndForwardResult::FAILURE) {
    return RememberedSetEntryNeeded(result);
  }

  heap()->FatalProcessOutOfMemory("Scavenger: semi-space copy");
  UNREACHABLE();
}

SlotCallbackResult Scavenger::EvacuateThinString(Map* map, HeapObject** slot,
                                                 ThinString* object,
                                                 int object_size) {
  if (!is_incremental_marking_) {
    // Loading actual is fine in a parallel setting since there is no write.
    String* actual = object->actual();
    object->set_length(0);
    *slot = actual;
    // ThinStrings always refer to internalized strings, which are
    // always in old space.
    DCHECK(!Heap::InNewSpace(actual));
    base::AsAtomicPointer::Release_Store(
        reinterpret_cast<Map**>(object->address()),
        MapWord::FromForwardingAddress(actual).ToMap());
    return REMOVE_SLOT;
  }

  return EvacuateObjectDefault(
      map, reinterpret_cast<HeapObjectReference**>(slot), object, object_size);
}

SlotCallbackResult Scavenger::EvacuateShortcutCandidate(Map* map,
                                                        HeapObject** slot,
                                                        ConsString* object,
                                                        int object_size) {
  DCHECK(IsShortcutCandidate(map->instance_type()));
  if (!is_incremental_marking_ &&
      object->unchecked_second() == ReadOnlyRoots(heap()).empty_string()) {
    HeapObject* first = HeapObject::cast(object->unchecked_first());

    *slot = first;

    if (!Heap::InNewSpace(first)) {
      base::AsAtomicPointer::Release_Store(
          reinterpret_cast<Map**>(object->address()),
          MapWord::FromForwardingAddress(first).ToMap());
      return REMOVE_SLOT;
    }

    MapWord first_word = first->synchronized_map_word();
    if (first_word.IsForwardingAddress()) {
      HeapObject* target = first_word.ToForwardingAddress();

      *slot = target;
      base::AsAtomicPointer::Release_Store(
          reinterpret_cast<Map**>(object->address()),
          MapWord::FromForwardingAddress(target).ToMap());
      return Heap::InToSpace(target) ? KEEP_SLOT : REMOVE_SLOT;
    }
    Map* map = first_word.ToMap();
    SlotCallbackResult result = EvacuateObjectDefault(
        map, reinterpret_cast<HeapObjectReference**>(slot), first,
        first->SizeFromMap(map));
    base::AsAtomicPointer::Release_Store(
        reinterpret_cast<Map**>(object->address()),
        MapWord::FromForwardingAddress(*slot).ToMap());
    return result;
  }

  return EvacuateObjectDefault(
      map, reinterpret_cast<HeapObjectReference**>(slot), object, object_size);
}

SlotCallbackResult Scavenger::EvacuateObject(HeapObjectReference** slot,
                                             Map* map, HeapObject* source) {
  SLOW_DCHECK(Heap::InFromSpace(source));
  SLOW_DCHECK(!MapWord::FromMap(map).IsForwardingAddress());
  int size = source->SizeFromMap(map);
  // Cannot use ::cast() below because that would add checks in debug mode
  // that require re-reading the map.
  switch (map->visitor_id()) {
    case kVisitThinString:
      // At the moment we don't allow weak pointers to thin strings.
      DCHECK(!(*slot)->IsWeak());
      return EvacuateThinString(map, reinterpret_cast<HeapObject**>(slot),
                                reinterpret_cast<ThinString*>(source), size);
    case kVisitShortcutCandidate:
      DCHECK(!(*slot)->IsWeak());
      // At the moment we don't allow weak pointers to cons strings.
      return EvacuateShortcutCandidate(
          map, reinterpret_cast<HeapObject**>(slot),
          reinterpret_cast<ConsString*>(source), size);
    default:
      return EvacuateObjectDefault(map, slot, source, size);
  }
}

SlotCallbackResult Scavenger::ScavengeObject(HeapObjectReference** p,
                                             HeapObject* object) {
  DCHECK(Heap::InFromSpace(object));

  // Synchronized load that consumes the publishing CAS of MigrateObject.
  MapWord first_word = object->synchronized_map_word();

  // If the first word is a forwarding address, the object has already been
  // copied.
  if (first_word.IsForwardingAddress()) {
    HeapObject* dest = first_word.ToForwardingAddress();
    DCHECK(Heap::InFromSpace(*p));
    if ((*p)->IsWeak()) {
      *p = HeapObjectReference::Weak(dest);
    } else {
      DCHECK((*p)->IsStrong());
      *p = HeapObjectReference::Strong(dest);
    }
    DCHECK(Heap::InToSpace(dest) || !Heap::InNewSpace((dest)));
    return Heap::InToSpace(dest) ? KEEP_SLOT : REMOVE_SLOT;
  }

  Map* map = first_word.ToMap();
  // AllocationMementos are unrooted and shouldn't survive a scavenge
  DCHECK_NE(ReadOnlyRoots(heap()).allocation_memento_map(), map);
  // Call the slow part of scavenge object.
  return EvacuateObject(p, map, object);
}

SlotCallbackResult Scavenger::CheckAndScavengeObject(Heap* heap,
                                                     Address slot_address) {
  MaybeObject** slot = reinterpret_cast<MaybeObject**>(slot_address);
  MaybeObject* object = *slot;
  if (Heap::InFromSpace(object)) {
    HeapObject* heap_object = object->GetHeapObject();
    DCHECK(heap_object->IsHeapObject());

    SlotCallbackResult result = ScavengeObject(
        reinterpret_cast<HeapObjectReference**>(slot), heap_object);
    DCHECK_IMPLIES(result == REMOVE_SLOT, !Heap::InNewSpace(*slot));
    return result;
  } else if (Heap::InToSpace(object)) {
    // Already updated slot. This can happen when processing of the work list
    // is interleaved with processing roots.
    return KEEP_SLOT;
  }
  // Slots can point to "to" space if the slot has been recorded multiple
  // times in the remembered set. We remove the redundant slot now.
  return REMOVE_SLOT;
}

void ScavengeVisitor::VisitPointers(HeapObject* host, Object** start,
                                    Object** end) {
  for (Object** p = start; p < end; p++) {
    Object* object = *p;
    if (!Heap::InNewSpace(object)) continue;
    scavenger_->ScavengeObject(reinterpret_cast<HeapObjectReference**>(p),
                               reinterpret_cast<HeapObject*>(object));
  }
}

void ScavengeVisitor::VisitPointers(HeapObject* host, MaybeObject** start,
                                    MaybeObject** end) {
  for (MaybeObject** p = start; p < end; p++) {
    MaybeObject* object = *p;
    if (!Heap::InNewSpace(object)) continue;
    // Treat the weak reference as strong.
    HeapObject* heap_object;
    if (object->GetHeapObject(&heap_object)) {
      scavenger_->ScavengeObject(reinterpret_cast<HeapObjectReference**>(p),
                                 heap_object);
    } else {
      UNREACHABLE();
    }
  }
}

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_SCAVENGER_INL_H_
