// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_FREE_SPACE_INL_H_
#define V8_OBJECTS_FREE_SPACE_INL_H_

#include "src/execution/isolate.h"
#include "src/heap/heap-write-barrier-inl.h"
#include "src/heap/heap.h"
#include "src/objects/free-space.h"
#include "src/objects/objects-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

#include "torque-generated/src/objects/free-space-tq-inl.inc"

TQ_OBJECT_CONSTRUCTORS_IMPL(FreeSpace)

RELAXED_SMI_ACCESSORS(FreeSpace, size, kSizeOffset)

int FreeSpace::Size() { return size(kRelaxedLoad); }

FreeSpace FreeSpace::next() const {
  DCHECK(IsValid());
#ifdef V8_EXTERNAL_CODE_SPACE
  intptr_t diff_to_next =
      static_cast<intptr_t>(TaggedField<Smi, kNextOffset>::load(*this).value());
  if (diff_to_next == 0) {
    return FreeSpace();
  }
  Address next_ptr = ptr() + diff_to_next * kObjectAlignment;
  return FreeSpace::unchecked_cast(Object(next_ptr));
#else
  return FreeSpace::unchecked_cast(
      TaggedField<Object, kNextOffset>::load(*this));
#endif  // V8_EXTERNAL_CODE_SPACE
}

void FreeSpace::set_next(FreeSpace next) {
  DCHECK(IsValid());
#ifdef V8_EXTERNAL_CODE_SPACE
  if (next.is_null()) {
    TaggedField<Smi, kNextOffset>::Relaxed_Store(*this, Smi::zero());
    return;
  }
  intptr_t diff_to_next = next.ptr() - ptr();
  DCHECK(IsAligned(diff_to_next, kObjectAlignment));
  TaggedField<Smi, kNextOffset>::Relaxed_Store(
      *this, Smi::FromIntptr(diff_to_next / kObjectAlignment));
#else
  TaggedField<Object, kNextOffset>::Relaxed_Store(*this, next);
#endif  // V8_EXTERNAL_CODE_SPACE
}

FreeSpace FreeSpace::cast(HeapObject o) {
  SLOW_DCHECK((!GetHeapFromWritableObject(o)->deserialization_complete()) ||
              o.IsFreeSpace());
  return base::bit_cast<FreeSpace>(o);
}

FreeSpace FreeSpace::unchecked_cast(const Object o) {
  return base::bit_cast<FreeSpace>(o);
}

bool FreeSpace::IsValid() const {
  Heap* heap = GetHeapFromWritableObject(*this);
  Object free_space_map =
      Isolate::FromHeap(heap)->root(RootIndex::kFreeSpaceMap);
  CHECK(!heap->deserialization_complete() ||
        map_slot().contains_map_value(free_space_map.ptr()));
  CHECK_LE(kNextOffset + kTaggedSize, size(kRelaxedLoad));
  return true;
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_FREE_SPACE_INL_H_
