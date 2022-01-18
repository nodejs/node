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
  void* sp;
  void* stack_limit;
  // TODO(thibaudm/fgm): Add general-purpose registers.
};

constexpr int kJmpBufSpOffset = offsetof(JumpBuffer, sp);
constexpr int kJmpBufStackLimitOffset = offsetof(JumpBuffer, stack_limit);

class StackMemory {
 public:
  static StackMemory* New() { return new StackMemory(); }

  // Returns a non-owning view of the current stack.
  static StackMemory* GetCurrentStackView(Isolate* isolate) {
    byte* limit =
        reinterpret_cast<byte*>(isolate->stack_guard()->real_jslimit());
    return new StackMemory(limit);
  }

  ~StackMemory() {
    PageAllocator* allocator = GetPlatformPageAllocator();
    if (owned_) allocator->DecommitPages(limit_, size_);
  }

  void* limit() { return limit_; }
  void* base() { return limit_ + size_; }

  // Track external memory usage for Managed<StackMemory> objects.
  size_t owned_size() { return sizeof(StackMemory) + (owned_ ? size_ : 0); }

 private:
  // This constructor allocates a new stack segment.
  StackMemory() : owned_(true) {
    PageAllocator* allocator = GetPlatformPageAllocator();
    size_ = allocator->AllocatePageSize();
    // TODO(thibaudm): Leave space for runtime functions.
    limit_ = static_cast<byte*>(allocator->AllocatePages(
        nullptr, size_, size_, PageAllocator::kReadWrite));
  }

  // Overload to represent a view of the libc stack.
  explicit StackMemory(byte* limit) : limit_(limit), size_(0), owned_(false) {}

  byte* limit_;
  size_t size_;
  bool owned_;
};

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_STACKS_H_
