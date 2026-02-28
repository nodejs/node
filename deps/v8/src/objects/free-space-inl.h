// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_FREE_SPACE_INL_H_
#define V8_OBJECTS_FREE_SPACE_INL_H_

#include "src/objects/free-space.h"
// Include the non-inl header before the rest of the headers.

#include "src/execution/isolate.h"
#include "src/heap/heap-write-barrier-inl.h"
#include "src/heap/heap.h"
#include "src/objects/objects-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

int FreeSpace::size(RelaxedLoadTag) const {
  return size_in_tagged_.Relaxed_Load().value() * kTaggedSize;
}

// static
inline void FreeSpace::SetSize(const WritableFreeSpace& writable_free_space,
                               int size, RelaxedStoreTag tag) {
  // For size <= 2 * kTaggedSize, we expect to use one/two pointer filler maps.
  DCHECK_GT(size, 2 * kTaggedSize);
  DCHECK_EQ(size % kTaggedSize, 0);
  writable_free_space
      .WriteHeaderSlot<Smi, offsetof(FreeSpace, size_in_tagged_)>(
          Smi::FromInt(size / kTaggedSize), tag);
}

int FreeSpace::Size() { return size(kRelaxedLoad); }

Tagged<FreeSpace> FreeSpace::next() const {
  DCHECK(IsValid(Isolate::Current()->heap()));
#ifdef V8_EXTERNAL_CODE_SPACE
  intptr_t diff_to_next{next_.Relaxed_Load().value()};
  if (diff_to_next == 0) {
    return {};
  }
  Address next_ptr = ptr() + diff_to_next * kObjectAlignment;
  return UncheckedCast<FreeSpace>(Tagged<Object>(next_ptr));
#else
  return next_.Relaxed_Load();
#endif  // V8_EXTERNAL_CODE_SPACE
}

void FreeSpace::SetNext(const Heap* heap,
                        const WritableFreeSpace& writable_free_space,
                        Tagged<FreeSpace> next) {
  DCHECK(IsValid(heap));

#ifdef V8_EXTERNAL_CODE_SPACE
  if (next.is_null()) {
    writable_free_space.WriteHeaderSlot<Smi, offsetof(FreeSpace, next_)>(
        Smi::zero(), kRelaxedStore);
    return;
  }
  intptr_t diff_to_next = next.ptr() - ptr();
  DCHECK(IsAligned(diff_to_next, kObjectAlignment));
  writable_free_space.WriteHeaderSlot<Smi, offsetof(FreeSpace, next_)>(
      Smi::FromIntptr(diff_to_next / kObjectAlignment), kRelaxedStore);
#else
  writable_free_space.WriteHeaderSlot<Object, offsetof(FreeSpace, next_)>(
      next, kRelaxedStore);
#endif  // V8_EXTERNAL_CODE_SPACE
}

bool FreeSpace::IsValid(const Heap* heap) const {
  return heap->IsFreeSpaceValid(this);
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_FREE_SPACE_INL_H_
