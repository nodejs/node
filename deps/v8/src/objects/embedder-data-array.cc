// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects/embedder-data-array.h"

#include "src/isolate.h"
#include "src/objects/embedder-data-array-inl.h"

namespace v8 {
namespace internal {

// static
Handle<EmbedderDataArray> EmbedderDataArray::EnsureCapacity(
    Isolate* isolate, Handle<EmbedderDataArray> array, int index) {
  if (index < array->length()) return array;
  DCHECK_LT(index, kMaxLength);
  Handle<EmbedderDataArray> new_array =
      isolate->factory()->NewEmbedderDataArray(index + 1);
  DisallowHeapAllocation no_gc;
  // Last new space allocation does not require any write barriers.
  size_t size = array->length() * kEmbedderDataSlotSize;
  MemCopy(reinterpret_cast<void*>(new_array->slots_start()),
          reinterpret_cast<void*>(array->slots_start()), size);
  return new_array;
}

}  // namespace internal
}  // namespace v8
