// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/store-buffer.h"

#include <algorithm>

#include "src/counters.h"
#include "src/heap/incremental-marking.h"
#include "src/isolate.h"
#include "src/objects-inl.h"
#include "src/v8.h"

namespace v8 {
namespace internal {

StoreBuffer::StoreBuffer(Heap* heap)
    : heap_(heap),
      top_(nullptr),
      start_(nullptr),
      limit_(nullptr),
      virtual_memory_(nullptr) {}

void StoreBuffer::SetUp() {
  // Allocate 3x the buffer size, so that we can start the new store buffer
  // aligned to 2x the size.  This lets us use a bit test to detect the end of
  // the area.
  virtual_memory_ = new base::VirtualMemory(kStoreBufferSize * 2);
  uintptr_t start_as_int =
      reinterpret_cast<uintptr_t>(virtual_memory_->address());
  start_ = reinterpret_cast<Address*>(RoundUp(start_as_int, kStoreBufferSize));
  limit_ = start_ + (kStoreBufferSize / kPointerSize);

  DCHECK(reinterpret_cast<Address>(start_) >= virtual_memory_->address());
  DCHECK(reinterpret_cast<Address>(limit_) >= virtual_memory_->address());
  Address* vm_limit = reinterpret_cast<Address*>(
      reinterpret_cast<char*>(virtual_memory_->address()) +
      virtual_memory_->size());
  DCHECK(start_ <= vm_limit);
  DCHECK(limit_ <= vm_limit);
  USE(vm_limit);
  DCHECK((reinterpret_cast<uintptr_t>(limit_) & kStoreBufferMask) == 0);

  if (!virtual_memory_->Commit(reinterpret_cast<Address>(start_),
                               kStoreBufferSize,
                               false)) {  // Not executable.
    V8::FatalProcessOutOfMemory("StoreBuffer::SetUp");
  }
  top_ = start_;
}


void StoreBuffer::TearDown() {
  delete virtual_memory_;
  top_ = start_ = limit_ = nullptr;
}


void StoreBuffer::StoreBufferOverflow(Isolate* isolate) {
  isolate->heap()->store_buffer()->MoveEntriesToRememberedSet();
  isolate->counters()->store_buffer_overflows()->Increment();
}

void StoreBuffer::MoveEntriesToRememberedSet() {
  if (top_ == start_) return;
  DCHECK(top_ <= limit_);
  for (Address* current = start_; current < top_; current++) {
    DCHECK(!heap_->code_space()->Contains(*current));
    Address addr = *current;
    Page* page = Page::FromAnyPointerAddress(heap_, addr);
    RememberedSet<OLD_TO_NEW>::Insert(page, addr);
  }
  top_ = start_;
}

}  // namespace internal
}  // namespace v8
