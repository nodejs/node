// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The common functionality when building with internal or external natives.

#include "src/heap/heap.h"
#include "src/objects-inl.h"
#include "src/snapshot/natives.h"

namespace v8 {
namespace internal {

template <>
FixedArray* NativesCollection<CORE>::GetSourceCache(Heap* heap) {
  return heap->natives_source_cache();
}


template <>
FixedArray* NativesCollection<EXPERIMENTAL>::GetSourceCache(Heap* heap) {
  return heap->experimental_natives_source_cache();
}


template <>
FixedArray* NativesCollection<EXTRAS>::GetSourceCache(Heap* heap) {
  return heap->extra_natives_source_cache();
}


template <>
FixedArray* NativesCollection<EXPERIMENTAL_EXTRAS>::GetSourceCache(Heap* heap) {
  return heap->experimental_extra_natives_source_cache();
}


template <NativeType type>
void NativesCollection<type>::UpdateSourceCache(Heap* heap) {
  for (int i = 0; i < GetBuiltinsCount(); i++) {
    Object* source = GetSourceCache(heap)->get(i);
    if (!source->IsUndefined()) {
      ExternalOneByteString::cast(source)->update_data_cache();
    }
  }
}


// Explicit template instantiations.
template void NativesCollection<CORE>::UpdateSourceCache(Heap* heap);
template void NativesCollection<EXPERIMENTAL>::UpdateSourceCache(Heap* heap);
template void NativesCollection<EXTRAS>::UpdateSourceCache(Heap* heap);
template void NativesCollection<EXPERIMENTAL_EXTRAS>::UpdateSourceCache(
    Heap* heap);

}  // namespace internal
}  // namespace v8
