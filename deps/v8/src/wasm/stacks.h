// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_STACKS_H_
#define V8_WASM_STACKS_H_

#if !V8_ENABLE_WEBASSEMBLY
#error This header should only be included if WebAssembly is enabled.
#endif  // !V8_ENABLE_WEBASSEMBLY

#include "src/base/build_config.h"
#include "src/common/globals.h"
#include "src/execution/isolate.h"
#include "src/utils/allocation.h"

namespace v8 {
namespace internal {
namespace wasm {

struct JumpBuffer {
  Address sp;
  Address fp;
  void* stack_limit;
  // TODO(thibaudm/fgm): Add general-purpose registers.
};

constexpr int kJmpBufSpOffset = offsetof(JumpBuffer, sp);
constexpr int kJmpBufFpOffset = offsetof(JumpBuffer, fp);
constexpr int kJmpBufStackLimitOffset = offsetof(JumpBuffer, stack_limit);

class StackMemory {
 public:
  static StackMemory* New(Isolate* isolate) { return new StackMemory(isolate); }

  // Returns a non-owning view of the current stack.
  static StackMemory* GetCurrentStackView(Isolate* isolate) {
    byte* limit =
        reinterpret_cast<byte*>(isolate->stack_guard()->real_jslimit());
    return new StackMemory(isolate, limit);
  }

  ~StackMemory() {
    if (FLAG_trace_wasm_stack_switching) {
      PrintF("Delete stack (sp: %p)\n", reinterpret_cast<void*>(jmpbuf_.sp));
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

  void* jslimit() const { return limit_ + kJSLimitOffsetKB; }
  Address base() const { return reinterpret_cast<Address>(limit_ + size_); }
  JumpBuffer* jmpbuf() { return &jmpbuf_; }

  // Insert a stack in the linked list after this stack.
  void Add(StackMemory* stack) {
    stack->next_ = this->next_;
    stack->prev_ = this;
    this->next_->prev_ = stack;
    this->next_ = stack;
  }

  StackMemory* next() { return next_; }

  // Track external memory usage for Managed<StackMemory> objects.
  size_t owned_size() { return sizeof(StackMemory) + (owned_ ? size_ : 0); }
  bool IsActive() {
    byte* sp = reinterpret_cast<byte*>(GetCurrentStackPosition());
    return limit_ < sp && sp <= limit_ + size_;
  }

 private:
  static constexpr int kJSLimitOffsetKB = 40;

  // This constructor allocates a new stack segment.
  explicit StackMemory(Isolate* isolate) : isolate_(isolate), owned_(true) {
    PageAllocator* allocator = GetPlatformPageAllocator();
    int kJsStackSizeKB = 4;
    size_ = (kJsStackSizeKB + kJSLimitOffsetKB) * KB;
    size_ = RoundUp(size_, allocator->AllocatePageSize());
    limit_ = static_cast<byte*>(
        allocator->AllocatePages(nullptr, size_, allocator->AllocatePageSize(),
                                 PageAllocator::kReadWrite));
    if (FLAG_trace_wasm_stack_switching)
      PrintF("Allocate stack (sp: %p, limit: %p)\n", limit_ + size_, limit_);
  }

  // Overload to represent a view of the libc stack.
  StackMemory(Isolate* isolate, byte* limit)
      : isolate_(isolate),
        limit_(limit),
        size_(reinterpret_cast<size_t>(limit)),
        owned_(false) {}

  Isolate* isolate_;
  byte* limit_;
  size_t size_;
  bool owned_;
  JumpBuffer jmpbuf_;
  // Stacks form a circular doubly linked list per isolate.
  StackMemory* next_ = this;
  StackMemory* prev_ = this;
};

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_STACKS_H_
