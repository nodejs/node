// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/cppgc/stack.h"

#include <limits>

#include "src/base/platform/platform.h"
#include "src/heap/cppgc/globals.h"
#include "src/heap/cppgc/sanitizers.h"

namespace cppgc {
namespace internal {

using IterateStackCallback = void (Stack::*)(StackVisitor*, intptr_t*) const;
extern "C" void PushAllRegistersAndIterateStack(const Stack*, StackVisitor*,
                                                IterateStackCallback);

Stack::Stack(const void* stack_start) : stack_start_(stack_start) {}

bool Stack::IsOnStack(void* slot) const {
  void* raw_slot = v8::base::Stack::GetStackSlot(slot);
  return v8::base::Stack::GetCurrentStackPosition() <= raw_slot &&
         raw_slot <= stack_start_;
}

namespace {

#ifdef V8_USE_ADDRESS_SANITIZER

// No ASAN support as accessing fake frames otherwise results in
// "stack-use-after-scope" warnings.
NO_SANITIZE_ADDRESS
void IterateAsanFakeFrameIfNecessary(StackVisitor* visitor,
                                     void* asan_fake_stack,
                                     const void* stack_start,
                                     const void* stack_end, void* address) {
  // When using ASAN fake stack a pointer to the fake frame is kept on the
  // native frame. In case |addr| points to a fake frame of the current stack
  // iterate the fake frame. Frame layout see
  // https://github.com/google/sanitizers/wiki/AddressSanitizerUseAfterReturn
  if (asan_fake_stack) {
    void* fake_frame_begin;
    void* fake_frame_end;
    void* real_stack_frame = __asan_addr_is_in_fake_stack(
        asan_fake_stack, address, &fake_frame_begin, &fake_frame_end);
    if (real_stack_frame) {
      // |address| points to a fake frame. Check that the fake frame is part
      // of this stack.
      if (stack_start >= real_stack_frame && real_stack_frame >= stack_end) {
        // Iterate the fake frame.
        for (void** current = reinterpret_cast<void**>(fake_frame_begin);
             current < fake_frame_end; ++current) {
          void* addr = *current;
          if (addr == nullptr) continue;
          visitor->VisitPointer(addr);
        }
      }
    }
  }
}

#endif  // V8_USE_ADDRESS_SANITIZER

#ifdef CPPGC_SUPPORTS_CONSERVATIVE_STACK_SCAN

void IterateSafeStackIfNecessary(StackVisitor* visitor) {
#if defined(__has_feature)
#if __has_feature(safe_stack)
  // Source:
  // https://github.com/llvm/llvm-project/blob/master/compiler-rt/lib/safestack/safestack.cpp
  constexpr size_t kSafeStackAlignmentBytes = 16;
  void* stack_end = __builtin___get_unsafe_stack_ptr();
  void* stack_start = __builtin___get_unsafe_stack_top();
  CHECK_GT(stack_start, stack_end);
  CHECK_EQ(0u, reinterpret_cast<uintptr_t>(stack_end) &
                   (kSafeStackAlignmentBytes - 1));
  CHECK_EQ(0u, reinterpret_cast<uintptr_t>(stack_start) &
                   (kSafeStackAlignmentBytes - 1));
  void** current = reinterpret_cast<void**>(stack_end);
  for (; current < stack_start; ++current) {
    void* address = *current;
    if (address == nullptr) continue;
    visitor->VisitPointer(address);
  }
#endif  // __has_feature(safe_stack)
#endif  // defined(__has_feature)
}

#endif  // CPPGC_SUPPORTS_CONSERVATIVE_STACK_SCAN

}  // namespace

#ifdef CPPGC_SUPPORTS_CONSERVATIVE_STACK_SCAN
void Stack::IteratePointers(StackVisitor* visitor) const {
  PushAllRegistersAndIterateStack(this, visitor, &Stack::IteratePointersImpl);
  // No need to deal with callee-saved registers as they will be kept alive by
  // the regular conservative stack iteration.
  IterateSafeStackIfNecessary(visitor);
}
#endif  // CPPGC_SUPPORTS_CONSERVATIVE_STACK_SCAN

// No ASAN support as method accesses redzones while walking the stack.
NO_SANITIZE_ADDRESS
void Stack::IteratePointersImpl(StackVisitor* visitor,
                                intptr_t* stack_end) const {
#ifdef V8_USE_ADDRESS_SANITIZER
  void* asan_fake_stack = __asan_get_current_fake_stack();
#endif  // V8_USE_ADDRESS_SANITIZER
  // All supported platforms should have their stack aligned to at least
  // sizeof(void*).
  constexpr size_t kMinStackAlignment = sizeof(void*);
  // Redzone should not contain any pointers as the iteration is always called
  // from the assembly trampoline. If inline assembly is ever inlined through
  // LTO this may become necessary.
  constexpr size_t kRedZoneBytes = 128;
  void** current = reinterpret_cast<void**>(
      reinterpret_cast<uintptr_t>(stack_end - kRedZoneBytes));
  CHECK_EQ(0u, reinterpret_cast<uintptr_t>(current) & (kMinStackAlignment - 1));
  for (; current < stack_start_; ++current) {
    // MSAN: Instead of unpoisoning the whole stack, the slot's value is copied
    // into a local which is unpoisoned.
    void* address = *current;
    MSAN_UNPOISON(address, sizeof(address));
    if (address == nullptr) continue;
    visitor->VisitPointer(address);
#ifdef V8_USE_ADDRESS_SANITIZER
    IterateAsanFakeFrameIfNecessary(visitor, asan_fake_stack, stack_start_,
                                    stack_end, address);
#endif  // V8_USE_ADDRESS_SANITIZER
  }
}

}  // namespace internal
}  // namespace cppgc
