// Copyright 2009 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/regexp/regexp-stack.h"

#include "src/execution/isolate.h"
#include "src/utils/memcopy.h"

namespace v8 {
namespace internal {

RegExpStackScope::RegExpStackScope(Isolate* isolate)
    : regexp_stack_(isolate->regexp_stack()) {
  // Initialize, if not already initialized.
  regexp_stack_->EnsureCapacity(0);
  // Irregexp is not reentrant in several ways; in particular, the
  // RegExpStackScope is not reentrant since the destructor frees allocated
  // memory. Protect against reentrancy here.
  CHECK(!regexp_stack_->is_in_use());
  regexp_stack_->set_is_in_use(true);
}


RegExpStackScope::~RegExpStackScope() {
  // Reset the buffer if it has grown.
  regexp_stack_->Reset();
  DCHECK(!regexp_stack_->is_in_use());
}

RegExpStack::RegExpStack() : thread_local_(this), isolate_(nullptr) {}

RegExpStack::~RegExpStack() { thread_local_.FreeAndInvalidate(); }

char* RegExpStack::ArchiveStack(char* to) {
  if (!thread_local_.owns_memory_) {
    // Force dynamic stacks prior to archiving. Any growth will do. A dynamic
    // stack is needed because stack archival & restoration rely on `memory_`
    // pointing at a fixed-location backing store, whereas the static stack is
    // tied to a RegExpStack instance.
    EnsureCapacity(thread_local_.memory_size_ + 1);
    DCHECK(thread_local_.owns_memory_);
  }

  MemCopy(reinterpret_cast<void*>(to), &thread_local_, kThreadLocalSize);
  thread_local_ = ThreadLocal(this);
  return to + kThreadLocalSize;
}


char* RegExpStack::RestoreStack(char* from) {
  MemCopy(&thread_local_, reinterpret_cast<void*>(from), kThreadLocalSize);
  return from + kThreadLocalSize;
}

void RegExpStack::Reset() { thread_local_.ResetToStaticStack(this); }

void RegExpStack::ThreadLocal::ResetToStaticStack(RegExpStack* regexp_stack) {
  if (owns_memory_) DeleteArray(memory_);

  memory_ = regexp_stack->static_stack_;
  memory_top_ = regexp_stack->static_stack_ + kStaticStackSize;
  memory_size_ = kStaticStackSize;
  limit_ = reinterpret_cast<Address>(regexp_stack->static_stack_) +
           kStackLimitSlack * kSystemPointerSize;
  owns_memory_ = false;
  is_in_use_ = false;
}

void RegExpStack::ThreadLocal::FreeAndInvalidate() {
  if (owns_memory_) DeleteArray(memory_);

  // This stack may not be used after being freed. Just reset to invalid values
  // to ensure we don't accidentally use old memory areas.
  memory_ = nullptr;
  memory_top_ = nullptr;
  memory_size_ = 0;
  limit_ = kMemoryTop;
}

Address RegExpStack::EnsureCapacity(size_t size) {
  if (size > kMaximumStackSize) return kNullAddress;
  if (size < kMinimumDynamicStackSize) size = kMinimumDynamicStackSize;
  if (thread_local_.memory_size_ < size) {
    byte* new_memory = NewArray<byte>(size);
    if (thread_local_.memory_size_ > 0) {
      // Copy original memory into top of new memory.
      MemCopy(new_memory + size - thread_local_.memory_size_,
              thread_local_.memory_, thread_local_.memory_size_);
      if (thread_local_.owns_memory_) DeleteArray(thread_local_.memory_);
    }
    thread_local_.memory_ = new_memory;
    thread_local_.memory_top_ = new_memory + size;
    thread_local_.memory_size_ = size;
    thread_local_.limit_ = reinterpret_cast<Address>(new_memory) +
                           kStackLimitSlack * kSystemPointerSize;
    thread_local_.owns_memory_ = true;
  }
  return reinterpret_cast<Address>(thread_local_.memory_top_);
}


}  // namespace internal
}  // namespace v8
