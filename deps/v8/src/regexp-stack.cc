// Copyright 2009 the V8 project authors. All rights reserved.
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

#include "v8.h"
#include "top.h"
#include "regexp-stack.h"

namespace v8 { namespace internal {

RegExpStack::RegExpStack() {
  // Initialize, if not already initialized.
  RegExpStack::EnsureCapacity(0);
}


RegExpStack::~RegExpStack() {
  // Reset the buffer if it has grown.
  RegExpStack::Reset();
}


char* RegExpStack::ArchiveStack(char* to) {
  size_t size = sizeof(thread_local_);
  memcpy(reinterpret_cast<void*>(to),
         &thread_local_,
         size);
  thread_local_ = ThreadLocal();
  return to + size;
}


char* RegExpStack::RestoreStack(char* from) {
  size_t size = sizeof(thread_local_);
  memcpy(&thread_local_, reinterpret_cast<void*>(from), size);
  return from + size;
}


void RegExpStack::Reset() {
  if (thread_local_.memory_size_ > kMinimumStackSize) {
    DeleteArray(thread_local_.memory_);
    thread_local_ = ThreadLocal();
  }
}


Address RegExpStack::EnsureCapacity(size_t size) {
  if (size > kMaximumStackSize) return NULL;
  if (size < kMinimumStackSize) size = kMinimumStackSize;
  if (thread_local_.memory_size_ < size) {
    Address new_memory = NewArray<byte>(size);
    if (thread_local_.memory_size_ > 0) {
      // Copy original memory into top of new memory.
      memcpy(reinterpret_cast<void*>(
          new_memory + size - thread_local_.memory_size_),
             reinterpret_cast<void*>(thread_local_.memory_),
             thread_local_.memory_size_);
      DeleteArray(thread_local_.memory_);
    }
    thread_local_.memory_ = new_memory;
    thread_local_.memory_size_ = size;
    thread_local_.limit_ = new_memory + kStackLimitSlack * kPointerSize;
  }
  return thread_local_.memory_ + thread_local_.memory_size_;
}


RegExpStack::ThreadLocal RegExpStack::thread_local_;

}}  // namespace v8::internal
