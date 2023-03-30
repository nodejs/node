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
  explicit Stack(const void* stack_start = nullptr,
                 bool wasm_stack_switching = false)
      : stack_start_(stack_start),
        wasm_stack_switching_(wasm_stack_switching) {}

  // Sets the start of the stack.
  void SetStackStart(const void* stack_start, bool wasm_stack_switching) {
    stack_start_ = stack_start;
    wasm_stack_switching_ = wasm_stack_switching;
  }

  // Returns true if |slot| is part of the stack and false otherwise.
  bool IsOnStack(const void* slot) const;

  // Word-aligned iteration of the stack. Callee-saved registers are pushed to
  // the stack before iterating pointers. Slot values are passed on to
  // `visitor`.
  void IteratePointers(StackVisitor* visitor) const;

  // Word-aligned iteration of the stack, starting at the `stack_marker_`. Slot
  // values are passed on to `visitor`. This is intended to be used with
  // verifiers that only visit a subset of the stack of IteratePointers().
  //
  // **Ignores:**
  // - Callee-saved registers.
  // - SafeStack.
  void IteratePointersUntilMarker(StackVisitor* visitor) const;

  void AddStackSegment(const void* start, const void* top);
  void ClearStackSegments();

  // This method should be inlined, to set the marker at the current frame's
  // stack top.
  V8_INLINE void SetMarkerToCurrentStackPosition() {
    stack_marker_ = v8::base::Stack::GetCurrentStackPosition();
  }

 private:
#ifdef DEBUG
  static bool IsOnCurrentStack(const void* ptr);
#endif

  static void IteratePointersImpl(const Stack* stack, StackVisitor* visitor,
                                  const void* stack_end);

  const void* stack_start_;

  // Marker that signals end of the interesting stack region in which on-heap
  // pointers can be found.
  const void* stack_marker_;

  // TODO(v8:13493): This is for suppressing the check that we are in the
  // correct stack, in the case of  WASM stack switching. It will be removed as
  // soon as context saving becomes compatible with stack switching.
  bool wasm_stack_switching_;

  // Stack segments that may also contain pointers and should be scanned.
  struct StackSegments {
    const void* start;
    const void* top;
  };
  std::vector<StackSegments> inactive_stacks_;
};

}  // namespace heap::base

#endif  // V8_HEAP_BASE_STACK_H_
