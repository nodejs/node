// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXECUTION_INTERRUPTS_SCOPE_H_
#define V8_EXECUTION_INTERRUPTS_SCOPE_H_

#include "src/execution/isolate.h"
#include "src/execution/stack-guard.h"

namespace v8 {
namespace internal {

class Isolate;

// Scope intercepts only interrupt which is part of its interrupt_mask and does
// not affect other interrupts.
class V8_NODISCARD InterruptsScope {
 public:
  enum Mode : uint8_t { kPostponeInterrupts, kRunInterrupts, kNoop };

  V8_EXPORT_PRIVATE InterruptsScope(Isolate* isolate, uint32_t intercept_mask,
                                    Mode mode)
      : stack_guard_(nullptr),
        intercept_mask_(intercept_mask),
        intercepted_flags_(0),
        mode_(mode) {
    if (mode_ != kNoop) {
      stack_guard_ = isolate->stack_guard();
      stack_guard_->PushInterruptsScope(this);
    }
  }

  ~InterruptsScope() {
    if (mode_ != kNoop) {
      stack_guard_->PopInterruptsScope();
    }
  }

  // Find the scope that intercepts this interrupt.
  // It may be outermost PostponeInterruptsScope or innermost
  // SafeForInterruptsScope if any.
  // Return whether the interrupt has been intercepted.
  bool Intercept(StackGuard::InterruptFlag flag);

 private:
  StackGuard* stack_guard_;
  InterruptsScope* prev_;
  const uint32_t intercept_mask_;
  uint32_t intercepted_flags_;
  const Mode mode_;

  friend class StackGuard;
};

// Support for temporarily postponing interrupts. When the outermost
// postpone scope is left the interrupts will be re-enabled and any
// interrupts that occurred while in the scope will be taken into
// account.
class V8_NODISCARD PostponeInterruptsScope : public InterruptsScope {
 public:
  explicit PostponeInterruptsScope(
      Isolate* isolate, uint32_t intercept_mask = StackGuard::ALL_INTERRUPTS)
      : InterruptsScope(isolate, intercept_mask,
                        InterruptsScope::kPostponeInterrupts) {}
};

// Support for overriding PostponeInterruptsScope. Interrupt is not ignored if
// innermost scope is SafeForInterruptsScope ignoring any outer
// PostponeInterruptsScopes.
class V8_NODISCARD SafeForInterruptsScope : public InterruptsScope {
 public:
  explicit SafeForInterruptsScope(
      Isolate* isolate, uint32_t intercept_mask = StackGuard::ALL_INTERRUPTS)
      : InterruptsScope(isolate, intercept_mask,
                        InterruptsScope::kRunInterrupts) {}
};

}  // namespace internal
}  // namespace v8

#endif  // V8_EXECUTION_INTERRUPTS_SCOPE_H_
