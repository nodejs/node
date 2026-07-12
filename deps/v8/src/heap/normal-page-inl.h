// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_NORMAL_PAGE_INL_H_
#define V8_HEAP_NORMAL_PAGE_INL_H_

#include "src/heap/normal-page.h"
// Include the non-inl header before the rest of the headers.

#include "src/heap/memory-chunk-inl.h"
#include "src/heap/paged-spaces.h"
#include "src/heap/spaces.h"

namespace v8::internal {

// static
NormalPage* NormalPage::FromAddress(Address addr) {
  return SbxCast<NormalPage>(MemoryChunk::FromAddress(addr)->Metadata());
}

// static
NormalPage* NormalPage::FromAddress(const Isolate* isolate, Address addr) {
  return SbxCast<NormalPage>(MemoryChunk::FromAddress(addr)->Metadata(isolate));
}

// static
NormalPage* NormalPage::FromHeapObject(Tagged<HeapObject> o) {
  return FromAddress(o.ptr());
}

// static
NormalPage* NormalPage::FromAllocationAreaAddress(Address address) {
  return NormalPage::FromAddress(address - kTaggedSize);
}

template <typename Callback>
void NormalPage::ForAllFreeListCategories(Callback callback) {
  for (int i = kFirstCategory; i < owner()->free_list()->number_of_categories();
       i++) {
    callback(categories_[i]);
  }
}

}  // namespace v8::internal

#endif  // V8_HEAP_NORMAL_PAGE_INL_H_
