// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects/embedder-data-array.h"

#include "src/execution/isolate.h"
#include "src/objects/embedder-data-array-inl.h"

namespace v8 {
namespace internal {

#ifdef V8_COMPRESS_POINTERS
namespace {
CppHeapPointerHandle LoadCppHeapPointerHandle(const EmbedderDataSlot& slot) {
  Address loc = slot.address() + EmbedderDataSlot::kCppHeapPointerOffset;
  return CppHeapPointerSlot(loc).Relaxed_LoadHandle();
}
}  // namespace
#endif

// static
DirectHandle<EmbedderDataArray> EmbedderDataArray::EnsureCapacity(
    Isolate* isolate, DirectHandle<EmbedderDataArray> array, int index) {
  if (index < array->length()) return array;
  DCHECK_LT(index, kEmbedderDataArrayMaxLength);
  DirectHandle<EmbedderDataArray> new_array =
      isolate->factory()->NewEmbedderDataArray(index + 1);
  DisallowGarbageCollection no_gc;
  // Last new space allocation does not require any write barriers.
#ifdef V8_COMPRESS_POINTERS
  for (int i = 0; i < array->length(); i++) {
    EmbedderDataSlot src(*array, i);
    EmbedderDataSlot dest(*new_array, i);
    CppHeapPointerHandle src_handle = LoadCppHeapPointerHandle(src);
    if (src_handle != kNullCppHeapPointerHandle) {
      CHECK(dest.store_handle_without_barrier(isolate, src_handle));
    } else {
      dest.store_tagged_without_barrier(src.load_tagged());
    }
  }
#else
  size_t size = array->length() * kEmbedderDataSlotSize;
  MemCopy(reinterpret_cast<void*>(new_array->slots_start()),
          reinterpret_cast<void*>(array->slots_start()), size);
#endif  // V8_COMPRESS_POINTERS
  return new_array;
}

}  // namespace internal
}  // namespace v8
