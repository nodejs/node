// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXECUTION_INTERRUPTS_SCOPE_H_
#define V8_EXECUTION_INTERRUPTS_SCOPE_H_

#include "src/execution/stack-guard.h"

namespace v8 {
namespace internal {

class Isolate;

// Scope intercepts only interrupt which is part of its interrupt_mask and does
// not affect other interrupts.
class InterruptsScope {
 public:
  enum Mode { kPostponeInterrupts, kRunInterrupts, kNoop };

  V8_EXPORT_PRIVATE InterruptsScope(Isolate* isolate, int intercept_mask,
                                    Mode mode);

  virtual ~InterruptsScope() {
    if (mode_ != kNoop) stack_guard_->PopInterruptsScope();
  }

  // Find the scope that intercepts this interrupt.
  // It may be outermost PostponeInterruptsScope or innermost
  // SafeForInterruptsScope if any.
  // Return whether the interrupt has been intercepted.
  bool Intercept(StackGuard::InterruptFlag flag);

 private:
  StackGuard* stack_guard_;
  int intercept_mask_;
  int intercepted_flags_;
  Mode mode_;
  InterruptsScope* prev_;

  friend class StackGuard;
};

// Support for temporarily postponing interrupts. When the outermost
// postpone scope is left the interrupts will be re-enabled and any
// interrupts that occurred while in the scope will be taken into
// account.
class PostponeInterruptsScope : public InterruptsScope {
 public:
  PostponeInterruptsScope(Isolate* isolate,
                          int intercept_mask = StackGuard::ALL_INTERRUPTS)
      : InterruptsScope(isolate, intercept_mask,
                        InterruptsScope::kPostponeInterrupts) {}
  ~PostponeInterruptsScope() override = default;
};

// Support for overriding PostponeInterruptsScope. Interrupt is not ignored if
// innermost scope is SafeForInterruptsScope ignoring any outer
// PostponeInterruptsScopes.
class SafeForInterruptsScope : public InterruptsScope {
 public:
  SafeForInterruptsScope(Isolate* isolate,
                         int intercept_mask = StackGuard::ALL_INTERRUPTS)
      : InterruptsScope(isolate, intercept_mask,
                        InterruptsScope::kRunInterrupts) {}
  ~SafeForInterruptsScope() override = default;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_EXECUTION_INTERRUPTS_SCOPE_H_
