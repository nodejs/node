// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_HEAP_LAYOUT_INL_H_
#define V8_HEAP_HEAP_LAYOUT_INL_H_

#include "src/heap/heap-layout.h"
// Include the non-inl header before the rest of the headers.

#include "src/flags/flags.h"
#include "src/heap/memory-chunk-inl.h"
#include "src/objects/casting.h"
#include "src/objects/objects.h"
#include "src/objects/tagged-impl-inl.h"

namespace v8::internal {

// static
bool HeapLayout::InReadOnlySpace(Tagged<HeapObject> object) {
#if CONTIGUOUS_COMPRESSED_READ_ONLY_SPACE_BOOL
#ifdef DEBUG
  // See `kContiguousReadOnlySpaceMask` definition for explanation.
  const bool ro_via_address =
      (object.ptr() & kContiguousReadOnlySpaceMask) == 0;
  DCHECK_EQ(ro_via_address,
            MemoryChunk::FromHeapObject(object)->InReadOnlySpace());
#endif  // DEBUG
#endif  // CONTIGUOUS_COMPRESSED_READ_ONLY_SPACE_BOOL
  return MemoryChunk::FromHeapObject(object)->InReadOnlySpace();
}

// static
bool HeapLayout::InYoungGeneration(const MemoryChunk* chunk,
                                   Tagged<HeapObject> object) {
  if constexpr (v8_flags.single_generation.value()) {
    return false;
  }
  if constexpr (v8_flags.sticky_mark_bits.value()) {
    return InYoungGenerationForStickyMarkbits(chunk, object);
  }
  const bool in_young_generation = chunk->InYoungGeneration();
#ifdef DEBUG
  if (in_young_generation) {
    CheckYoungGenerationConsistency(chunk);
  }
#endif  // DEBUG
  return in_young_generation;
}

// static
bool HeapLayout::InYoungGeneration(Tagged<Object> object) {
  if (object.IsSmi()) {
    return false;
  }
  return InYoungGeneration(Cast<HeapObject>(object));
}

// static
bool HeapLayout::InYoungGeneration(Tagged<MaybeObject> object) {
  Tagged<HeapObject> heap_object;
  return object.GetHeapObject(&heap_object) && InYoungGeneration(heap_object);
}

// static
bool HeapLayout::InYoungGeneration(Tagged<HeapObject> object) {
  return InYoungGeneration(MemoryChunk::FromHeapObject(object), object);
}

// static
bool HeapLayout::InYoungGeneration(const HeapObjectLayout* object) {
  return InYoungGeneration(Tagged<HeapObject>(object));
}

// static
bool HeapLayout::InWritableSharedSpace(Tagged<HeapObject> object) {
  return MemoryChunk::FromHeapObject(object)->InWritableSharedSpace();
}

// static
bool HeapLayout::InAnySharedSpace(Tagged<HeapObject> object) {
  if (HeapLayout::InReadOnlySpace(object)) {
    return true;
  }
  return HeapLayout::InWritableSharedSpace(object);
}

bool HeapLayout::InBlackAllocatedPage(Tagged<HeapObject> object) {
  DCHECK(v8_flags.black_allocated_pages);
  return MemoryChunk::FromHeapObject(object)->GetFlags() &
         MemoryChunk::BLACK_ALLOCATED;
}

// static
bool HeapLayout::InAnyLargeSpace(Tagged<HeapObject> object) {
  return MemoryChunk::FromHeapObject(object)->IsLargePage();
}

// static
bool TrustedHeapLayout::InCodeSpace(Tagged<HeapObject> object) {
  return MemoryChunk::FromHeapObject(object)
      ->MetadataNoIsolateCheck()
      ->is_executable();
}

// static
bool TrustedHeapLayout::InTrustedSpace(Tagged<HeapObject> object) {
  return MemoryChunk::FromHeapObject(object)
      ->MetadataNoIsolateCheck()
      ->is_trusted();
}

// static
bool TrustedHeapLayout::IsOwnedByAnyHeap(Tagged<HeapObject> object) {
  return MemoryChunk::FromHeapObject(object)->Metadata()->heap();
}

}  // namespace v8::internal

#endif  // V8_HEAP_HEAP_LAYOUT_INL_H_
