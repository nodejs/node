// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_BASE_STACK_H_
#define V8_HEAP_BASE_STACK_H_

#include <vector>

#include "src/base/macros.h"
#include "src/base/platform/platform.h"

namespace heap::base {

class StackVisitor {
 public:
  virtual ~StackVisitor() = default;
  virtual void VisitPointer(const void* address) = 0;
};

// Abstraction over the stack. Supports handling of:
// - native stack;
// - ASAN/MSAN;
// - SafeStack: https://releases.llvm.org/10.0.0/tools/clang/docs/SafeStack.html
//
// Stacks grow down, so throughout this class "start" refers the highest
// address of the stack, and top/marker the lowest.
//
// TODO(chromium:1056170): Consider adding a component that keeps track
// of relevant GC stack regions where interesting pointers can be found.
class V8_EXPORT_PRIVATE Stack final {
 public:
  explicit Stack(const void* stack_start = nullptr)
      : stack_start_(stack_start) {}

  // Sets the start of the stack.
  void SetStackStart(const void* stack_start) { stack_start_ = stack_start; }

  // Returns true if |slot| is part of the stack and false otherwise.
  bool IsOnStack(const void* slot) const;

  // Word-aligned iteration of the stack, starting at the `stack_marker_`
  // and going to the stack start. Slot values are passed on to `visitor`.
  void IteratePointersUntilMarker(StackVisitor* visitor) const;

  void AddStackSegment(const void* start, const void* top);
  void ClearStackSegments();

  // Push callee-saved registers to the stack, set the stack marker to the
  // current stack top and invoke the callback.
  template <typename Callback>
  V8_INLINE void SetMarkerAndCallback(Callback callback) {
    SetMarkerAndCallbackHelper(static_cast<void*>(&callback),
                               &SetMarkerAndCallbackImpl<Callback>);
  }

  template <typename Callback>
  V8_INLINE void SetMarkerIfNeededAndCallback(Callback callback) {
    if (stack_marker_ == nullptr) {
      SetMarkerAndCallbackHelper(static_cast<void*>(&callback),
                                 &SetMarkerAndCallbackImpl<Callback>);
    } else {
      DCHECK(IsOnCurrentStack(stack_marker_));
      callback();
    }
  }

  using IterateStackCallback = void (*)(Stack*, void*, const void*);

  // This method combines SetMarkerAndCallback with IteratePointersUntilMarker.
  // Callee-saved registers are pushed to the stack and then a word-aligned
  // iteration of the stack is performed. Slot values are passed on to
  // `visitor`. To be used for testing.
  void IteratePointersForTesting(StackVisitor* visitor);

  bool IsMarkerSet() const { return stack_marker_ != nullptr; }

 private:
#ifdef DEBUG
  static bool IsOnCurrentStack(const void* ptr);
#endif

  static void IteratePointersImpl(const Stack* stack, void* argument,
                                  const void* stack_end);

  V8_NOINLINE void SetMarkerAndCallbackHelper(void* argument,
                                              IterateStackCallback callback);

  template <typename Callback>
  static void SetMarkerAndCallbackImpl(Stack* stack, void* argument,
                                       const void* stack_end) {
    DCHECK_NULL(stack->stack_marker_);
    stack->stack_marker_ = stack_end;
    Callback* callback = static_cast<Callback*>(argument);
    (*callback)();
    stack->stack_marker_ = nullptr;
  }

  const void* stack_start_;

  // Marker that signals end of the interesting stack region in which on-heap
  // pointers can be found.
  const void* stack_marker_ = nullptr;

  // Stack segments that may also contain pointers and should be scanned.
  struct StackSegments {
    const void* start;
    const void* top;
  };
  std::vector<StackSegments> inactive_stacks_;
};

}  // namespace heap::base

#endif  // V8_HEAP_BASE_STACK_H_
