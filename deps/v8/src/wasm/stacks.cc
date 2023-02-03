// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/stacks.h"

namespace v8::internal::wasm {

// static
StackMemory* StackMemory::GetCurrentStackView(Isolate* isolate) {
  byte* limit = reinterpret_cast<byte*>(isolate->stack_guard()->real_jslimit());
  return new StackMemory(isolate, limit);
}

StackMemory::~StackMemory() {
  if (v8_flags.trace_wasm_stack_switching) {
    PrintF("Delete stack #%d\n", id_);
  }
  PageAllocator* allocator = GetPlatformPageAllocator();
  if (owned_) allocator->DecommitPages(limit_, size_);
  // We don't need to handle removing the last stack from the list (next_ ==
  // this). This only happens on isolate tear down, otherwise there is always
  // at least one reachable stack (the active stack).
  isolate_->wasm_stacks() = next_;
  prev_->next_ = next_;
  next_->prev_ = prev_;
}

void StackMemory::Add(StackMemory* stack) {
  stack->next_ = this->next_;
  stack->prev_ = this;
  this->next_->prev_ = stack;
  this->next_ = stack;
}

StackMemory::StackMemory(Isolate* isolate) : isolate_(isolate), owned_(true) {
  static std::atomic<int> next_id(1);
  id_ = next_id.fetch_add(1);
  PageAllocator* allocator = GetPlatformPageAllocator();
  int kJsStackSizeKB = v8_flags.wasm_stack_switching_stack_size;
  size_ = (kJsStackSizeKB + kJSLimitOffsetKB) * KB;
  size_ = RoundUp(size_, allocator->AllocatePageSize());
  limit_ = static_cast<byte*>(
      allocator->AllocatePages(nullptr, size_, allocator->AllocatePageSize(),
                               PageAllocator::kReadWrite));
  if (v8_flags.trace_wasm_stack_switching) {
    PrintF("Allocate stack #%d (limit: %p, base: %p)\n", id_, limit_,
           limit_ + size_);
  }
}

// Overload to represent a view of the libc stack.
StackMemory::StackMemory(Isolate* isolate, byte* limit)
    : isolate_(isolate),
      limit_(limit),
      size_(v8_flags.stack_size * KB),
      owned_(false) {
  id_ = 0;
}

}  // namespace v8::internal::wasm
