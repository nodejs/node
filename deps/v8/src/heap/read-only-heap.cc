// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/read-only-heap.h"

#include "src/heap/spaces.h"

namespace v8 {
namespace internal {

// static
ReadOnlyHeap* ReadOnlyHeap::GetOrCreateReadOnlyHeap(Heap* heap) {
  return new ReadOnlyHeap(new ReadOnlySpace(heap));
}

void ReadOnlyHeap::MaybeDeserialize(Isolate* isolate,
                                    ReadOnlyDeserializer* des) {
  des->DeserializeInto(isolate);
}

void ReadOnlyHeap::OnHeapTearDown() {
  delete read_only_space_;
  delete this;
}

}  // namespace internal
}  // namespace v8
