// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/base/stack.h"

#include <limits>

#include "src/base/sanitizer/asan.h"
#include "src/base/sanitizer/msan.h"
#include "src/base/sanitizer/tsan.h"

namespace heap::base {

Stack::Stack(const void* stack_start) : stack_start_(stack_start) {}

void Stack::SetStackStart(const void* stack_start) {
  DCHECK(!context_);
  stack_start_ = stack_start;
}

bool Stack::IsOnStack(const void* slot) const {
  DCHECK_NOT_NULL(stack_start_);
#ifdef V8_USE_ADDRESS_SANITIZER
  // If the slot is part of a fake frame, then it is definitely on the stack.
  if (__asan_addr_is_in_fake_stack(__asan_get_current_fake_stack(),
                                   const_cast<void*>(slot), nullptr, nullptr)) {
    return true;
  }
  // Fall through as there is still a regular stack present even when running
  // with ASAN fake stacks.
#endif  // V8_USE_ADDRESS_SANITIZER
#if defined(__has_feature)
#if __has_feature(safe_stack)
  if (__builtin___get_unsafe_stack_top() >= slot &&
      slot >= __builtin___get_unsafe_stack_ptr()) {
    return true;
  }
#endif  // __has_feature(safe_stack)
#endif  // defined(__has_feature)
  return v8::base::Stack::GetCurrentStackPosition() <= slot &&
         slot <= stack_start_;
}

namespace {

#ifdef V8_USE_ADDRESS_SANITIZER

// No ASAN support as accessing fake frames otherwise results in
// "stack-use-after-scope" warnings.
DISABLE_ASAN
// No TSAN support as the stack may not be exclusively owned by the current
// thread, e.g., for interrupt handling. Atomic reads are not enough as the
// other thread may use a lock to synchronize the access.
DISABLE_TSAN
void IterateAsanFakeFrameIfNecessary(StackVisitor* visitor,
                                     const void* asan_fake_stack,
                                     const void* stack_start,
                                     const void* stack_end,
                                     const void* address) {
  // When using ASAN fake stack a pointer to the fake frame is kept on the
  // native frame. In case |addr| points to a fake frame of the current stack
  // iterate the fake frame. Frame layout see
  // https://github.com/google/sanitizers/wiki/AddressSanitizerUseAfterReturn
  if (!asan_fake_stack) return;
  void* fake_frame_begin;
  void* fake_frame_end;
  void* real_stack_frame = __asan_addr_is_in_fake_stack(
      const_cast<void*>(asan_fake_stack), const_cast<void*>(address),
      &fake_frame_begin, &fake_frame_end);
  if (real_stack_frame) {
    // |address| points to a fake frame. Check that the fake frame is part
    // of this stack.
    if (stack_start >= real_stack_frame && real_stack_frame >= stack_end) {
      // Iterate the fake frame.
      for (const void* const* current =
               reinterpret_cast<const void* const*>(fake_frame_begin);
           current < fake_frame_end; ++current) {
        const void* address = *current;
        if (address == nullptr) continue;
        visitor->VisitPointer(address);
      }
    }
  }
}
#else
void IterateAsanFakeFrameIfNecessary(StackVisitor* visitor,
                                     const void* asan_fake_stack,
                                     const void* stack_start,
                                     const void* stack_end,
                                     const void* address) {}
#endif  // V8_USE_ADDRESS_SANITIZER

void IterateUnsafeStackIfNecessary(StackVisitor* visitor) {
#if defined(__has_feature)
#if __has_feature(safe_stack)
  // Source:
  // https://github.com/llvm/llvm-project/blob/main/compiler-rt/lib/safestack/safestack.cpp
  constexpr size_t kSafeStackAlignmentBytes = 16;
  const void* stack_end = __builtin___get_unsafe_stack_ptr();
  const void* stack_start = __builtin___get_unsafe_stack_top();
  CHECK_GT(stack_start, stack_end);
  CHECK_EQ(0u, reinterpret_cast<uintptr_t>(stack_end) &
                   (kSafeStackAlignmentBytes - 1));
  CHECK_EQ(0u, reinterpret_cast<uintptr_t>(stack_start) &
                   (kSafeStackAlignmentBytes - 1));
  for (const void* const* current =
           reinterpret_cast<const void* const*>(stack_end);
       current < stack_start; ++current) {
    const void* address = *current;
    if (address == nullptr) continue;
    visitor->VisitPointer(address);
  }
#endif  // __has_feature(safe_stack)
#endif  // defined(__has_feature)
}

// This method should never be inlined to ensure that a possible redzone cannot
// contain any data that needs to be scanned.
V8_NOINLINE
// No ASAN support as method accesses redzones while walking the stack.
DISABLE_ASAN
// No TSAN support as the stack may not be exclusively owned by the current
// thread, e.g., for interrupt handling. Atomic reads are not enough as the
// other thread may use a lock to synchronize the access.
DISABLE_TSAN
void IteratePointersInStack(StackVisitor* visitor, const void* top,
                            const void* start, const void* asan_fake_stack) {
  for (const void* const* current = reinterpret_cast<const void* const*>(top);
       current < start; ++current) {
    // MSAN: Instead of unpoisoning the whole stack, the slot's value is copied
    // into a local which is unpoisoned.
    const void* address = *current;
    MSAN_MEMORY_IS_INITIALIZED(&address, sizeof(address));
    if (address == nullptr) continue;
    visitor->VisitPointer(address);
    IterateAsanFakeFrameIfNecessary(visitor, asan_fake_stack, start, top,
                                    address);
  }
}

}  // namespace

void Stack::IteratePointers(StackVisitor* visitor) const {
  DCHECK_NOT_NULL(stack_start_);
  DCHECK(context_);
  DCHECK_NOT_NULL(context_->stack_marker);

#ifdef V8_USE_ADDRESS_SANITIZER
  const void* asan_fake_stack = __asan_get_current_fake_stack();
#else
  const void* asan_fake_stack = nullptr;
#endif  // V8_USE_ADDRESS_SANITIZER

  // Iterate through the registers.
  for (intptr_t value : context_->registers) {
    const void* address = reinterpret_cast<const void*>(value);
    MSAN_MEMORY_IS_INITIALIZED(&address, sizeof(address));
    if (address == nullptr) continue;
    visitor->VisitPointer(address);
    IterateAsanFakeFrameIfNecessary(visitor, asan_fake_stack, stack_start_,
                                    context_->stack_marker, address);
  }

  // Iterate through the stack.
  // All supported platforms should have their stack aligned to at least
  // sizeof(void*).
  constexpr size_t kMinStackAlignment = sizeof(void*);
  CHECK_EQ(0u, reinterpret_cast<uintptr_t>(context_->stack_marker) &
                   (kMinStackAlignment - 1));
  IteratePointersInStack(
      visitor, reinterpret_cast<const void* const*>(context_->stack_marker),
      stack_start_, asan_fake_stack);

  for (const auto& stack : inactive_stacks_) {
    IteratePointersInStack(visitor, stack.top, stack.start, asan_fake_stack);
  }

  IterateUnsafeStackIfNecessary(visitor);
}

namespace {
// Function with architecture-specific implementation:
// Saves all callee-saved registers in the specified buffer.
extern "C" void SaveCalleeSavedRegisters(intptr_t* buffer);

#ifdef DEBUG

bool IsOnCurrentStack(const void* ptr) {
  DCHECK_NOT_NULL(ptr);
  const void* current_stack_start = v8::base::Stack::GetStackStart();
  const void* current_stack_top = v8::base::Stack::GetCurrentStackPosition();
  return ptr <= current_stack_start && ptr >= current_stack_top;
}

#endif  // DEBUG

}  // namespace

void Stack::SaveContext(bool check_invariant) {
  // TODO(v8:13493): Remove the method's parameter and the implication as soon
  // as IsOnCurrentStack is compatible with stack switching.
  DCHECK_IMPLIES(check_invariant, IsOnCurrentStack(stack_start_));
  // Contexts can be nested but the marker and the registers are only saved on
  // the first invocation.
  if (context_) {
    ++context_->nesting_counter;
    return;
  }
  // Allocate the context and set the marker.
  const void* stack_top = v8::base::Stack::GetCurrentStackPosition();
  DCHECK_NOT_NULL(stack_top);
  context_ = std::make_unique<Context>(stack_top);
  // TODO(v8:13493): Remove the implication as soon as IsValidMarker is
  // compatible with stack switching.
  DCHECK_IMPLIES(check_invariant, stack_top <= stack_start_);
  context_->stack_marker = stack_top;
  // Save the registers.
  SaveCalleeSavedRegisters(context_->registers.data());
}

void Stack::ClearContext(bool check_invariant) {
  // TODO(v8:13493): Remove the method's parameter and the implication as soon
  // as IsOnCurrentStack is compatible with stack switching.
  DCHECK_IMPLIES(check_invariant, IsOnCurrentStack(stack_start_));
  DCHECK(context_);
  // Skip clearing the context if that was a nested invocation.
  if (context_->nesting_counter > 0) {
    --context_->nesting_counter;
    return;
  }
  context_.reset();
}

void Stack::AddStackSegment(const void* start, const void* top) {
  DCHECK_LE(top, start);
  inactive_stacks_.push_back({start, top});
}

void Stack::ClearStackSegments() { inactive_stacks_.clear(); }

}  // namespace heap::base
