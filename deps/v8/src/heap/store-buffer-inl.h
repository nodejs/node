// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_STORE_BUFFER_INL_H_
#define V8_STORE_BUFFER_INL_H_

#include "src/heap/heap.h"
#include "src/heap/remembered-set.h"
#include "src/heap/spaces-inl.h"
#include "src/heap/store-buffer.h"

namespace v8 {
namespace internal {

void LocalStoreBuffer::Record(Address addr) {
  if (top_->is_full()) top_ = new Node(top_);
  top_->buffer[top_->count++] = addr;
}

void LocalStoreBuffer::Process(StoreBuffer* store_buffer) {
  Node* current = top_;
  while (current != nullptr) {
    for (int i = 0; i < current->count; i++) {
      Address slot = current->buffer[i];
      Page* page = Page::FromAnyPointerAddress(heap_, slot);
      RememberedSet<OLD_TO_NEW>::Insert(page, slot);
    }
    current = current->next;
  }
}

}  // namespace internal
}  // namespace v8

#endif  // V8_STORE_BUFFER_INL_H_
