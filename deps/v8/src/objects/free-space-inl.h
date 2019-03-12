// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_FREE_SPACE_INL_H_
#define V8_OBJECTS_FREE_SPACE_INL_H_

#include "src/objects/free-space.h"

#include "src/heap/heap-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

OBJECT_CONSTRUCTORS_IMPL(FreeSpace, HeapObject)

SMI_ACCESSORS(FreeSpace, size, kSizeOffset)
RELAXED_SMI_ACCESSORS(FreeSpace, size, kSizeOffset)

int FreeSpace::Size() { return size(); }

FreeSpace FreeSpace::next() {
#ifdef DEBUG
  Heap* heap = Heap::FromWritableHeapObject(*this);
  Object free_space_map = heap->isolate()->root(RootIndex::kFreeSpaceMap);
  DCHECK_IMPLIES(!map_slot().contains_value(free_space_map->ptr()),
                 !heap->deserialization_complete() &&
                     map_slot().contains_value(kNullAddress));
#endif
  DCHECK_LE(kNextOffset + kPointerSize, relaxed_read_size());
  return FreeSpace::unchecked_cast(*ObjectSlot(address() + kNextOffset));
}

void FreeSpace::set_next(FreeSpace next) {
#ifdef DEBUG
  Heap* heap = Heap::FromWritableHeapObject(*this);
  Object free_space_map = heap->isolate()->root(RootIndex::kFreeSpaceMap);
  DCHECK_IMPLIES(!map_slot().contains_value(free_space_map->ptr()),
                 !heap->deserialization_complete() &&
                     map_slot().contains_value(kNullAddress));
#endif
  DCHECK_LE(kNextOffset + kPointerSize, relaxed_read_size());
  ObjectSlot(address() + kNextOffset).Relaxed_Store(next);
}

FreeSpace FreeSpace::cast(HeapObject o) {
  SLOW_DCHECK(!Heap::FromWritableHeapObject(o)->deserialization_complete() ||
              o->IsFreeSpace());
  return bit_cast<FreeSpace>(o);
}

FreeSpace FreeSpace::unchecked_cast(const Object o) {
  return bit_cast<FreeSpace>(o);
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_FREE_SPACE_INL_H_
