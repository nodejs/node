// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_BASE_STACK_H_
#define V8_HEAP_BASE_STACK_H_

#include "src/base/macros.h"

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
class V8_EXPORT_PRIVATE Stack final {
 public:
  explicit Stack(const void* stack_start = nullptr);

  // Sets the start of the stack.
  void SetStackStart(const void* stack_start);

  // Returns true if |slot| is part of the stack and false otherwise.
  bool IsOnStack(void* slot) const;

  // Word-aligned iteration of the stack. Callee-saved registers are pushed to
  // the stack before iterating pointers. Slot values are passed on to
  // `visitor`.
  void IteratePointers(StackVisitor* visitor) const;

  // Word-aligned iteration of the stack, starting at `stack_end`. Slot values
  // are passed on to `visitor`. This is intended to be used with verifiers that
  // only visit a subset of the stack of IteratePointers().
  //
  // **Ignores:**
  // - Callee-saved registers.
  // - SafeStack.
  void IteratePointersUnsafe(StackVisitor* visitor, uintptr_t stack_end) const;

  // Returns the start of the stack.
  const void* stack_start() const { return stack_start_; }

 private:
  const void* stack_start_;
};

}  // namespace heap::base

#endif  // V8_HEAP_BASE_STACK_H_
