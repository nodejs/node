// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_STORE_BUFFER_INL_H_
#define V8_HEAP_STORE_BUFFER_INL_H_

#include "src/heap/store-buffer.h"

#include "src/heap/heap-inl.h"

namespace v8 {
namespace internal {

void StoreBuffer::InsertDeletionIntoStoreBuffer(Address start, Address end) {
  if (top_ + sizeof(Address) * 2 > limit_[current_]) {
    StoreBufferOverflow(heap_->isolate());
  }
  *top_ = MarkDeletionAddress(start);
  top_++;
  *top_ = end;
  top_++;
}

void StoreBuffer::InsertIntoStoreBuffer(Address slot) {
  if (top_ + sizeof(Address) > limit_[current_]) {
    StoreBufferOverflow(heap_->isolate());
  }
  *top_ = slot;
  top_++;
}

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_STORE_BUFFER_INL_H_
