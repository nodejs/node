// Copyright 2011 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef V8_STORE_BUFFER_INL_H_
#define V8_STORE_BUFFER_INL_H_

#include "store-buffer.h"

namespace v8 {
namespace internal {

Address StoreBuffer::TopAddress() {
  return reinterpret_cast<Address>(heap_->store_buffer_top_address());
}


void StoreBuffer::Mark(Address addr) {
  ASSERT(!heap_->cell_space()->Contains(addr));
  ASSERT(!heap_->code_space()->Contains(addr));
  Address* top = reinterpret_cast<Address*>(heap_->store_buffer_top());
  *top++ = addr;
  heap_->public_set_store_buffer_top(top);
  if ((reinterpret_cast<uintptr_t>(top) & kStoreBufferOverflowBit) != 0) {
    ASSERT(top == limit_);
    Compact();
  } else {
    ASSERT(top < limit_);
  }
}


void StoreBuffer::EnterDirectlyIntoStoreBuffer(Address addr) {
  if (store_buffer_rebuilding_enabled_) {
    SLOW_ASSERT(!heap_->cell_space()->Contains(addr) &&
                !heap_->code_space()->Contains(addr) &&
                !heap_->old_data_space()->Contains(addr) &&
                !heap_->new_space()->Contains(addr));
    Address* top = old_top_;
    *top++ = addr;
    old_top_ = top;
    old_buffer_is_sorted_ = false;
    old_buffer_is_filtered_ = false;
    if (top >= old_limit_) {
      ASSERT(callback_ != NULL);
      (*callback_)(heap_,
                   MemoryChunk::FromAnyPointerAddress(addr),
                   kStoreBufferFullEvent);
    }
  }
}


} }  // namespace v8::internal

#endif  // V8_STORE_BUFFER_INL_H_
