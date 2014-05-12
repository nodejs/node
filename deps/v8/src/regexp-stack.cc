// Copyright 2009 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "v8.h"
#include "regexp-stack.h"

namespace v8 {
namespace internal {

RegExpStackScope::RegExpStackScope(Isolate* isolate)
    : regexp_stack_(isolate->regexp_stack()) {
  // Initialize, if not already initialized.
  regexp_stack_->EnsureCapacity(0);
}


RegExpStackScope::~RegExpStackScope() {
  // Reset the buffer if it has grown.
  regexp_stack_->Reset();
}


RegExpStack::RegExpStack()
    : isolate_(NULL) {
}


RegExpStack::~RegExpStack() {
  thread_local_.Free();
}


char* RegExpStack::ArchiveStack(char* to) {
  size_t size = sizeof(thread_local_);
  OS::MemCopy(reinterpret_cast<void*>(to), &thread_local_, size);
  thread_local_ = ThreadLocal();
  return to + size;
}


char* RegExpStack::RestoreStack(char* from) {
  size_t size = sizeof(thread_local_);
  OS::MemCopy(&thread_local_, reinterpret_cast<void*>(from), size);
  return from + size;
}


void RegExpStack::Reset() {
  if (thread_local_.memory_size_ > kMinimumStackSize) {
    DeleteArray(thread_local_.memory_);
    thread_local_ = ThreadLocal();
  }
}


void RegExpStack::ThreadLocal::Free() {
  if (memory_size_ > 0) {
    DeleteArray(memory_);
    Clear();
  }
}


Address RegExpStack::EnsureCapacity(size_t size) {
  if (size > kMaximumStackSize) return NULL;
  if (size < kMinimumStackSize) size = kMinimumStackSize;
  if (thread_local_.memory_size_ < size) {
    Address new_memory = NewArray<byte>(static_cast<int>(size));
    if (thread_local_.memory_size_ > 0) {
      // Copy original memory into top of new memory.
      OS::MemCopy(
          reinterpret_cast<void*>(
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


}}  // namespace v8::internal
